#include "node2D.hpp"

Node2D::Node2D (
  std::vector<std::unique_ptr<Node>> children,
  glm::vec3 position,
  glm::vec2 scale,
  glm::vec1 rotation
) :
  Node(std::move(children)),
  position(position),
  scale(scale),
  rotation(rotation)
{ }

// TODO: Propogate positional and scale changes to children
void Node2D::process(float deltaTime) {
  Node::process(deltaTime);
  // for (std::unique_ptr<Node> &child : children) {
  //   Node2D &node2D = dynamic_cast<Node2D&>(*child);
  // }
}
