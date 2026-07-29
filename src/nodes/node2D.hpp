#pragma once

#include "node.hpp"
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <memory>
#include <vector>

class Node2D : public Node {
public:
  Node2D (
    std::vector<std::unique_ptr<Node>> children,
    glm::vec3 position,
    glm::vec2 scale,
    glm::vec1 rotation
  );

  void process(float deltaTime) override;

  glm::vec3 position;
  glm::vec2 scale;
  glm::vec1 rotation;
};
