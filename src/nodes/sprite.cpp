#include "sprite.hpp"
#include <numbers>

Sprite::Sprite (
  std::vector<std::unique_ptr<Node>> children,
  glm::vec3 position,
  glm::vec2 scale,
  glm::vec1 rotation,
  std::string name
) :
  Node2D(
    std::move(children),
    position,
    scale,
    rotation,
    name
  ),
  lastPosition(position),
  lastScale(scale),
  lastRotation(rotation)
{ }

Sprite::Sprite (
  std::vector<std::unique_ptr<Node>> children,
  glm::vec3 position,
  glm::vec2 scale,
  glm::vec1 rotation,
  std::string texturePath,
  std::string name
) :
  Node2D(
    std::move(children),
    position,
    scale,
    rotation,
    name
  ),
  lastPosition(position),
  lastScale(scale),
  lastRotation(rotation),
  texturePath(texturePath),
  lastTexturePath(texturePath)
{ }

void Sprite::_init() {
  std::cout << "Init CanvasItem: " << texturePath << std::endl;
}

void Sprite::_ready() {
  std::cout << "Ready CanvasItem: " << texturePath << std::endl;
}

void Sprite::_process(float deltaTime) {
  if (lastPosition != global_position ||
      lastScale != global_scale       ||
      lastRotation != global_rotation ||
      lastTexturePath != texturePath
  ) { dirty = true; }

  lastPosition = global_position;
  lastScale = global_scale;
  lastRotation = global_rotation;
  lastTexturePath = texturePath;
}

