#include "vulkan/vulkan.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <glm/ext/matrix_float4x4.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#define APP_HPP_IMPLEMENTATION
#include "app.hpp"

#include "utils/utils.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

void Application::run() {
  std::cout << (enableValidationLayers ? "validationLayers enabled" : "validationLayers off") << std::endl;

	initWindow();
	initVulkan();
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
  createSwapChain();
  createImageViews();
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createCommandPool();
  createDepthResources();
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
  createVertexBuffer();
  createIndexBuffer();
  createCameraUBOs();
  createGPUObjectsBuffer();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();
  createSyncObjects();
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
                                                             vk::PhysicalDeviceVulkan13Features,
                                                             vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                                    features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
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
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
    featureChain = {
      {.features = { .samplerAnisotropy = true }},              // vk::PhysicalDeviceFeatures2
      {.shaderDrawParameters = true},                           // Enable shader draw parameters from Vulkan 1.1
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
    swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
  }
}

void Application::createDescriptorSetLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 3> bindings {
    {
      {
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex
      },
      {
        .binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment
      },
      {
        .binding = 2,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex
      }
    }
  };
  vk::DescriptorSetLayoutCreateInfo layoutInfo {
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings    = bindings.data()
  };
  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void Application::createGraphicsPipeline() {
  vk::raii::ShaderModule shaderModule = createShaderModule(readFile("src/shaders/slang.spv"));
  // vk::raii::ShaderModule vertshaderModule = createShaderModule(readFile("src/shaders/vert.spv"));
  // vk::raii::ShaderModule fragshaderModule = createShaderModule(readFile("src/shaders/frag.spv"));

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo {
    .stage  = vk::ShaderStageFlagBits::eVertex,
    .module = shaderModule,
    .pName  = "vertMain",
  };
  vk::PipelineShaderStageCreateInfo fragShaderStageInfo {
    .stage  = vk::ShaderStageFlagBits::eFragment,
    .module = shaderModule,
    .pName  = "fragMain",
  };
  vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
    .vertexBindingDescriptionCount   = 1,
    .pVertexBindingDescriptions      = &bindingDescription,
    .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
    .pVertexAttributeDescriptions    = attributeDescriptions.data()
  };
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly { .topology = vk::PrimitiveTopology::eTriangleList };
  vk::PipelineViewportStateCreateInfo viewportState { .viewportCount = 1, .scissorCount = 1 };

  vk::PipelineRasterizationStateCreateInfo rasterizer {
    .depthClampEnable        = vk::False,
    .rasterizerDiscardEnable = vk::False,
    .polygonMode             = vk::PolygonMode::eFill,
    .cullMode                = vk::CullModeFlagBits::eBack,
    .frontFace               = vk::FrontFace::eCounterClockwise,
    .depthBiasEnable         = vk::False,
    .lineWidth               = 1.0f
  };

  vk::PipelineMultisampleStateCreateInfo multisampling {
    .rasterizationSamples = vk::SampleCountFlagBits::e1,
    .sampleShadingEnable  = vk::False
  };

  vk::PipelineDepthStencilStateCreateInfo depthStencil {
    .depthTestEnable       = vk::True,
    .depthWriteEnable      = vk::True,
    .depthCompareOp        = vk::CompareOp::eLess,
    .depthBoundsTestEnable = vk::False,
    .stencilTestEnable     = vk::False,
  };

  vk::PipelineColorBlendAttachmentState colorBlendAttachment {
    .blendEnable    = vk::False,
    .colorWriteMask = vk::ColorComponentFlagBits::eR |
                      vk::ColorComponentFlagBits::eG | 
                      vk::ColorComponentFlagBits::eB | 
                      vk::ColorComponentFlagBits::eA
  };

  vk::PipelineColorBlendStateCreateInfo colorBlending {
    .logicOpEnable = vk::False,
    .logicOp = vk::LogicOp::eCopy,
    .attachmentCount = 1,
    .pAttachments = &colorBlendAttachment
  };

  std::vector<vk::DynamicState>      dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
	vk::PipelineDynamicStateCreateInfo dynamicState {
    .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
    .pDynamicStates = dynamicStates.data()
  };

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
    .setLayoutCount         = 1,
    .pSetLayouts            = &*descriptorSetLayout,
    .pushConstantRangeCount = 0
  };
  pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

  vk::Format depthFormat = findDepthFormat();

  vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
    {
      .stageCount          = 2,
      .pStages             = shaderStages,
      .pVertexInputState   = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState      = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState   = &multisampling,
      .pDepthStencilState  = &depthStencil,
      .pColorBlendState    = &colorBlending,
      .pDynamicState       = &dynamicState,
      .layout              = pipelineLayout,
      .renderPass          = nullptr
    },
    {
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
      .depthAttachmentFormat = depthFormat,
    }
  };

  graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void Application::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo {
    .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = queueIndex
  };
  commandPool = vk::raii::CommandPool(device, poolInfo);
}

void Application::createDepthResources() {
  vk::Format depthFormat = findDepthFormat();

  std::tie(depthImage, depthImageMemory) = createImage(
    swapChainExtent.width,
    swapChainExtent.height,
    depthFormat,
    vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eDepthStencilAttachment,
    vk::MemoryPropertyFlagBits::eDeviceLocal
  );
  depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

vk::Format Application::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
  for (const auto format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);
    if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
        ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)))
    {
      return format;
    }
  };

  throw std::runtime_error("Failed to find supported format!");
}

vk::Format Application::findDepthFormat() {
  return findSupportedFormat(
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
}

void Application::createTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  auto [stagingBuffer, stagingBufferMemory] = 
    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data = stagingBufferMemory.mapMemory(0, imageSize);
  memcpy(data, pixels, imageSize);
  stagingBufferMemory.unmapMemory();

  stbi_image_free(pixels);

  std::tie(textureImage, textureImageMemory) = createImage(
    texWidth,
    texHeight,
    vk::Format::eR8G8B8A8Srgb,
    vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    vk::MemoryPropertyFlagBits::eDeviceLocal
  );

  vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
  transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  copyBufferToImage(commandBuffer, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
  endSingleTimeCommands(std::move(commandBuffer));
}

void Application::createTextureSampler() {
  vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
  vk::SamplerCreateInfo samplerInfo {
    .magFilter        = vk::Filter::eLinear,
    .minFilter        = vk::Filter::eLinear,
    .mipmapMode       = vk::SamplerMipmapMode::eLinear,
    .addressModeU     = vk::SamplerAddressMode::eRepeat,
    .addressModeV     = vk::SamplerAddressMode::eRepeat,
    .addressModeW     = vk::SamplerAddressMode::eRepeat,
    .anisotropyEnable = vk::True,
    .maxAnisotropy    = properties.limits.maxSamplerAnisotropy,
    .compareEnable    = vk::False,
    .compareOp        = vk::CompareOp::eAlways,
  };

  textureSampler = vk::raii::Sampler(device, samplerInfo);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> Application::createImage(
  uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties
) {
  vk::ImageCreateInfo imageInfo{
    .imageType   = vk::ImageType::e2D,
    .format      = format,
    .extent      = {width, height, 1},
    .mipLevels   = 1,
    .arrayLayers = 1,
    .samples     = vk::SampleCountFlagBits::e1,
    .tiling      = tiling,
    .usage       = usage,
    .sharingMode = vk::SharingMode::eExclusive
  };

  vk::raii::Image image = vk::raii::Image(device, imageInfo);

  vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo {.allocationSize = memRequirements.size,
                                    .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
  image.bindMemory(imageMemory, 0);

  return {std::move(image), std::move(imageMemory)};
}

void Application::createTextureImageView() {
  textureImageView = createImageView(*textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

vk::raii::ImageView Application::createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
  vk::ImageViewCreateInfo viewInfo {
    .image            = image,
    .viewType         = vk::ImageViewType::e2D,
    .format           = format,
    .subresourceRange = {
      .aspectMask = aspectFlags,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    },
  };

  return vk::raii::ImageView(device, viewInfo);
}

void Application::transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
  vk::ImageMemoryBarrier barrier {
    .oldLayout           = oldLayout,
    .newLayout           = newLayout,
    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
    .image               = image,
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  };

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    sourceStage      = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }
  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}

void Application::copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height) {
  vk::BufferImageCopy region {
    .bufferOffset      = 0,
    .bufferRowLength   = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1
    },
    .imageOffset = {0, 0, 0},
    .imageExtent = {width, height, 1}
  };
  commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Application::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  vk::BufferCreateInfo bufferInfo {
    .size        = size,
    .usage       = usage,
    .sharingMode = vk::SharingMode::eExclusive,
  };
  vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo memoryAllocateInfo {
    .allocationSize  = memRequirements.size,
    .memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, 
      properties
    )
  };
  vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
  buffer.bindMemory(*bufferMemory, 0);
  return {std::move(buffer), std::move(bufferMemory)};
}

void Application::createVertexBuffer() {
  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto [stagingBuffer, stagingBufferMemory] =
    createBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, vertices.data(), bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(vertexBuffer, vertexBufferMemory) = 
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

  vertexBufferCapacity = bufferSize;
}

// void Application::updateVertexBuffer() {
//   vk::DeviceSize requiredSize = sizeof(vertices[0]) * vertices.size();
//   auto [stagingBuffer, stagingBufferMemory] = createBuffer(
//     requiredSize,
//     vk::BufferUsageFlagBits::eTransferSrc,
//     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
//   );
//
//   void* data = stagingBufferMemory.mapMemory(0, requiredSize);
//   memcpy(data, vertices.data(), requiredSize);
//   stagingBufferMemory.unmapMemory();
//
//   if (requiredSize > vertexBufferCapacity) {
//     vk::DeviceSize newCapacity = vertexBufferCapacity == 0 ? requiredSize : vertexBufferCapacity;
//     while (newCapacity < requiredSize) newCapacity *= 2;
//
//     device.waitIdle(); // old buffer may still be in flight
//
//     std::tie(vertexBuffer, vertexBufferMemory) = createBuffer(
//       newCapacity,
//       vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
//       vk::MemoryPropertyFlagBits::eDeviceLocal
//     );
//
//     vertexBufferCapacity = newCapacity;
//   }
//
//   copyBuffer(stagingBuffer, vertexBuffer, requiredSize);
// }

void Application::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto [stagingBuffer, stagingBufferMemory] =
    createBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, indices.data(), (size_t) bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(indexBuffer, indexBufferMemory) = 
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer, indexBuffer, bufferSize);

  indexBufferCapacity = bufferSize;
}

// void Application::updateIndexBuffer() {
//   vk::DeviceSize requiredSize = sizeof(indices[0]) * indices.size();
//   auto [stagingBuffer, stagingBufferMemory] = createBuffer(
//     requiredSize,
//     vk::BufferUsageFlagBits::eTransferSrc,
//     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
//   );
//
//   void* data = stagingBufferMemory.mapMemory(0, requiredSize);
//   memcpy(data, indices.data(), requiredSize);
//   stagingBufferMemory.unmapMemory();
//
//   if (requiredSize > indexBufferCapacity) {
//     vk::DeviceSize newCapacity = indexBufferCapacity == 0 ? requiredSize : indexBufferCapacity;
//     while (newCapacity < requiredSize) newCapacity *= 2;
//
//     device.waitIdle(); // old buffer may still be in flight
//
//     std::tie(indexBuffer, indexBufferMemory) = createBuffer(
//       newCapacity,
//       vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
//       vk::MemoryPropertyFlagBits::eDeviceLocal
//     );
//
//     indexBufferCapacity = newCapacity;
//   }
//
//   copyBuffer(stagingBuffer, indexBuffer, requiredSize);
// }

// void Application::addQuad(Quad const &quad) {
//   uint32_t base = static_cast<uint32_t>(vertices.size());
//   auto verts = quad.toVertices();
//   vertices.insert(vertices.end(), verts.begin(), verts.end());
//
//   indices.insert(indices.end(), {
//     base + 0, base + 1, base + 2,
//     base + 2, base + 3, base + 0
//   });
//
//   updateVertexBuffer(); 
//   updateIndexBuffer();
// }

void Application::createCameraUBOs() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(CameraUBO);
    auto [buffer, bufferMem] = createBuffer(
      bufferSize, 
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );
    cameraUBOs.emplace_back(std::move(buffer));
    cameraUBOsMemory.emplace_back(std::move(bufferMem));
    cameraUBOsMapped.emplace_back(cameraUBOsMemory.back().mapMemory(0, bufferSize));
  }
}

void Application::createGPUObjectsBuffer() {
  vk::DeviceSize bufferSize = sizeof(GPUObject) * 10;
  auto [buffer, bufferMem] = createBuffer(
    bufferSize,
    vk::BufferUsageFlagBits::eStorageBuffer,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
  );
  gpuObjectsBuffer = std::move(buffer);
  gpuObjectsMemory = std::move(bufferMem);
  gpuObjectsMapped = gpuObjectsMemory.mapMemory(0, bufferSize);
  gpuObjectsBufferCapacity = bufferSize;
}

void Application::updateGPUObjects() {
  gpuObjects.clear();
  
  for (auto const& object : quadObjects) {
    glm::mat4 model(1.0f);
  
    model = glm::translate(model, object.position);
  
    model = glm::rotate(
      model,
      object.rotation,
      glm::vec3(0,0,1));
  
    model = glm::scale(
        model,
        glm::vec3(object.scale.x, object.scale.y,1));
  
    gpuObjects.push_back({
      model
    });
  }
}

void Application::updateGPUObjectsBuffer() {
  updateGPUObjects();
  vk::DeviceSize requiredSize = sizeof(GPUObject) * gpuObjects.size();

  if (requiredSize > gpuObjectsBufferCapacity) {
    vk::DeviceSize newCapacity = gpuObjectsBufferCapacity == 0 ? requiredSize : gpuObjectsBufferCapacity;
    while (newCapacity < requiredSize) newCapacity *= 2;
  
    device.waitIdle(); // old buffer may still be in flight

    gpuObjectsMemory.unmapMemory();
    gpuObjectsBuffer.clear();
    gpuObjectsMemory.clear();

    std::tie(gpuObjectsBuffer, gpuObjectsMemory) = createBuffer(
      newCapacity,
      vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );
    gpuObjectsMapped = gpuObjectsMemory.mapMemory(0, newCapacity);
    gpuObjectsBufferCapacity = newCapacity;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo gpuObjectsBufferInfo {
        .buffer = gpuObjectsBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE
      };
      
      vk::WriteDescriptorSet write {
        .dstSet          = descriptorSets[i],
        .dstBinding      = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo     = &gpuObjectsBufferInfo
      };
      
      device.updateDescriptorSets(write, {});
    }
  }

  if (!gpuObjects.empty()) {
    memcpy(gpuObjectsMapped, gpuObjects.data(), sizeof(GPUObject) * gpuObjects.size());
  }
}

void Application::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 3> poolSize {
    {
      {
        .type            = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
      },
      {
        .type            = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
      },
      {
        .type            = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
      }
    }
  };
  vk::DescriptorPoolCreateInfo poolInfo {
    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    .maxSets       = MAX_FRAMES_IN_FLIGHT,
    .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
    .pPoolSizes    = poolSize.data()
  };

  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void Application::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
  vk::DescriptorSetAllocateInfo        allocInfo {
    .descriptorPool     = descriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
    .pSetLayouts        = layouts.data()
  };

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo {
      .buffer = cameraUBOs[i],
      .offset = 0,
      .range  = sizeof(CameraUBO)
    };
    vk::DescriptorImageInfo imageInfo {
      .sampler = textureSampler,
      .imageView = textureImageView,
      .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };
    vk::DescriptorBufferInfo gpuObjectsBufferInfo {
      .buffer = gpuObjectsBuffer,
      .offset = 0,
      .range  = VK_WHOLE_SIZE
    };
    std::array<vk::WriteDescriptorSet, 3> descriptorWrites {{
      {
        .dstSet          = descriptorSets[i],
        .dstBinding      = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo     = &cameraBufferInfo
      },
      {
        .dstSet          = descriptorSets[i],
        .dstBinding      = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo      = &imageInfo
      },
      {
        .dstSet          = descriptorSets[i],
        .dstBinding      = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo     = &gpuObjectsBufferInfo
      }
    }};
    device.updateDescriptorSets(descriptorWrites, {});
  }
}

vk::raii::CommandBuffer Application::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo allocInfo {
    .commandPool        = commandPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1
  };
  vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
  commandBuffer.begin(beginInfo);

  return std::move(commandBuffer);
}

void Application::endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo {
    .commandBufferCount = 1,
    .pCommandBuffers    = &*commandBuffer
  };
  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}

void Application::copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {
  vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
  commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{ .size = size });
  endSingleTimeCommands(std::move(commandCopyBuffer));
}

uint32_t Application::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (
      (typeFilter & (1 << i)) &&
      (memProperties.memoryTypes[i].propertyFlags & properties) == properties
    ) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void Application::createCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo {
    .commandPool = commandPool,
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT
  };
  commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void Application::recordCommandBuffer(uint32_t imageIndex) {
  auto &commandBuffer = commandBuffers[frameIndex];

  commandBuffer.begin({});

  // Before strating render, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
  transition_image_layout(
    swapChainImages[imageIndex],
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::eColorAttachmentOptimal,
    {},
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::ImageAspectFlagBits::eColor
  );

  transition_image_layout(
    *depthImage,
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::eDepthAttachmentOptimal,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    vk::ImageAspectFlagBits::eDepth
  );

  vk::ClearValue              clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
  vk::ClearValue              clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
  vk::RenderingAttachmentInfo attachmentInfo = {
    .imageView   = swapChainImageViews[imageIndex],
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp      = vk::AttachmentLoadOp::eClear,
    .storeOp     = vk::AttachmentStoreOp::eStore,
    .clearValue  = clearColor
  };
  vk::RenderingAttachmentInfo depthAttachmentInfo = {
    .imageView   = depthImageView,
    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
    .loadOp      = vk::AttachmentLoadOp::eClear,
    .storeOp     = vk::AttachmentStoreOp::eDontCare,
    .clearValue  = clearDepth
  };

  vk::RenderingInfo renderingInfo = {
    .renderArea = {
      .offset = {0, 0},
      .extent = swapChainExtent
    },
    .layerCount           = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments    = &attachmentInfo,
    .pDepthAttachment     = &depthAttachmentInfo,
  };

  commandBuffer.beginRendering(renderingInfo);

  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
  commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
  commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
  commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
  commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);
  commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), gpuObjects.size(), 0, 0, 0);

  commandBuffer.endRendering();

  // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
  transition_image_layout(
    swapChainImages[imageIndex],
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageLayout::ePresentSrcKHR,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    {},
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::ImageAspectFlagBits::eColor
  );

  commandBuffer.end();
}

void Application::transition_image_layout(
  vk::Image               image,
  vk::ImageLayout         old_layout,
  vk::ImageLayout         new_layout,
  vk::AccessFlags2        src_access_mask,
  vk::AccessFlags2        dst_access_mask,
  vk::PipelineStageFlags2 src_stage_mask,
  vk::PipelineStageFlags2 dst_stage_mask,
  vk::ImageAspectFlags    image_aspect_flags
) {
  vk::ImageMemoryBarrier2 barrier = {
		.srcStageMask        = src_stage_mask,
		.srcAccessMask       = src_access_mask,
		.dstStageMask        = dst_stage_mask,
		.dstAccessMask       = dst_access_mask,
		.oldLayout           = old_layout,
		.newLayout           = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image               = image,
		.subresourceRange    = {
		  .aspectMask     = image_aspect_flags,
		  .baseMipLevel   = 0,
		  .levelCount     = 1,
		  .baseArrayLayer = 0,
		  .layerCount     = 1
    }
  };
	vk::DependencyInfo dependencyInfo = {
		.dependencyFlags         = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier
  };
  commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
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

void Application::updateUniformBuffer(uint32_t currentImage) {
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
  updateUniformBuffer(frameIndex);

  // Only reset the fence if we are submitting work
  device.resetFences(*inFlightFences[frameIndex]);

  commandBuffers[frameIndex].reset();
  recordCommandBuffer(imageIndex);

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

void Application::mainLoop() {
  int quad_offset = 0;

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
            // addQuad({
            //   {{
            //     {-1.5f - quad_offset, -0.5f,  0.0f},
            //     {-0.5f - quad_offset, -0.5f,  0.0f},
            //     {-0.5f - quad_offset,  0.5f,  0.0f},
            //     {-1.5f - quad_offset,  0.5f,  0.0f}
            //   }}
            // });
            quadObjects.push_back({
              .position = {quad_offset, 0.0f, 0.0f},
              .rotation = 0.0f,
              .scale = {1.0f, 1.0f}
            });
            quad_offset++;
            std::cout << "SizeC: " << gpuObjectsBufferCapacity << "SizeG: " << gpuObjects.size() << std::endl;
  auto &m = gpuObjects.back().model;

std::cout
    << m[3].x << " "
    << m[3].y << " "
    << m[3].z << '\n';
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

    updateGPUObjectsBuffer();

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
  // Explicitly destroy all Vulkan objects before quitting SDL
  // destroys the Wayland display underneath them
  inFlightFences.clear();
  renderFinishedSemaphores.clear();
  presentCompleteSemaphores.clear();
  commandBuffers.clear();
  commandPool.clear();
  descriptorSets.clear();
  descriptorPool.clear();
  gpuObjectsMemory.clear();
  gpuObjectsBuffer.clear();
  cameraUBOsMapped.clear();
  cameraUBOsMemory.clear();
  cameraUBOs.clear();
  indexBufferMemory.clear();
  indexBuffer.clear();
  vertexBufferMemory.clear();
  vertexBuffer.clear();
  textureSampler.clear();
  textureImageView.clear();
  textureImageMemory.clear();
  textureImage.clear();
  depthImageView.clear();
  depthImageMemory.clear();
  depthImage.clear();
  graphicsPipeline.clear();
  pipelineLayout.clear();
  descriptorSetLayout.clear();
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
