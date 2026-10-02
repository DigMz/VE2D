#include "node2D.hpp"
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>

Node2D::Node2D () :
  Node({}),
  position(0.0f),
  scale(1.0f),
  rotation(0.0f),
  global_position(position),
  global_scale(scale),
  global_rotation(rotation)
{}

Node2D::Node2D (
  std::vector<std::unique_ptr<Node>> children,
  glm::vec3 position,
  glm::vec2 scale,
  glm::vec1 rotation,
  std::string name
) :
  Node(name, std::move(children)),
  position(position),
  scale(scale),
  rotation(rotation),
  global_position(position),
  global_scale(scale),
  global_rotation(rotation)
{ }

glm::vec3 Node2D::get_global_position() { return global_position; }
glm::vec2 Node2D::get_global_scale() { return global_scale; }
glm::vec1 Node2D::get_global_rotation() { return global_rotation; }

glm::vec3 rotate_position(glm::vec3 position, glm::vec1 rotation) {
  return {
    position.x * cosf(rotation.x) - position.y * sinf(rotation.x),
    position.x * sinf(rotation.x) + position.y * cosf(rotation.x),
    position.z
  };
}

void Node2D::updateTransform() {
  if (Node2D* parent2D = dynamic_cast<Node2D*>(parent)) {
    global_rotation = parent2D->global_rotation + rotation;
    glm::vec3 rotated_position = rotate_position(position, global_rotation);
    global_position = parent2D->global_position + glm::vec3({
      rotated_position.x * parent2D->global_scale.x,
      rotated_position.y * parent2D->global_scale.y,
      position.z
    });
    global_scale = {
      parent2D->global_scale.x * scale.x,
      parent2D->global_scale.y * scale.y
    };
  } else {
    global_position = position;
    global_scale = scale;
    global_rotation = rotation;
  }

  // Recurse after updating our own global transform, so children see a fresh parent global
  for (std::unique_ptr<Node>& child : children) {
    updateNode2DTransforms(*child);
  }
}

void updateNode2DTransforms(Node& node) {
  if (Node2D* node2D = dynamic_cast<Node2D*>(&node)) {
    node2D->updateTransform();
  } else {
    for (std::unique_ptr<Node>& child : node.children) {
      updateNode2DTransforms(*child);
    }
  }
}

