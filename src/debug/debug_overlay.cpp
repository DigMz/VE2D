#include "debug_overlay.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <stdexcept>

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
  ImGui::GetIO().IniFilename = nullptr; // nothing worth persisting yet (fixed layout)

  ImGui_ImplSDL3_InitForVulkan(window);

  // Dedicated pool for ImGui's own descriptor sets, per imgui_impl_vulkan.h:
  // needs eFreeDescriptorSet, one combined image sampler for the font atlas,
  // plus one per ImGui_ImplVulkan_AddTexture() call (the viewport images).
  vk::DescriptorPoolSize poolSize {
    .type            = vk::DescriptorType::eCombinedImageSampler,
    .descriptorCount = 1 + MAX_VIEWPORT_TEXTURES
  };
  vk::DescriptorPoolCreateInfo poolInfo {
    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    .maxSets       = 1 + MAX_VIEWPORT_TEXTURES,
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
  for (VkDescriptorSet texture : viewportTextures) {
    ImGui_ImplVulkan_RemoveTexture(texture);
  }
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

EditorActions DebugOverlay::buildUI(float deltaTime, size_t quadCount, size_t textureCount, uint32_t frameIndex) {
  constexpr float smoothing = 0.1f;
  smoothedDeltaTime = smoothedDeltaTime <= 0.0f
    ? deltaTime
    : smoothedDeltaTime + (deltaTime - smoothedDeltaTime) * smoothing;

  EditorActions actions;

  constexpr float panelWidth = 260.0f;
  constexpr ImGuiWindowFlags fixedWindowFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

  const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
  const ImVec2 workPos  = mainViewport->WorkPos;
  const ImVec2 workSize = mainViewport->WorkSize;

  // Left: editor controls + stats.
  ImGui::SetNextWindowPos(workPos);
  ImGui::SetNextWindowSize(ImVec2(panelWidth, workSize.y));
  ImGui::Begin("Scene", nullptr, fixedWindowFlags);
  if (ImGui::Button("Add Quad", ImVec2(-FLT_MIN, 0.0f))) {
    actions.addQuad = true;
  }
  ImGui::SeparatorText("Stats");
  ImGui::Text("Frame time: %.3f ms", smoothedDeltaTime * 1000.0f);
  ImGui::Text("FPS: %.1f", smoothedDeltaTime > 0.0f ? 1.0f / smoothedDeltaTime : 0.0f);
  ImGui::Text("Quads: %zu", quadCount);
  ImGui::Text("Textures: %zu", textureCount);
  ImGui::End();

  // Right: the game, rendered offscreen by the Renderer and shown as an image.
  ImGui::SetNextWindowPos(ImVec2(workPos.x + panelWidth, workPos.y));
  ImGui::SetNextWindowSize(ImVec2(std::max(workSize.x - panelWidth, 1.0f), workSize.y));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("Game", nullptr, fixedWindowFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();

  const ImVec2 available = ImGui::GetContentRegionAvail();
  if (available.x >= 1.0f && available.y >= 1.0f && frameIndex < viewportTextures.size()) {
    ImGui::Image((ImTextureID)viewportTextures[frameIndex], available);
  }
  // ImGui works in logical points; the offscreen image should match pixels.
  const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
  actions.viewportSize = vk::Extent2D {
    static_cast<uint32_t>(std::max(available.x * framebufferScale.x, 0.0f)),
    static_cast<uint32_t>(std::max(available.y * framebufferScale.y, 0.0f))
  };
  ImGui::End();

  ImGui::Render();

  return actions;
}

void DebugOverlay::setViewportTextures(vk::Sampler sampler, const std::vector<vk::ImageView>& imageViews) {
  for (VkDescriptorSet texture : viewportTextures) {
    ImGui_ImplVulkan_RemoveTexture(texture);
  }
  viewportTextures.clear();

  if (imageViews.size() > MAX_VIEWPORT_TEXTURES) {
    throw std::runtime_error("DebugOverlay: too many viewport textures for its descriptor pool");
  }
  for (vk::ImageView imageView : imageViews) {
    viewportTextures.push_back(ImGui_ImplVulkan_AddTexture(
      static_cast<VkSampler>(sampler),
      static_cast<VkImageView>(imageView),
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ));
  }
}

void DebugOverlay::draw(vk::raii::CommandBuffer& commandBuffer) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);
}
