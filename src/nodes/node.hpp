#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <utility>

class Node {
public:
  std::vector<std::unique_ptr<Node>> children;

  Node (
    std::vector<std::unique_ptr<Node>> children
  ) : 
    children(std::move(children))
  {}

  void init() {
    std::cout << children.size() << std::endl;
    _init();
    for (std::unique_ptr<Node> &child : children) {
      child->init();
    }
  }
  void ready() {
    for (std::unique_ptr<Node> &child : children) {
      child->ready();
    }
    _ready();
  }
  void process(float deltaTime) {
    for (std::unique_ptr<Node> &child : children) {
      child->process(deltaTime);
    }
    _process(deltaTime);
  }

  template<std::derived_from<Node> T, class... Args>
  void addChild(Args&&... args) {
    auto child = std::make_unique<T>(std::forward<Args>(args)...);
    children.push_back(std::move(child));
  }

private:
  // Called before children's init
  void virtual _init() {}
  // Called after children's ready
  void virtual _ready() {}
  // Called every frame
  void virtual _process(float deltaTime) {}
};
