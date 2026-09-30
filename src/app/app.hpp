#pragma once

#include <assert.h>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan.hpp;
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#ifdef APP_HPP_IMPLEMENTATION
  #define STB_IMAGE_IMPLEMENTATION
  #define TINYOBJLOADER_IMPLEMENTATION
#endif

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include "scenes/scene.hpp"
#include "editor/editor_overlay.hpp"
#include "input/camera.hpp"
#include "input/input_handler.hpp"

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
const std::vector<std::string> TEXTURE_PATHS = {
  "assets/textures/texture.jpg",
  "assets/textures/rockTexture.jpg"
};
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif // NDEBUG

class Application
{
public:
	void run();

private:
  bool                                 running        = true;

	SDL_Window                           *window        = nullptr;
  vk::raii::Context                    context;
  vk::raii::Instance                   instance       = nullptr;
  vk::raii::DebugUtilsMessengerEXT     debugMessenger = nullptr;
  vk::raii::SurfaceKHR                 surface        = nullptr;
  vk::raii::PhysicalDevice             physicalDevice = nullptr;
  vk::raii::Device                     device         = nullptr;
  vk::raii::Queue                      queue          = nullptr;
  uint32_t                             queueIndex     = ~0;
  vk::raii::SwapchainKHR               swapChain      = nullptr;
  std::vector<vk::Image>               swapChainImages;
  vk::SurfaceFormatKHR                 swapChainSurfaceFormat;
  vk::Extent2D                         swapChainExtent;
  std::vector<vk::raii::ImageView>     swapChainImageViews;

  vk::raii::Image                      depthImage         = nullptr;
  vk::raii::DeviceMemory               depthImageMemory   = nullptr;
  vk::raii::ImageView                  depthImageView     = nullptr;

  std::vector<vk::raii::Buffer>        cameraUBOs;
  std::vector<vk::raii::DeviceMemory>  cameraUBOsMemory;
  std::vector<void *>                  cameraUBOsMapped;

  vk::raii::CommandPool                commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;

  std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
  std::vector<vk::raii::Fence>         inFlightFences;
  uint32_t                             frameIndex         = 0;
  bool                                 framebufferResized = false;

  Camera                        camera;
  std::unique_ptr<InputHandler> input = nullptr;

  vk::Extent2D renderTargetExtent();

  std::chrono::high_resolution_clock::time_point lastFrameTime;

  std::vector<const char *> requiredDeviceExtension = {
    vk::KHRSwapchainExtensionName
  };

  std::unique_ptr<Scene> currentScene = nullptr;

  std::unique_ptr<EditorOverlay> editorOverlay = nullptr;
  bool showEditor = false; // F1: editor UI with the game in a panel, vs. fullscreen game

  int nextQuadOffset = 1;
  void setEditorOpen(bool open);
  void addQuad();
  void resizeViewport(vk::Extent2D extent);

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                        vk::DebugUtilsMessageTypeFlagsEXT              type,
                                                        const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                        void *                                         pUserData)
  {
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
  };

	void initWindow();
	void initVulkan();
  void initEditorOverlay();
  void initScene();

  void createInstance();
  void setupDebugMessenger();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSwapChain();
  void createImageViews();
  void createCommandPool();
  void createDepthResources();
  void createCameraUBOs();
  void createCommandBuffers();

  void createSyncObjects();
  void updateCameraUBOBuffer(uint32_t currentImage);
  void drawFrame();

  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo {
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    vk::raii::ShaderModule shaderModule { device, createInfo };
    return shaderModule;
  }

  uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilites);
  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats);
  vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
  vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);

  std::vector<const char*> getRequiredInstanceExtensions();

	void mainLoop();
	void cleanup();

  void cleanupSwapChain();
  void recreateSwapChain();
};
