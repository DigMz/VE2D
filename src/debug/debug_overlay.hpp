#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan.hpp;
#endif

#include <SDL3/SDL.h>

// Owns the entire Dear ImGui lifecycle (context + SDL3 + Vulkan backends) so
// the rest of the engine never needs to know ImGui exists.
class DebugOverlay {
public:
  DebugOverlay(
    SDL_Window* window,
    vk::raii::Instance& instance,
    vk::raii::PhysicalDevice& physicalDevice,
    vk::raii::Device& device,
    uint32_t queueFamily,
    vk::raii::Queue& queue,
    vk::Format colorFormat,
    vk::Format depthFormat,
    uint32_t imageCount
  );
  ~DebugOverlay();

  DebugOverlay(const DebugOverlay&) = delete;
  DebugOverlay& operator=(const DebugOverlay&) = delete;

  void processEvent(const SDL_Event& event);
  void newFrame();
  void buildUI(float deltaTime, size_t quadCount, size_t textureCount);
  void draw(vk::raii::CommandBuffer& commandBuffer);

private:
  vk::raii::DescriptorPool descriptorPool = nullptr;
  float smoothedDeltaTime = 0.0f; // EMA, so the displayed FPS isn't noisy frame-to-frame
};
