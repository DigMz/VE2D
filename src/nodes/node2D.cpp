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

glm::vec3 Node2D::get_global_position() {
  if (Node2D* node = dynamic_cast<Node2D*>(parent)) {
    return global_position;
  } else { return position;}
}
glm::vec2 Node2D::get_global_scale() {
  if (Node2D* node = dynamic_cast<Node2D*>(parent)) {
    return global_scale;
  } else { return scale; }
}
glm::vec1 Node2D::get_global_rotation() {
  if (Node2D* node = dynamic_cast<Node2D*>(parent)) {
    return global_rotation;
  } else { return rotation; }
}

glm::vec3 rotate_position(glm::vec3 position, glm::vec1 rotation) {
  return {
    position.x * cosf(rotation.x) - position.y * sinf(rotation.x),
    position.x * sinf(rotation.x) + position.y * cosf(rotation.x),
    position.z
  };
}

void Node2D::process(float deltaTime) {
  // std::cout << name << std::endl;
  Node::process(deltaTime);
  for (std::unique_ptr<Node> &child : children) {
    Node2D &node2D = dynamic_cast<Node2D&>(*child);
    glm::vec3 rotated_position = rotate_position(node2D.position, node2D.rotation);
    node2D.global_position = this->global_position + glm::vec3({
      rotated_position.x * this->global_scale.x,
      rotated_position.y * this->global_scale.y,
      node2D.position.z
    });
    node2D.global_scale = {
      this->global_scale.x * node2D.scale.x,
      this->global_scale.y * node2D.scale.y
    };
    node2D.global_rotation = this->global_rotation + node2D.rotation;
  }
}

