#include "input_handler.hpp"

#include <algorithm>
#include <cmath>

InputHandler::InputHandler(SDL_Window* window): window(window) {
  setMouseCaptured(true);
}

void InputHandler::beginFrame() {
  actions = {};
}

void InputHandler::processEvent(const SDL_Event& event, Camera& camera) {
  switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      // Only start a pan over the game view, so middle-clicks on editor panels are left to ImGui
      if (event.button.button == SDL_BUTTON_MIDDLE && gameViewHovered) panning = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (event.button.button == SDL_BUTTON_MIDDLE) panning = false;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if (panning) panCamera(camera, event.motion.xrel, event.motion.yrel);
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      if (gameViewHovered) {
        zoomCamera(camera, event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y);
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (event.key.scancode == SDL_SCANCODE_F1) {
        actions.toggleEditor = true;
      }
      if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
        if (editorOpen) {
          actions.closeEditor = true;
        } else {
          setMouseCaptured(!mouseCaptured);
        }
      }
      if (event.key.scancode == SDL_SCANCODE_Q) {
        actions.addQuad = true;
      }
      break;
    default:
      break;
  }
}

void InputHandler::update(float deltaTime, Camera& camera) {
  if (!gameViewHovered) return;

  const bool *keys = SDL_GetKeyboardState(nullptr);
  float velocity = cameraSpeed * deltaTime;

  glm::vec3 right = camera.right();

  if (keys[SDL_SCANCODE_W]) camera.position += camera.up * velocity;
  if (keys[SDL_SCANCODE_S]) camera.position -= camera.up * velocity;
  if (keys[SDL_SCANCODE_A]) camera.position -= right * velocity;
  if (keys[SDL_SCANCODE_D]) camera.position += right * velocity;
  if (keys[SDL_SCANCODE_SPACE])    camera.position -= camera.front * velocity;
  if (keys[SDL_SCANCODE_LCTRL])    camera.position += camera.front * velocity;
}

void InputHandler::setGameView(vk::Extent2D extent, bool hovered) {
  gameViewExtent  = extent;
  gameViewHovered = hovered;
}

void InputHandler::setEditorOpen(bool open) {
  editorOpen = open;
  setMouseCaptured(!open);
}

void InputHandler::setMouseCaptured(bool captured) {
  mouseCaptured = captured;
  SDL_SetWindowRelativeMouseMode(window, mouseCaptured);
}

// Grab-style pan: the point under the cursor on the z = 0 plane (where the
// sprites live) follows the cursor. dx/dy are mouse deltas in window
// coordinates. Assumes the camera looks straight down -z, as it always does now.
void InputHandler::panCamera(Camera& camera, float dx, float dy) {
  float pixelDensity = SDL_GetWindowPixelDensity(window);
  if (pixelDensity <= 0.0f) pixelDensity = 1.0f;
  float viewHeight = gameViewExtent.height / pixelDensity;
  if (viewHeight <= 0.0f) return;

  float distance            = std::max(camera.position.z, cameraMinDistance);
  float unitsPerWindowPixel = 2.0f * distance * std::tan(glm::radians(camera.fov) * 0.5f) / viewHeight;

  camera.position -= camera.right() * (dx * unitsPerWindowPixel);
  camera.position += camera.up      * (dy * unitsPerWindowPixel);
}

// Moves the camera along z. Each notch covers a fixed fraction of the distance
// to z = 0, so zooming feels the same near and far and never passes the plane.
void InputHandler::zoomCamera(Camera& camera, float wheel) {
  float distance    = std::max(camera.position.z, cameraMinDistance);
  float newDistance = std::clamp(distance * std::pow(1.0f - cameraZoomStep, wheel), cameraMinDistance, cameraMaxDistance);
  camera.position += camera.front * (distance - newDistance);
}
