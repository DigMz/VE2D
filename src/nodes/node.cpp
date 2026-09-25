#include "node.hpp"

#include <memory>
#include <utility>
#include <vector>

Node::Node(
  std::vector<std::unique_ptr<Node>> children
) :
  children(std::move(children))
{}


void Node::init() {
  _init();
  for (std::unique_ptr<Node> &child : children) {
    child->init();
  }
}
void Node::ready() {
  for (std::unique_ptr<Node> &child : children) {
    child->ready();
  }
  _ready();
}
void Node::process(float deltaTime) {
  for (std::unique_ptr<Node> &child : children) {
    child->process(deltaTime);
  }
  _process(deltaTime);
}

// Called before children's init
void Node::_init() {}
// Called after children's ready
void Node::_ready() {}
// Called every frame
void Node::_process(float deltaTime) {}
