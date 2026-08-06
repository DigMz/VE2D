#include "node.hpp"

#include <vector>
#include <concepts>
#include <memory>
#include <utility>

Node::Node(
  std::vector<std::unique_ptr<Node>> children
) :
  children(std::move(children))
{}

Node::Node(
  std::string name,
  std::vector<std::unique_ptr<Node>> children
) :
  name(name),
  children(std::move(children))
{}

void Node::init() {
  _init();
  for (std::unique_ptr<Node> &child : children) {
    child->parent = this;
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

template<std::derived_from<Node> T, class... Args>
void Node::addChild(Args&&... args) {
  auto child = std::make_unique<T>(std::forward<Args>(args)...);
  children.push_back(std::move(child));
}

// Called before children's init
void Node::_init() {}
// Called after children's ready
void Node::_ready() {}
// Called every frame
void Node::_process(float deltaTime) {}
