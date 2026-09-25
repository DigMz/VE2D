#pragma once

#include "node2D.hpp"
#include <cstdint>
#include <string>

class Sprite : public Node2D {
public:
  Sprite (
    std::vector<std::unique_ptr<Node>> children,
    glm::vec3 position,
    glm::vec2 scale,
    float rotation
  );

  Sprite (
    std::vector<std::unique_ptr<Node>> children,
    glm::vec3 position,
    glm::vec2 scale,
    float rotation,
    std::string texturePath
  );

  void _init() override;
  void _ready() override;
  void _process(float deltaTime) override;

  std::string texturePath = "assets/textures/texture.jpg";
  uint32_t textureId = 0;
  bool dirty = false;
  int quadIndex = -1;

private:
  glm::vec3 lastPosition;
  glm::vec2 lastScale;
  float lastRotation;
  std::string lastTexturePath;
};

