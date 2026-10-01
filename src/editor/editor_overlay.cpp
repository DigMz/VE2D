#include "editor_overlay.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <stdexcept>

EditorOverlay::EditorOverlay(
  SDL_Window* window,
  vk::raii::Instance& instance,
  vk::raii::PhysicalDevice& physicalDevice,
  vk::raii::Device& device,
  uint32_t queueFamily,
  vk::raii::Queue& queue,
  vk::Format colorFormat,
  vk::Format depthFormat,
  uint32_t imageCount,
  uint32_t viewportTextureCount
) : viewportTextureCount(viewportTextureCount) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr; // nothing worth persisting yet (fixed layout)

  ImGui_ImplSDL3_InitForVulkan(window);

  // Dedicated pool for ImGui's own descriptor sets, per imgui_impl_vulkan.h:
  // needs eFreeDescriptorSet, separate sampled-image descriptors for the font
  // atlas (IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE) plus one per
  // ImGui_ImplVulkan_AddTexture() call (the viewport images), and sampler
  // descriptors for the backend's own samplers.
  std::array<vk::DescriptorPoolSize, 2> poolSizes {{
    { .type = vk::DescriptorType::eSampledImage,
      .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + viewportTextureCount },
    { .type = vk::DescriptorType::eSampler,
      .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
  }};
  vk::DescriptorPoolCreateInfo poolInfo {
    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    .maxSets       = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + viewportTextureCount
                   + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
    .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
    .pPoolSizes    = poolSizes.data()
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
  initInfo.ApiVersion                                   = VK_API_VERSION_1_4;
  initInfo.Instance                                     = *instance;
  initInfo.PhysicalDevice                               = *physicalDevice;
  initInfo.Device                                       = *device;
  initInfo.QueueFamily                                  = queueFamily;
  initInfo.Queue                                        = *queue;
  initInfo.DescriptorPool                               = *descriptorPool;
  initInfo.MinImageCount                                = imageCount;
  initInfo.ImageCount                                   = imageCount;
  initInfo.PipelineCache                                = VK_NULL_HANDLE;
  initInfo.PipelineInfoMain.RenderPass                  = VK_NULL_HANDLE;
  initInfo.PipelineInfoMain.Subpass                     = 0;
  initInfo.PipelineInfoMain.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
  initInfo.UseDynamicRendering                          = true;
  initInfo.Allocator                                    = nullptr;
  initInfo.CheckVkResultFn                              = nullptr;
  initInfo.MinAllocationSize                            = 1024 * 1024;

  // The font atlas is uploaded by the backend on the first frame
  // (ImGuiBackendFlags_RendererHasTextures), so no explicit font upload here.
  ImGui_ImplVulkan_Init(&initInfo);
}

EditorOverlay::~EditorOverlay() {
  for (VkDescriptorSet texture : viewportTextures) {
    ImGui_ImplVulkan_RemoveTexture(texture);
  }
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

void EditorOverlay::processEvent(const SDL_Event& event) {
  ImGui_ImplSDL3_ProcessEvent(&event);
}

void EditorOverlay::newFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

EditorActions EditorOverlay::buildUI(float deltaTime, size_t quadCount, size_t textureCount, uint32_t frameIndex) {
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
  actions.viewportHovered = ImGui::IsWindowHovered();
  ImGui::End();

  ImGui::Render();

  return actions;
}

void EditorOverlay::setViewportTextures(vk::Sampler sampler, const std::vector<vk::ImageView>& imageViews) {
  for (VkDescriptorSet texture : viewportTextures) {
    ImGui_ImplVulkan_RemoveTexture(texture);
  }
  viewportTextures.clear();

  if (imageViews.size() > viewportTextureCount) {
    throw std::runtime_error("EditorOverlay: too many viewport textures for its descriptor pool");
  }
  for (vk::ImageView imageView : imageViews) {
    viewportTextures.push_back(ImGui_ImplVulkan_AddTexture(
      static_cast<VkSampler>(sampler),
      static_cast<VkImageView>(imageView),
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ));
  }
}

void EditorOverlay::draw(vk::raii::CommandBuffer& commandBuffer) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);
}
