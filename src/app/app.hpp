#pragma once

#include <array>
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
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

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
const std::string TEXTURE_PATH = "assets/textures/texture.jpg";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif // NDEBUG

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {
      .binding   = 0,
      .stride    = sizeof(Vertex),
      .inputRate = vk::VertexInputRate::eVertex,
    };
  }

  static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
		return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
		         {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
		         {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord)}}};
	}

  bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord; 
  }
};

template<>
struct std::hash<Vertex> {
  size_t operator()(Vertex const& vertex) const noexcept {
    return ((std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (std::hash<glm::vec2>()(vertex.texCoord) << 1);
  }
};

const std::vector<Vertex> vertices = {
  {{-0.5f, -0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
  {{ 0.5f, -0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
  {{ 0.5f,  0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
  {{-0.5f,  0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
};
const std::vector<uint32_t> indices = {
  0, 1, 2, 2, 3, 0,
};

struct Quad {
  std::array<glm::vec3, 4> points;
  glm::vec3                color = {1.0f, 1.0f, 1.0f};
  uint32_t                 texIndex = 0;

  std::array<Vertex, 4> toVertices() const {
    return {{
      {points[0], color, {1.0f, 0.0f}},
      {points[1], color, {0.0f, 0.0f}},
      {points[2], color, {0.0f, 1.0f}},
      {points[3], color, {1.0f, 1.0f}},
    }};
  }
};

struct QuadObject {
  glm::vec3 position{0.0f};
  float rotation = 0.0f; // Around Z
  glm::vec2 scale{1.0f};

  glm::vec3 color{0.0f};
};

struct CameraUBO {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct GPUObject {
  alignas(16) glm::mat4 model;
};

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

  vk::raii::DescriptorSetLayout        descriptorSetLayout = nullptr;
  vk::raii::PipelineLayout             pipelineLayout      = nullptr;
  vk::raii::Pipeline                   graphicsPipeline    = nullptr;

  vk::raii::Image                      depthImage         = nullptr;
  vk::raii::DeviceMemory               depthImageMemory   = nullptr;
  vk::raii::ImageView                  depthImageView     = nullptr;

  vk::raii::Image                      textureImage       = nullptr;
  vk::raii::DeviceMemory               textureImageMemory = nullptr;
  vk::raii::ImageView                  textureImageView   = nullptr;
  vk::raii::Sampler                    textureSampler     = nullptr;


  vk::raii::Buffer                     vertexBuffer         = nullptr;
  vk::raii::DeviceMemory               vertexBufferMemory   = nullptr;
  vk::DeviceSize                       vertexBufferCapacity = 0;
  vk::raii::Buffer                     indexBuffer          = nullptr;
  vk::raii::DeviceMemory               indexBufferMemory    = nullptr;
  vk::DeviceSize                       indexBufferCapacity  = 0;

  std::vector<vk::raii::Buffer>        cameraUBOs;
  std::vector<vk::raii::DeviceMemory>  cameraUBOsMemory;
  std::vector<void *>                  cameraUBOsMapped;

  std::vector<QuadObject>              quadObjects = {{}};
  std::vector<GPUObject>               gpuObjects;
  vk::raii::Buffer                     gpuObjectsBuffer = nullptr;
  vk::raii::DeviceMemory               gpuObjectsMemory = nullptr;
  void*                                gpuObjectsMapped = nullptr;
  vk::DeviceSize                       gpuObjectsBufferCapacity  = 0;

  vk::raii::DescriptorPool             descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptorSets;

  vk::raii::CommandPool                commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;

  std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
  std::vector<vk::raii::Fence>         inFlightFences;
  uint32_t                             frameIndex         = 0;
  bool                                 framebufferResized = false;

  const bool                           *keys = SDL_GetKeyboardState(nullptr);
  glm::vec3 cameraPos   = {0.0f, 0.0f, 2.0f};
  glm::vec3 cameraFront = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f));
  glm::vec3 cameraUp    = {0.0f, -1.0f, 0.0f};
  float     cameraYaw   = -90.0f;
  float     cameraPitch = 0.0f;
  float     cameraSpeed = 3.0f; // units/sec

  bool  mouseCaptured = true;
  float mouseSensitivity = 0.1f;

  std::chrono::high_resolution_clock::time_point lastFrameTime;

  void processInput(float deltaTime);

  std::vector<const char *> requiredDeviceExtension = {
    vk::KHRSwapchainExtensionName
  };

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
  void createInstance();
  void setupDebugMessenger();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSwapChain();
  void createImageViews();
  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  void createCommandPool();
  void createDepthResources();
  vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
  vk::Format findDepthFormat();
  void createTextureImage();
  void createTextureImageView();
  void createTextureSampler();
  std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
    uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties
  );
  vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags);
  void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
  void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height);
  std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
  vk::raii::CommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer);
  void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size);
  void createVertexBuffer();
  void updateVertexBuffer();
  void createIndexBuffer();
  void updateIndexBuffer();
  void addQuad(Quad const &quad);
  void createCameraUBOs();
  void createGPUObjectsBuffer();
  void updateGPUObjects();
  void updateGPUObjectsBuffer();
  void createDescriptorPool();
  void createDescriptorSets();
  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
  void createCommandBuffers();

  void recordCommandBuffer(uint32_t imageIndex);
  void transition_image_layout(
    vk::Image               image,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags    image_aspect_flags
  );

  void createSyncObjects();
  void updateUniformBuffer(uint32_t currentImage);
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
