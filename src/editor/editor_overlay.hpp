#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan.hpp;
#endif

#include <SDL3/SDL.h>

// What the user did in the editor UI this frame, for Application to act on.
struct EditorActions {
  bool         addQuad      = false;
  vk::Extent2D viewportSize = {0, 0}; // pixel size of the "Game" panel's image area
  bool         viewportHovered = false; // mouse is over the "Game" panel
};

// Owns the entire Dear ImGui lifecycle (context + SDL3 + Vulkan backends) so
// the rest of the engine never needs to know ImGui exists.
//
// Draws a Godot-style editor layout: a fixed side panel on the left with
// controls/stats, and a "Game" panel on the right showing the scene, which the
// Renderer draws into an offscreen image (one per frame in flight).
class EditorOverlay {
public:
  EditorOverlay(
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
  );
  ~EditorOverlay();

  EditorOverlay(const EditorOverlay&) = delete;
  EditorOverlay& operator=(const EditorOverlay&) = delete;

  void processEvent(const SDL_Event& event);
  void newFrame();
  EditorActions buildUI(float deltaTime, size_t quadCount, size_t textureCount, uint32_t frameIndex);
  void draw(vk::raii::CommandBuffer& commandBuffer);

  // Registers the Renderer's viewport images (indexed by frame in flight) as
  // ImGui textures, replacing any previous ones. The GPU must be idle.
  void setViewportTextures(vk::Sampler sampler, const std::vector<vk::ImageView>& imageViews);

private:
  uint32_t                     viewportTextureCount; // pool capacity for ImGui_ImplVulkan_AddTexture()
  vk::raii::DescriptorPool     descriptorPool = nullptr;
  std::vector<VkDescriptorSet> viewportTextures;
  float smoothedDeltaTime = 0.0f; // EMA, so the displayed FPS isn't noisy frame-to-frame
};
