#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
  #define GLM_ENABLE_EXPERIMENTAL
#endif // !GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/hash.hpp>

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

  glm::vec3 color = {1.0f, 0.0f, 0.5f};
  uint32_t textureIndex = 0;
};

struct CameraUBO {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct GPUObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::vec3 color;
  alignas(4) uint32_t textureIndex = 0;
};

namespace vk_util {

static uint32_t findMemoryType(vk::raii::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
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

static std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(
  vk::raii::Device& device,
  vk::raii::PhysicalDevice& physicalDevice,
  vk::DeviceSize size,
  vk::BufferUsageFlags usage,
  vk::MemoryPropertyFlags properties
) {
  vk::BufferCreateInfo bufferInfo {
    .size        = size,
    .usage       = usage,
    .sharingMode = vk::SharingMode::eExclusive,
  };
  vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo memoryAllocateInfo {
    .allocationSize  = memRequirements.size,
    .memoryTypeIndex = vk_util::findMemoryType(
      physicalDevice,
      memRequirements.memoryTypeBits, 
      properties
    )
  };
  vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
  buffer.bindMemory(*bufferMemory, 0);
  return {std::move(buffer), std::move(bufferMemory)};
}

static vk::raii::CommandBuffer beginSingleTimeCommands(
  vk::raii::Device& device,
  vk::raii::CommandPool& commandPool
) {
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

static void endSingleTimeCommands(
  vk::raii::Queue& queue,
  vk::raii::CommandBuffer &&commandBuffer
) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo {
    .commandBufferCount = 1,
    .pCommandBuffers    = &*commandBuffer
  };
  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}

static void copyBufferToImage(
  vk::raii::CommandBuffer &commandBuffer,
  const vk::raii::Buffer &buffer,
  vk::raii::Image &image,
  uint32_t width,
  uint32_t height
) {
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

static void transitionImageLayout(
  vk::raii::CommandBuffer &commandBuffer,
  const vk::raii::Image &image,
  vk::ImageLayout oldLayout,
  vk::ImageLayout newLayout
) {
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

static std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
  vk::raii::Device& device,
  vk::raii::PhysicalDevice& physicalDevice,
  uint32_t width,
  uint32_t height,
  vk::Format format,
  vk::ImageTiling tiling,
  vk::ImageUsageFlags usage,
  vk::MemoryPropertyFlags properties
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
                                    .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
  image.bindMemory(imageMemory, 0);

  return {std::move(image), std::move(imageMemory)};
}

static vk::raii::ImageView createImageView(
  vk::raii::Device& device,
  vk::Image const &image,
  vk::Format format,
  vk::ImageAspectFlags aspectFlags
) {
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

static vk::Format findSupportedFormat(
  vk::raii::Device& device,
  vk::raii::PhysicalDevice& physicalDevice,
  const std::vector<vk::Format>& candidates,
  vk::ImageTiling tiling,
  vk::FormatFeatureFlags features
) {
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

static vk::Format findDepthFormat(
  vk::raii::Device& device,
  vk::raii::PhysicalDevice& physicalDevice
) {
  return findSupportedFormat(
    device,
    physicalDevice,
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
}

static void copyBuffer(
  vk::raii::Device& device,
  vk::raii::CommandPool& commandPool,
  vk::raii::Queue& queue,
  vk::raii::Buffer & srcBuffer,
  vk::raii::Buffer & dstBuffer,
  vk::DeviceSize size
) {
  vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands(device, commandPool);
  commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{ .size = size });
  endSingleTimeCommands(queue, std::move(commandCopyBuffer));
}

}
