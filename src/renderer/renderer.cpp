#include "renderer.hpp" 
#include "utils/utils.hpp"
#include "vulkan/vulkan.hpp"

#include <stb_image.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <unordered_set>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan.hpp;
#endif

namespace {

// Extensions stb_image.h's stbi_load() can decode (JPEG/PNG/TGA/BMP/PSD/GIF/HDR/PIC/PNM).
bool hasStbSupportedExtension(const std::string& path) {
  static const std::unordered_set<std::string> supportedExtensions = {
    ".jpg", ".jpeg", ".png", ".tga", ".bmp", ".psd", ".gif", ".hdr", ".pic", ".pnm", ".ppm", ".pgm"
  };
  std::string extension = std::filesystem::path(path).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
    [](unsigned char c) { return std::tolower(c); });
  return supportedExtensions.contains(extension);
}

} // namespace

void Renderer::init() {
  createTextureImages();
  createTextureImageViews();
  createTextureSamplers();
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createVertexBuffer();
  createIndexBuffer();
  createGPUObjectsBuffer();
  createDescriptorPool();
  createDescriptorSets();
  createViewportSampler();
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> Renderer::createTextureImage(std::string texturePath) {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;
  
  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }
  
  auto [stagingBuffer, stagingBufferMemory] = 
    vk_util::createBuffer(
      device,
      physicalDevice,
      imageSize, 
      vk::BufferUsageFlagBits::eTransferSrc, 
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );
  
  void* data = stagingBufferMemory.mapMemory(0, imageSize);
  memcpy(data, pixels, imageSize);
  stagingBufferMemory.unmapMemory();
  
  stbi_image_free(pixels);
  
  auto [textureImage, textureImageMemory] = vk_util::createImage(
    device,
    physicalDevice,
    texWidth,
    texHeight,
    vk::Format::eR8G8B8A8Srgb,
    vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    vk::MemoryPropertyFlagBits::eDeviceLocal
  );
  
  vk::raii::CommandBuffer commandBuffer = vk_util::beginSingleTimeCommands(device, commandPool);
  vk_util::transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  vk_util::copyBufferToImage(commandBuffer, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  vk_util::transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
  vk_util::endSingleTimeCommands(queue, std::move(commandBuffer));

  return {std::move(textureImage), std::move(textureImageMemory)};
}

vk::raii::ImageView Renderer::createTextureImageView(vk::raii::Image& textureImage) {
  auto textureImageView = vk_util::createImageView(device, *textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
  return std::move(textureImageView);
}

vk::raii::Sampler Renderer::createTextureSampler() {
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

  auto textureSampler = vk::raii::Sampler(device, samplerInfo);
  return std::move(textureSampler);
}

void Renderer::createTextureImages() {
  for (std::string TEXTURE_PATH : TEXTURE_PATHS) {
    auto [textureImage, textureImageMemory] = createTextureImage(TEXTURE_PATH);
    textureImages.push_back(std::move(textureImage));
    textureImageMemories.push_back(std::move(textureImageMemory));
  }

  std::cout << "Total textures loaded: " << textureImages.size() << std::endl;
}

void Renderer::createTextureImageViews() {
  for (int i = 0; i < textureImages.size(); i++) {
    textureImageViews.push_back(std::move(createTextureImageView(textureImages[i])));
  }
}

void Renderer::createTextureSamplers() {
  textureSampler = createTextureSampler();
}

void Renderer::createDescriptorSetLayout() {
  vk::DescriptorBindingFlags bindingFlags[3] = {
    {},
    {},
    vk::DescriptorBindingFlagBits::eVariableDescriptorCount | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
  };
  vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {
    .bindingCount = 3,
    .pBindingFlags = bindingFlags
  };

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
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex
      },
      {
        .binding = 2,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = MAX_TEXTURES,
        .stageFlags = vk::ShaderStageFlagBits::eFragment
      },
    }
  };
  vk::DescriptorSetLayoutCreateInfo layoutInfo {
    .pNext        = &flagsInfo,
    .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings    = bindings.data()
  };
  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void Renderer::createGraphicsPipeline() {
  vk::raii::ShaderModule shaderModule = createShaderModule(readFile("src/shaders/slang.spv"));

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
    .depthWriteEnable      = vk::False,
    .depthCompareOp        = vk::CompareOp::eLess,
    .depthBoundsTestEnable = vk::False,
    .stencilTestEnable     = vk::False,
  };

  vk::PipelineColorBlendAttachmentState colorBlendAttachment {
    .blendEnable         = vk::True,
    .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
    .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
    .colorBlendOp        = vk::BlendOp::eAdd,
    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
    // "Over" for alpha too, so transparent texels don't punch holes in the
    // opaque clear - the editor's Game panel alpha-blends this image.
    .dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
    .alphaBlendOp        = vk::BlendOp::eAdd,
    .colorWriteMask      = vk::ColorComponentFlagBits::eR |
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

  vk::Format depthFormat = vk_util::findDepthFormat(device, physicalDevice);

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

void Renderer::createVertexBuffer() {
  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto [stagingBuffer, stagingBufferMemory] =
    vk_util::createBuffer(
      device,
      physicalDevice,
      bufferSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

  void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, vertices.data(), bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(vertexBuffer, vertexBufferMemory) = 
    vk_util::createBuffer(
      device,
      physicalDevice,
      bufferSize,
      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );

  vk_util::copyBuffer(
    device,
    commandPool,
    queue,
    stagingBuffer, 
    vertexBuffer,
    bufferSize
  );

  vertexBufferCapacity = bufferSize;
}

void Renderer::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto [stagingBuffer, stagingBufferMemory] =
    vk_util::createBuffer(
      device,
      physicalDevice,
      bufferSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

  void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, indices.data(), (size_t) bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(indexBuffer, indexBufferMemory) = 
    vk_util::createBuffer(
      device,
      physicalDevice,
      bufferSize,
      vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );

  vk_util::copyBuffer(
    device,
    commandPool,
    queue,
    stagingBuffer, 
    indexBuffer,
    bufferSize
  );

  indexBufferCapacity = bufferSize;
}

void Renderer::createGPUObjectsBuffer() {
  vk::DeviceSize bufferSize = sizeof(GPUObject) * 10000;
  auto [buffer, bufferMem] = 
    vk_util::createBuffer(
      device,
      physicalDevice,
      bufferSize,
      vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );
  gpuObjectsBuffer = std::move(buffer);
  gpuObjectsMemory = std::move(bufferMem);
  gpuObjectsMapped = gpuObjectsMemory.mapMemory(0, bufferSize);
  gpuObjectsBufferCapacity = bufferSize;
}

void Renderer::updateGPUObjects() {
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
      .model = model,
      .color = object.color,
      .textureIndex = object.textureIndex,
    });
  }
}

void Renderer::updateGPUObjectsBuffer() {
  updateGPUObjects();
  vk::DeviceSize requiredSize = sizeof(GPUObject) * gpuObjects.size();

  if (requiredSize > gpuObjectsBufferCapacity) {
    vk::DeviceSize newCapacity = gpuObjectsBufferCapacity == 0 ? requiredSize : gpuObjectsBufferCapacity;
    while (newCapacity < requiredSize) newCapacity *= 2;
  
    device.waitIdle(); // old buffer may still be in flight

    gpuObjectsMemory.unmapMemory();
    gpuObjectsBuffer.clear();
    gpuObjectsMemory.clear();

    std::tie(gpuObjectsBuffer, gpuObjectsMemory) = 
      vk_util::createBuffer(
        device,
        physicalDevice,
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
        .dstBinding      = 1,
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

void Renderer::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 3> poolSize {
    {
      {
        .type            = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT,
      },
      {
        .type            = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT,
      },
      {
        .type            = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES,
      },
    }
  };
  vk::DescriptorPoolCreateInfo poolInfo {
    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
                     vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
    .maxSets       = (uint32_t) MAX_FRAMES_IN_FLIGHT,
    .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
    .pPoolSizes    = poolSize.data()
  };

  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void Renderer::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

  std::vector<uint32_t> variableCounts(MAX_FRAMES_IN_FLIGHT, MAX_TEXTURES);

  vk::DescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo {
    .descriptorSetCount = static_cast<uint32_t>(variableCounts.size()),
    .pDescriptorCounts = variableCounts.data()
  };

  vk::DescriptorSetAllocateInfo        allocInfo {
    .pNext              = &variableCountInfo,
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
    std::vector<vk::DescriptorImageInfo> imageInfos;
    for (int i = 0; i < textureImages.size(); i++) {
      imageInfos.push_back(vk::DescriptorImageInfo {
        .sampler = textureSampler,
        .imageView = textureImageViews[i],
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
      });
    }
    vk::DescriptorBufferInfo gpuObjectsBufferInfo {
      .buffer = gpuObjectsBuffer,
      .offset = 0,
      .range  = VK_WHOLE_SIZE
    };

    vk::WriteDescriptorSet cameraBufferWrite {
      .dstSet          = descriptorSets[i],
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = vk::DescriptorType::eUniformBuffer,
      .pBufferInfo     = &cameraBufferInfo
    };
    vk::WriteDescriptorSet gpuObjectsWrite {
      .dstSet          = descriptorSets[i],
      .dstBinding      = 1,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = vk::DescriptorType::eStorageBuffer,
      .pBufferInfo     = &gpuObjectsBufferInfo
    };
    vk::WriteDescriptorSet imageWrite {
      .dstSet          = descriptorSets[i],
      .dstBinding      = 2,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
      .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
      .pImageInfo      = imageInfos.data()
    };

    std::array<vk::WriteDescriptorSet, 3> descriptorWrites {{
      cameraBufferWrite,
      gpuObjectsWrite,
      imageWrite,
    }};
    device.updateDescriptorSets(descriptorWrites, {});
  }
}

void Renderer::transition_image_layout(
  vk::raii::CommandBuffer& commandBuffer,
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
  commandBuffer.pipelineBarrier2(dependencyInfo);
}

// Transitions the given color/depth attachments for writing and begins a
// dynamic-rendering pass that clears both.
void Renderer::beginPass(
  vk::raii::CommandBuffer& commandBuffer,
  vk::Image     colorImage,
  vk::ImageView colorImageView,
  vk::Image     depthImage,
  vk::ImageView depthImageView,
  vk::Extent2D  extent
) {
  transition_image_layout(
    commandBuffer,
    colorImage,
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::eColorAttachmentOptimal,
    {},
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::ImageAspectFlagBits::eColor
  );

  transition_image_layout(
    commandBuffer,
    depthImage,
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
    .imageView   = colorImageView,
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
      .extent = extent
    },
    .layerCount           = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments    = &attachmentInfo,
    .pDepthAttachment     = &depthAttachmentInfo,
  };

  commandBuffer.beginRendering(renderingInfo);
}

void Renderer::drawScene(vk::raii::CommandBuffer& commandBuffer, vk::Extent2D extent, uint32_t frameIndex) {
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
  commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f));
  commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
  commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
  commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);
  commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), gpuObjects.size(), 0, 0, 0);
}

void Renderer::recordFrame(
  vk::raii::CommandBuffer& commandBuffer,
  vk::Image swapChainImage,
  vk::Extent2D swapChainExtent,
  vk::raii::ImageView& swapChainImageView,
  vk::raii::Image& depthImage,
  vk::raii::ImageView& depthImageView,
  uint32_t frameIndex,
  unsigned int imageIndex,
  std::function<void(vk::raii::CommandBuffer&)> overlayDraw,
  bool sceneToViewport
) {
  updateGPUObjectsBuffer();

  commandBuffer.begin({});

  if (sceneToViewport) {
    // Pass 1: draw the scene into this frame's offscreen viewport image...
    ViewportTarget& target = viewportTargets[frameIndex];
    beginPass(commandBuffer, *target.colorImage, *target.colorImageView, *target.depthImage, *target.depthImageView, viewportExtent);
    drawScene(commandBuffer, viewportExtent, frameIndex);
    commandBuffer.endRendering();

    // ...then make it readable by the UI's fragment shader.
    transition_image_layout(
      commandBuffer,
      *target.colorImage,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::AccessFlagBits2::eShaderSampledRead,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eFragmentShader,
      vk::ImageAspectFlagBits::eColor
    );

    // Pass 2: the swapchain only holds the UI, which samples the image above.
    beginPass(commandBuffer, swapChainImage, *swapChainImageView, *depthImage, *depthImageView, swapChainExtent);
  } else {
    beginPass(commandBuffer, swapChainImage, *swapChainImageView, *depthImage, *depthImageView, swapChainExtent);
    drawScene(commandBuffer, swapChainExtent, frameIndex);
  }

  if (overlayDraw) overlayDraw(commandBuffer);

  commandBuffer.endRendering();

  // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
  transition_image_layout(
    commandBuffer,
    swapChainImage,
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

void Renderer::createViewportSampler() {
  vk::SamplerCreateInfo samplerInfo {
    .magFilter    = vk::Filter::eLinear,
    .minFilter    = vk::Filter::eLinear,
    .mipmapMode   = vk::SamplerMipmapMode::eLinear,
    .addressModeU = vk::SamplerAddressMode::eClampToEdge,
    .addressModeV = vk::SamplerAddressMode::eClampToEdge,
    .addressModeW = vk::SamplerAddressMode::eClampToEdge,
  };
  viewportSampler = vk::raii::Sampler(device, samplerInfo);
}

void Renderer::resizeViewport(vk::Extent2D extent) {
  if (extent.width == 0 || extent.height == 0) return;
  if (extent == viewportExtent && !viewportTargets.empty()) return;

  device.waitIdle(); // the old targets may still be in use by frames in flight
  viewportTargets.clear();

  // Same color/depth formats as the swapchain pass, so the one graphics
  // pipeline can render into either.
  vk::Format colorFormat = swapChainSurfaceFormat.format;
  vk::Format depthFormat = vk_util::findDepthFormat(device, physicalDevice);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    ViewportTarget target;

    std::tie(target.colorImage, target.colorImageMemory) = vk_util::createImage(
      device,
      physicalDevice,
      extent.width,
      extent.height,
      colorFormat,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    target.colorImageView = vk_util::createImageView(device, target.colorImage, colorFormat, vk::ImageAspectFlagBits::eColor);

    std::tie(target.depthImage, target.depthImageMemory) = vk_util::createImage(
      device,
      physicalDevice,
      extent.width,
      extent.height,
      depthFormat,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    target.depthImageView = vk_util::createImageView(device, target.depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

    viewportTargets.push_back(std::move(target));
  }

  viewportExtent = extent;
}

std::vector<vk::ImageView> Renderer::getViewportImageViews() const {
  std::vector<vk::ImageView> views;
  for (const ViewportTarget& target : viewportTargets) {
    views.push_back(*target.colorImageView);
  }
  return views;
}

int Renderer::addTexture(std::string texturePath) {
  if (!(std::filesystem::exists(texturePath) && hasStbSupportedExtension(texturePath))) {
    return -1;
  }

  auto [textureImage, textureImageMemory] = createTextureImage(texturePath);
  auto textureImageView = createTextureImageView(textureImage);

  uint32_t textureIndex = textureImages.size();
  textureImages.push_back(std::move(textureImage));
  textureImageMemories.push_back(std::move(textureImageMemory));
  textureImageViews.push_back(std::move(textureImageView));

  // Update descriptor sets for all frames
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorImageInfo imageInfo {
      .sampler = *textureSampler,
      .imageView = *textureImageViews[textureIndex],
      .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };
    
    vk::WriteDescriptorSet write {
      .dstSet = descriptorSets[i],
      .dstBinding = 2,
      .dstArrayElement = textureIndex,  // Write to specific index
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .pImageInfo = &imageInfo
    };
    
    device.updateDescriptorSets(write, {});
  }

  return textureIndex;
}
