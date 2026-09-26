#include "debug_overlay.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

DebugOverlay::DebugOverlay(
  SDL_Window* window,
  vk::raii::Instance& instance,
  vk::raii::PhysicalDevice& physicalDevice,
  vk::raii::Device& device,
  uint32_t queueFamily,
  vk::raii::Queue& queue,
  vk::Format colorFormat,
  vk::Format depthFormat,
  uint32_t imageCount
) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr; // nothing worth persisting yet (single stats window)

  ImGui_ImplSDL3_InitForVulkan(window);

  // Dedicated pool for ImGui's own descriptor sets (font atlas, etc.), per
  // imgui_impl_vulkan.h: needs eFreeDescriptorSet and room for at least one
  // combined image sampler.
  vk::DescriptorPoolSize poolSize {
    .type            = vk::DescriptorType::eCombinedImageSampler,
    .descriptorCount = 1
  };
  vk::DescriptorPoolCreateInfo poolInfo {
    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    .maxSets       = 1,
    .poolSizeCount = 1,
    .pPoolSizes    = &poolSize
  };
  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);

  // ImGui_ImplVulkan_Init stores a pointer into this array (not a copy of its
  // contents), so it must outlive pipeline creation - a function-static keeps
  // it alive for the whole program.
  static VkFormat colorFormats[1] = { static_cast<VkFormat>(colorFormat) };

  VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
  pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipelineRenderingCreateInfo.colorAttachmentCount = 1;
  pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats;
  pipelineRenderingCreateInfo.depthAttachmentFormat = static_cast<VkFormat>(depthFormat);

  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.Instance                    = *instance;
  initInfo.PhysicalDevice              = *physicalDevice;
  initInfo.Device                      = *device;
  initInfo.QueueFamily                 = queueFamily;
  initInfo.Queue                       = *queue;
  initInfo.DescriptorPool              = *descriptorPool;
  initInfo.RenderPass                  = VK_NULL_HANDLE;
  initInfo.MinImageCount               = imageCount;
  initInfo.ImageCount                  = imageCount;
  initInfo.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;
  initInfo.PipelineCache               = VK_NULL_HANDLE;
  initInfo.Subpass                     = 0;
  initInfo.UseDynamicRendering         = true;
  initInfo.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
  initInfo.Allocator                   = nullptr;
  initInfo.CheckVkResultFn             = nullptr;
  initInfo.MinAllocationSize           = 1024 * 1024;

  ImGui_ImplVulkan_Init(&initInfo);
  ImGui_ImplVulkan_CreateFontsTexture();
}

DebugOverlay::~DebugOverlay() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

void DebugOverlay::processEvent(const SDL_Event& event) {
  ImGui_ImplSDL3_ProcessEvent(&event);
}

void DebugOverlay::newFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

void DebugOverlay::buildUI(float deltaTime, size_t quadCount, size_t textureCount) {
  constexpr float smoothing = 0.1f;
  smoothedDeltaTime = smoothedDeltaTime <= 0.0f
    ? deltaTime
    : smoothedDeltaTime + (deltaTime - smoothedDeltaTime) * smoothing;

  ImGui::Begin("Debug");
  ImGui::Text("Frame time: %.3f ms", smoothedDeltaTime * 1000.0f);
  ImGui::Text("FPS: %.1f", smoothedDeltaTime > 0.0f ? 1.0f / smoothedDeltaTime : 0.0f);
  ImGui::Text("Quads: %zu", quadCount);
  ImGui::Text("Textures: %zu", textureCount);
  ImGui::End();

  ImGui::Render();
}

void DebugOverlay::draw(vk::raii::CommandBuffer& commandBuffer) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);
}
