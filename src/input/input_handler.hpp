#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan.hpp;
#endif

#include <SDL3/SDL.h>

#include "input/camera.hpp"

// One-shot actions triggered by keys this frame, for Application to act on.
struct InputActions {
  bool toggleEditor = false; // F1
  bool closeEditor  = false; // Escape while the editor is open
  bool addQuad      = false; // Q
};

// Mouse and keyboard game controls: camera movement (WASD/Space/Ctrl held keys,
// middle-drag pan, scroll-wheel zoom), mouse capture, and the key shortcuts
// reported through InputActions. ImGui input is handled by EditorOverlay.
class InputHandler {
public:
  explicit InputHandler(SDL_Window* window);

  // Clears last frame's InputActions. Call before polling events.
  void beginFrame();
  void processEvent(const SDL_Event& event, Camera& camera);
  // Applies held-key camera movement.
  void update(float deltaTime, Camera& camera);

  // Pixel size of whatever the game is drawn into (window or editor "Game"
  // panel), and whether the mouse is over it. Camera controls only start
  // while it's hovered.
  void setGameView(vk::Extent2D extent, bool hovered);
  // An open editor needs a free cursor; closing it recaptures the mouse.
  void setEditorOpen(bool open);

  const InputActions& getActions() const { return actions; }

private:
  SDL_Window*  window;
  InputActions actions;

  bool         editorOpen      = false;
  bool         mouseCaptured   = true;
  bool         panning         = false; // middle mouse held, started over the game view
  bool         gameViewHovered = true;
  vk::Extent2D gameViewExtent  = {0, 0};

  float cameraSpeed       = 3.0f; // units/sec
  float cameraZoomStep    = 0.1f; // fraction of the distance to z = 0 covered per scroll notch
  float cameraMinDistance = 0.2f;
  float cameraMaxDistance = 50.0f;

  void setMouseCaptured(bool captured);
  void panCamera(Camera& camera, float dx, float dy);
  void zoomCamera(Camera& camera, float wheel);
};
