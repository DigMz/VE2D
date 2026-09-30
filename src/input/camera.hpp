#pragma once

#include <glm/glm.hpp>

// Plain camera state. Application turns it into the view/projection UBO each
// frame; InputHandler moves it around.
struct Camera {
  glm::vec3 position = {0.0f, 0.0f, 2.0f};
  glm::vec3 front    = {0.0f, 0.0f, -1.0f};
  glm::vec3 up       = {0.0f, -1.0f, 0.0f};
  float     fov      = 45.0f; // vertical, degrees

  glm::vec3 right() const { return glm::normalize(glm::cross(front, up)); }
};
