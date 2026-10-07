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
  )
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
  texturePath(texturePath),
  lastTexturePath(texturePath)
{ }

void Sprite::_init() {
  std::cout << "Init CanvasItem: " << texturePath << std::endl;
}

void Sprite::_ready() {
  std::cout << "Ready CanvasItem: " << texturePath << std::endl;
}

void Sprite::_process(float deltaTime, bool temporal) {
  if (temporal) rotation += 3.14159 * deltaTime;

  if (lastTexturePath != texturePath) { dirty = true; }
  lastTexturePath = texturePath;
}

void Sprite::updateTransform() {
  Node2D::updateTransform();
  if (transformDirty) dirty = true;
}

