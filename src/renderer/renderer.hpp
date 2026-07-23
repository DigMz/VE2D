#pragma once

#include <assert.h>
#include <cstdint>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan.hpp;
#endif

#include "utils/vulkan_utils.hpp"


class Renderer {
public:
  Renderer(
    int MAX_FRAMES_IN_FLIGHT,
    vk::raii::Device& device,
    vk::raii::PhysicalDevice& physicalDevice,
    vk::raii::Queue& queue,
    vk::raii::CommandPool& commandPool,
    vk::SurfaceFormatKHR& swapChainSurfaceFormat,
    std::vector<vk::raii::Buffer>& cameraUBOs
  ):
    MAX_FRAMES_IN_FLIGHT(MAX_FRAMES_IN_FLIGHT),
    device(device),
    physicalDevice(physicalDevice),
    queue(queue),
    commandPool(commandPool),
    swapChainSurfaceFormat(swapChainSurfaceFormat),
    cameraUBOs(cameraUBOs)
  {
    init();
  }

  void clear() {
    descriptorSets.clear();
    descriptorPool.clear();
    indexBufferMemory.clear();
    indexBuffer.clear();
    vertexBufferMemory.clear();
    vertexBuffer.clear();
    textureSampler.clear();
    textureImageViews.clear();
    textureImageMemories.clear();
    textureImages.clear();
    gpuObjectsMemory.clear();
    gpuObjectsBuffer.clear();
    graphicsPipeline.clear();
    pipelineLayout.clear();
    descriptorSetLayout.clear();
  }

  void recordFrame(
    vk::raii::CommandBuffer& commandBuffer,
    vk::Image swapChainImage,
    vk::Extent2D swapChainExtent,
    vk::raii::ImageView& swapChainImageView,
    vk::raii::Image& depthImage,
    vk::raii::ImageView& depthImageView,
    uint32_t frameIndex,
    unsigned int imageIndex
  );

  void addQuadObject(QuadObject x) {
    quadObjects.push_back(x);
  }

  void printDebug() {
    std::cout << "SizeC: " << gpuObjectsBufferCapacity << "SizeG: " << gpuObjects.size() << std::endl;
  }

private:
  const std::vector<std::string> TEXTURE_PATHS = {
    "assets/textures/texture.jpg",
  };

  int MAX_FRAMES_IN_FLIGHT;
  const uint32_t MAX_TEXTURES = 512;

  vk::raii::Device&         device;
  vk::raii::PhysicalDevice& physicalDevice;
  vk::raii::Queue&          queue;
  vk::raii::CommandPool&    commandPool;
  vk::SurfaceFormatKHR      swapChainSurfaceFormat;
  std::vector<vk::raii::Buffer>&        cameraUBOs;

  vk::raii::DescriptorSetLayout        descriptorSetLayout  = nullptr;
  vk::raii::PipelineLayout             pipelineLayout       = nullptr;
  vk::raii::Pipeline                   graphicsPipeline     = nullptr;

  std::vector<vk::raii::Image>         textureImages;
  std::vector<vk::raii::DeviceMemory>  textureImageMemories;
  std::vector<vk::raii::ImageView>     textureImageViews;
  vk::raii::Sampler                    textureSampler       = nullptr;

  vk::raii::Buffer                     vertexBuffer         = nullptr;
  vk::raii::DeviceMemory               vertexBufferMemory   = nullptr;
  vk::DeviceSize                       vertexBufferCapacity = 0;
  vk::raii::Buffer                     indexBuffer          = nullptr;
  vk::raii::DeviceMemory               indexBufferMemory    = nullptr;
  vk::DeviceSize                       indexBufferCapacity  = 0;

  std::vector<QuadObject>              quadObjects = {};
  std::vector<GPUObject>               gpuObjects;
  vk::raii::Buffer                     gpuObjectsBuffer = nullptr;
  vk::raii::DeviceMemory               gpuObjectsMemory = nullptr;
  void*                                gpuObjectsMapped = nullptr;
  vk::DeviceSize                       gpuObjectsBufferCapacity  = 0;

  vk::raii::DescriptorPool             descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptorSets;

  void init();

  std::pair<vk::raii::Image, vk::raii::DeviceMemory> createTextureImage(std::string texturePath);
  vk::raii::ImageView createTextureImageView(vk::raii::Image& textureImage);
  vk::raii::Sampler   createTextureSampler();
  
  void createTextureImages();
  void createTextureImageViews();
  void createTextureSamplers();

  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  void createVertexBuffer();
  void createIndexBuffer();
  void createGPUObjectsBuffer();
  void updateGPUObjects();
  void updateGPUObjectsBuffer();
  void createDescriptorPool();
  void createDescriptorSets();

  void addTexture(std::string texturePath);

  void transition_image_layout(
    vk::raii::CommandBuffer& commandBuffer,
    vk::Image               image,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags    image_aspect_flags
  );

  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo {
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    vk::raii::ShaderModule shaderModule { device, createInfo };
    return shaderModule;
  }


};
