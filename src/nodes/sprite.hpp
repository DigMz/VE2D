#pragma once

#include "node2D.hpp"
#include "utils/utils.hpp"
#include <cstdint>

class Sprite : public Node2D {
public:
  Sprite (
    std::vector<std::unique_ptr<Node>> children,
    glm::vec3 position,
    glm::vec2 scale,
    glm::vec1 rotation,
    std::string name = "Sprite"
  );

  Sprite (
    std::vector<std::unique_ptr<Node>> children,
    glm::vec3 position,
    glm::vec2 scale,
    glm::vec1 rotation,
    std::string texturePath,
    std::string name = "Sprite"
  );

  void _init() override;
  void _ready() override;
  void _process(float deltaTime, bool temporal) override;
  void updateTransform() override;

  std::string texturePath = DEFAULT_TEXTURE_PATH;
  uint32_t textureId;
  bool dirty = false;
  int quadIndex = -1;

private:
  std::string lastTexturePath;
};

