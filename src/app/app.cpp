#include "nodes/node2D.hpp"
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>
#include <chrono>
#include <cstdint>
#include <cstring>

#define APP_HPP_IMPLEMENTATION
#include "app.hpp"

#include "utils/vulkan_utils.hpp"
#include "renderer/renderer.hpp"
#include "nodes/node.hpp"
#include "nodes/sprite.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

void Application::run() {
  std::cout << (enableValidationLayers ? "validationLayers enabled" : "validationLayers off") << std::endl;

	initWindow();
	initVulkan();
  initScene();
	mainLoop();
	cleanup();
}

void Application::initWindow() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
  window = SDL_CreateWindow("Vulkan", WIDTH, HEIGHT, flags);
  if (!window) {
    throw std::runtime_error(std::string("SDL_CreateWindow failed") + SDL_GetError());
  }

  glm::vec3 dir;
  dir.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
  dir.y = sin(glm::radians(cameraPitch));
  dir.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
  cameraFront = glm::normalize(dir);

  SDL_SetWindowRelativeMouseMode(window, true);
}


std::vector<const char*> Application::getRequiredInstanceExtensions() {
  uint32_t sdlExtensionCount = 0;
  auto sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

  std::vector extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
  if (enableValidationLayers) {
    extensions.push_back(vk::EXTDebugUtilsExtensionName);
  }

  return extensions;
}

void Application::initVulkan() {
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createCommandPool();

  createSwapChain();
  createImageViews();
  createDepthResources();

  createCommandBuffers();
  createSyncObjects();

  createCameraUBOs();
  

}

void Application::createInstance() {
  constexpr vk::ApplicationInfo appInfo{
    .pApplicationName   = "Hello Triangle",
    .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
    .pEngineName        = "No Engine",
    .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
    .apiVersion         = vk::ApiVersion14,
  };

  // Get the required layers
  std::vector<char const*> requiredLayers;
  if (enableValidationLayers) {
    requiredLayers.assign(validationLayers.begin(), validationLayers.end());
  }

  // Check if the requiredLayers are supportedby the Vulkan implementation
  auto layerProperties = context.enumerateInstanceLayerProperties();
  auto unsupportedLayerIt = 
    std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer) {
      return std::ranges::none_of(layerProperties,
        [requiredLayer](auto const &layerProperty) {
          return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
    });
  if (unsupportedLayerIt != requiredLayers.end()) {
    throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
  }

  // Get the required extensions
  auto requiredExtensions = getRequiredInstanceExtensions();

  // Check if the required extensions are supported by the Vulkan implementation
  auto extensionProperties = context.enumerateInstanceExtensionProperties();
  auto unsupportedPropertyIt =
    std::ranges::find_if(requiredExtensions, [&extensionProperties](auto const &requiredExtension) {
      return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
        return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
      });
    });

  // find_if passes through vector, returning end if no unsupported properties are found
  if (unsupportedPropertyIt != requiredExtensions.end()) {
    throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
  }

  vk::InstanceCreateInfo createInfo{
    .pApplicationInfo        = &appInfo,
    .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
    .ppEnabledLayerNames     = requiredLayers.data(),
    .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
    .ppEnabledExtensionNames = requiredExtensions.data(),
  };

  instance = vk::raii::Instance(context, createInfo);
}

void Application::createSurface() {
  VkSurfaceKHR _surface;
  if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &_surface) != 0) {
    throw std::runtime_error("failed to create window surface!");
  }
  surface = vk::raii::SurfaceKHR(instance, _surface);
}

void Application::pickPhysicalDevice() {
  auto physicalDevices = instance.enumeratePhysicalDevices();
  if (physicalDevices.empty()) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }

  // Use an ordered map to automatically sort candidates by increasing score
  std::multimap<int, vk::raii::PhysicalDevice> candidates;

  for (const auto& pd : physicalDevices) {
    auto deviceProperties = pd.getProperties();
    auto deviceFeatures = pd.getFeatures();
    // std::cout << deviceProperties.deviceName << std::endl;
    uint32_t score = 0;

    switch (deviceProperties.deviceType) {
      // Discrete GPUs have a significant performance advantage
      case vk::PhysicalDeviceType::eDiscreteGpu:
        score += 1000;
        break;
      // Integrated GPUs are better than Virtaul ones
      case vk::PhysicalDeviceType::eIntegratedGpu:
        score += 100;
        break;
      // Better than nothing
      case vk::PhysicalDeviceType::eVirtualGpu:
        score += 10;
        break;
      // Nothing
      case vk::PhysicalDeviceType::eCpu:
        score += 1;
        break;
      default:
        break;
    }

    // Maximum possible size of texture affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    // Check if any of the queue families support graphics operations
    bool supportsVulkan1_3 = pd.getProperties().apiVersion >= vk::ApiVersion13;

    // Check if all required physicalDevice extensions are available
    auto queueFamilies    = pd.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {
      return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
    });

    auto availableDeviceExtensions = pd.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions =
      std::ranges::all_of(requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
        return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](auto const &availableDeviceExtension) {
          return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
        });
      });

    auto features                 = pd.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                             vk::PhysicalDeviceVulkan11Features,
                                                             vk::PhysicalDeviceVulkan12Features,
                                                             vk::PhysicalDeviceVulkan13Features,
                                                             vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                                    features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                    features.template get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingVariableDescriptorCount &&
                                    features.template get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingSampledImageUpdateAfterBind &&
                                    features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                    features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
                                    features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

    if (!(
      deviceFeatures.geometryShader && // Application can't function without geometry shaders
      supportsVulkan1_3             &&
      supportsGraphics              &&
      supportsAllRequiredExtensions &&
      supportsRequiredFeatures      
    )) {continue;}

    candidates.insert(std::make_pair(score, pd));
  }

  // Check if the best candidate is suitable at all
  if (!candidates.empty() && candidates.rbegin()->first > 0) {
    physicalDevice = candidates.rbegin()->second;
    std::cout << "Selected device: " << physicalDevice.getProperties().deviceName << std::endl;
  } else {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

void Application::createLogicalDevice() {
  // find the index of the first queue family that supports graphics
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
      physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
    {
      // found a queue family that supports both graphics and present
      queueIndex = qfpIndex;
      break;
    }
  }
  if (queueIndex == ~0) {
    throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
  }

  // get the first index into queueFamilyProperties which supports graphics
  auto graphicsQueueFamilyProperty = 
    std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
      return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
    });
  assert(graphicsQueueFamilyProperty != queueFamilyProperties.end() && "No graphics queue family found!");

  // Create a chain of feature structures
  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan12Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
    featureChain = {
      {.features = { .samplerAnisotropy = true }},              // vk::PhysicalDeviceFeatures2
      {.shaderDrawParameters = true},                           // Enable shader draw parameters from Vulkan 1.1
      { // Vulkan 1.2 features
       .descriptorBindingSampledImageUpdateAfterBind = true,    // Enable
       .descriptorBindingVariableDescriptorCount = true,        // Enab
       .runtimeDescriptorArray = true,                          // Enable runtime descriptor arrays from Vulkan 1.2
      },
      {.synchronization2 = true, .dynamicRendering     = true}, // Enable dynamic rendering from Vulkan 1.3
      {.extendedDynamicState = true}                            // Enable extended dynamic state from the extension
  };

  float queuePriority = 0.5f;
  vk::DeviceQueueCreateInfo deviceQueueCreateInfo {
    .queueFamilyIndex = queueIndex,
    .queueCount       = 1,
    .pQueuePriorities = &queuePriority,
  };

  vk::DeviceCreateInfo deviceCreateInfo {
    .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
    .queueCreateInfoCount    = 1,
    .pQueueCreateInfos       = &deviceQueueCreateInfo,
    .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
    .ppEnabledExtensionNames = requiredDeviceExtension.data(),
  };

  device = vk::raii::Device(physicalDevice, deviceCreateInfo);
  queue = vk::raii::Queue(device, queueIndex, 0);
}

void Application::createSwapChain() {
  vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( *surface );
  swapChainExtent                               = chooseSwapExtent(surfaceCapabilities);
  uint32_t minImageCount                        = chooseSwapMinImageCount(surfaceCapabilities);

  std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
  swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

  std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
  vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

  vk::SwapchainCreateInfoKHR swapChainCreateInfo{
    .surface          = *surface,
    .minImageCount    = minImageCount,
    .imageFormat      = swapChainSurfaceFormat.format,
    .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
    .imageExtent      = swapChainExtent,
    .imageArrayLayers = 1,
    .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
    .imageSharingMode = vk::SharingMode::eExclusive,
    .preTransform     = surfaceCapabilities.currentTransform,
    .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
    .presentMode      = presentMode,
    .clipped          = true,
  };

  swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
  swapChainImages = swapChain.getImages();
}

void Application::createImageViews() {
  assert(swapChainImageViews.empty());

  swapChainImageViews.reserve(swapChainImages.size());
  for ( auto &image: swapChainImages ) {
    swapChainImageViews.emplace_back(
      vk_util::createImageView(
        device,
        image,
        swapChainSurfaceFormat.format,
        vk::ImageAspectFlagBits::eColor
      ));
  }
}

void Application::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo {
    .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = queueIndex
  };
  commandPool = vk::raii::CommandPool(device, poolInfo);
}

void Application::createDepthResources() {
  vk::Format depthFormat = vk_util::findDepthFormat(device, physicalDevice);

  std::tie(depthImage, depthImageMemory) = vk_util::createImage(
    device,
    physicalDevice,
    swapChainExtent.width,
    swapChainExtent.height,
    depthFormat,
    vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eDepthStencilAttachment,
    vk::MemoryPropertyFlagBits::eDeviceLocal
  );
  depthImageView = vk_util::createImageView(device, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void Application::createCameraUBOs() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(CameraUBO);
    auto [buffer, bufferMem] = vk_util::createBuffer(
      device,
      physicalDevice,
      bufferSize, 
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );
    cameraUBOs.emplace_back(std::move(buffer));
    cameraUBOsMemory.emplace_back(std::move(bufferMem));
    cameraUBOsMapped.emplace_back(cameraUBOsMemory.back().mapMemory(0, bufferSize));
  }
}

void Application::createCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo {
    .commandPool = commandPool,
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT
  };
  commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void Application::createSyncObjects() {
  assert(
    presentCompleteSemaphores.empty() &&
    renderFinishedSemaphores.empty()  &&
    inFlightFences.empty()
  );

  for (size_t i = 0; i < swapChainImages.size(); i++) {
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}

void Application::updateCameraUBOBuffer(uint32_t currentImage) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float>(currentTime - startTime).count();

  CameraUBO ubo{};
  ubo.view  = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
  ubo.proj  =
    glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 100.0f);
  ubo.proj[1][1] *= -1;

  memcpy(cameraUBOsMapped[currentImage], &ubo, sizeof(ubo));
}

void Application::drawFrame() {
  auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }

  auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapChain();
    return;
  }

  if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
    throw std::runtime_error("failed to aquire swap chain image!");
  }
  updateCameraUBOBuffer(frameIndex);

  // Only reset the fence if we are submitting work
  device.resetFences(*inFlightFences[frameIndex]);

  commandBuffers[frameIndex].reset();
  
  // recordCommandBuffer(imageIndex);
  currentScene->recordFrame(
    commandBuffers[frameIndex],
    swapChainImages[imageIndex],
    swapChainExtent,
    swapChainImageViews[imageIndex],
    depthImage,
    depthImageView,
    frameIndex,
    imageIndex
  );

  vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
  const vk::SubmitInfo submitInfo {
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
    .pWaitDstStageMask    = &waitDestinationStageMask,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &*commandBuffers[frameIndex],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]
  };
  queue.submit(submitInfo, *inFlightFences[frameIndex]);

  const vk::PresentInfoKHR presentInfoKHR {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
    .swapchainCount     = 1,
    .pSwapchains        = &*swapChain,
    .pImageIndices      = &imageIndex
  };
  result = queue.presentKHR(presentInfoKHR);
  if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized) {
    framebufferResized = false;
    recreateSwapChain();
  } else {
    // There are no other success codes other then eSuccess; on any error code, presentKHR already threw an exception
    assert(result == vk::Result::eSuccess);
  }

  frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t Application::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities) {
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

vk::SurfaceFormatKHR Application::chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats) {
  assert(!availableFormats.empty());
  const auto formatIt = std::ranges::find_if(availableFormats, [](const auto &format) {
    return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
  });
  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR Application::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes) {
  // Fails if eFifo presentMode can't be found, should be guaranteed.
  assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
    return presentMode == vk::PresentModeKHR::eFifo;
  }));
  // Sets mode to eMailbox if its found in list of availablePresentModes
  return std::ranges::any_of(availablePresentModes, [](auto presentMode) {
    return presentMode == vk::PresentModeKHR::eMailbox;
  }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

vk::Extent2D Application::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  int width, height;
  SDL_GetWindowSizeInPixels(window, &width, &height);

  return {
    std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
    std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
  };
}

void Application::setupDebugMessenger() {
  if (!enableValidationLayers) return;

  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
  );
  vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | 
    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
  );

  vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT{
    .messageSeverity = severityFlags,
    .messageType     = messageTypeFlags,
    .pfnUserCallback = &debugCallback,
  };

  debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
}

void Application::initScene() {
  Sprite root = Sprite(
    {},
    glm::vec3(0.0f),
    glm::vec2(2.0f),
    glm::vec1(0.0f),
    "assets/textures/circle.png",
    "Root Sprite"
  );
  Sprite* child = new Sprite(
    {},
    glm::vec3(1.0f, 0.0f, 1.0f),
    glm::vec2(1.0f),
    glm::vec1(2.0f),
    "assets/textures/circle.png",
    "Child Sprite"
  );
  child->children.push_back(std::unique_ptr<Node> ( new Sprite (
      std::vector<std::unique_ptr<Node>> {},
      glm::vec3(1.0f, 0.0f, 1.0f),
      glm::vec2(1.0f),
      glm::vec1(1.0f),
      "assets/textures/circle.png",
      "Child Child Sprite"
  )));
  root.children.push_back(std::unique_ptr<Node>(child));
  // root.children.push_back(std::unique_ptr<Node>( new Sprite(
  //   std::vector<std::unique_ptr<Node>> {},
  //   glm::vec3(1.0f, 0.0f, 1.0f),
  //   glm::vec2(1.0f),
  //   glm::vec1(2.0f),
  //   "assets/textures/circle.png",
  //   "Child Sprite"
  // )));

  std::cout << "Root Children: " << root.children.size() << std::endl;

  currentScene.reset(new Scene(
    std::make_unique<Sprite>(std::move(root)),
    std::make_unique<Renderer>(
      MAX_FRAMES_IN_FLIGHT,
      device,
      physicalDevice,
      queue,
      commandPool,
      swapChainSurfaceFormat,
      cameraUBOs
    )
  ));
}

void Application::mainLoop() {
  int quad_offset = 1;

  lastFrameTime = std::chrono::high_resolution_clock::now();

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_EVENT_QUIT:
          running = false;
          break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
          running = false;
          break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
          framebufferResized = true;
          break;
        case SDL_EVENT_MOUSE_MOTION:
          if (mouseCaptured) {
            cameraYaw   -= event.motion.xrel * mouseSensitivity;
            cameraPitch += event.motion.yrel * mouseSensitivity;
            cameraPitch  = std::clamp(cameraPitch, -89.0f, 89.0f);

            glm::vec3 dir;
            dir.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
            dir.y = sin(glm::radians(cameraPitch));
            dir.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
            cameraFront = glm::normalize(dir);
          }
          break;
        case SDL_EVENT_KEY_DOWN:
          if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
            mouseCaptured = !mouseCaptured;
            SDL_SetWindowRelativeMouseMode(window, mouseCaptured);
          }
          if (event.key.scancode == SDL_SCANCODE_Q) {
            currentScene->addSprite(std::unique_ptr<Sprite>( new Sprite(
              std::vector<std::unique_ptr<Node>> {},
              glm::vec3 {static_cast<float>(quad_offset), 0.0f, 0.0f},
              glm::vec2 {1.0f, 1.0f},
              glm::vec1 {0.0f}
            )));
            quad_offset++;
            currentScene->printDebug();
          }
          break;
        default:
          break;
      }
    }

    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
    deltaTime = std::min(deltaTime, 0.1f);
    lastFrameTime = now;

    processInput(deltaTime);

    // updateGPUObjectsBuffer();

    currentScene->process(deltaTime);

    drawFrame();
	}

  device.waitIdle();
}

void Application::processInput(float deltaTime) {
  const bool *keys = SDL_GetKeyboardState(nullptr);
  float velocity = cameraSpeed * deltaTime;

  glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));

  if (keys[SDL_SCANCODE_W]) cameraPos += cameraUp * velocity;
  if (keys[SDL_SCANCODE_S]) cameraPos -= cameraUp * velocity;
  if (keys[SDL_SCANCODE_A]) cameraPos -= right * velocity;
  if (keys[SDL_SCANCODE_D]) cameraPos += right * velocity;
  if (keys[SDL_SCANCODE_SPACE])    cameraPos -= cameraFront * velocity;
  if (keys[SDL_SCANCODE_LCTRL])    cameraPos += cameraFront * velocity;
}

void Application::cleanup() {
  currentScene.reset();
  // delete currentScene.release();

  // Explicitly destroy all Vulkan objects before quitting SDL
  // destroys the Wayland display underneath them
  inFlightFences.clear();
  renderFinishedSemaphores.clear();
  presentCompleteSemaphores.clear();
  commandBuffers.clear();
  commandPool.clear();
  cameraUBOsMapped.clear();
  cameraUBOsMemory.clear();
  cameraUBOs.clear();
  depthImageView.clear();
  depthImageMemory.clear();
  depthImage.clear();
  swapChainImageViews.clear();
  swapChain.clear();
  device.clear();
  surface.clear();
  debugMessenger.clear();
  instance.clear();

  SDL_DestroyWindow(window);
  SDL_Quit();
}

void Application::cleanupSwapChain() {
  swapChainImageViews.clear();
  swapChain = nullptr;
}

void Application::recreateSwapChain() {
  int width = 0, height = 0;

  while (width == 0 || height == 0) {
    SDL_GetWindowSizeInPixels(window, &width, &height);
    SDL_Event event;
    SDL_WaitEvent(&event); // blocks until something happens
  }

  device.waitIdle();

  cleanupSwapChain();
  createSwapChain();
  createImageViews();
  createDepthResources();
}
