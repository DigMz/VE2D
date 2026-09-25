#include "sprite.hpp"
#include <iostream>
#include <numbers>

Sprite::Sprite (
  std::vector<std::unique_ptr<Node>> children,
  glm::vec3 position,
  glm::vec2 scale,
  float rotation
) :
  Node2D(
    std::move(children),
    position,
    scale,
    rotation
  ),
  lastPosition(position),
  lastScale(scale),
  lastRotation(rotation)
{ }

Sprite::Sprite (
  std::vector<std::unique_ptr<Node>> children,
  glm::vec3 position,
  glm::vec2 scale,
  float rotation,
  std::string texturePath
) :
  Node2D(
    std::move(children),
    position,
    scale,
    rotation
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
  rotation += std::numbers::pi_v<float> * deltaTime;

  if (lastPosition != position ||
      lastScale != scale       ||
      lastRotation != rotation ||
      lastTexturePath != texturePath
  ) { dirty = true; }

  lastPosition = position;
  lastScale = scale;
  lastRotation = rotation;
  lastTexturePath = texturePath;
}

