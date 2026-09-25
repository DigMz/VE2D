#pragma once

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

class Node {
public:
  std::vector<std::unique_ptr<Node>> children;

  Node(std::vector<std::unique_ptr<Node>> children);
  virtual ~Node() = default;

  Node(Node&&) = default;
  Node& operator=(Node&&) = default;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  void virtual init();
  void virtual ready();
  void virtual process(float deltaTime);

  template<std::derived_from<Node> T, class... Args>
  void addChild(Args&&... args) {
    children.push_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

private:
  void virtual _init(); // Called before children's init
  void virtual _ready(); // Called after children's ready
  void virtual _process(float deltaTime); // Called every frame
};
