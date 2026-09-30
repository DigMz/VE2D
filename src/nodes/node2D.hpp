#pragma once

#include "node.hpp"
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <memory>
#include <vector>

class Node2D : public Node {
public:
  Node2D ();

  Node2D (
    std::vector<std::unique_ptr<Node>> children,
    glm::vec3 position,
    glm::vec2 scale,
    glm::vec1 rotation,
    std::string name = "Node2D"
  );

  void updateTransform();

  glm::vec3 position;
  glm::vec2 scale;
  glm::vec1 rotation;


  glm::vec3 get_global_position();
  glm::vec2 get_global_scale();
  glm::vec1 get_global_rotation();

private:
  glm::vec3 global_position;
  glm::vec2 global_scale;
  glm::vec1 global_rotation;
};

// Walks the subtree rooted at `node`, updating every Node2D found (skipping
// through non-Node2D nodes to keep reaching further Node2D descendants).
void updateNode2DTransforms(Node& node);
