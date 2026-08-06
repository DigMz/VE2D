#pragma once

#include <concepts>
#include <memory>

class Node {
public:
  std::string name = "Node";
  std::vector<std::unique_ptr<Node>> children;

  Node(std::vector<std::unique_ptr<Node>> children);
  Node(std::string name, std::vector<std::unique_ptr<Node>> children);

  void virtual init();
  void virtual ready();
  void virtual process(float deltaTime);

  template<std::derived_from<Node> T, class... Args>
  void addChild(Args&&... args);

protected:
  Node* parent = nullptr;

  void virtual _init(); // Called before children's init
  void virtual _ready(); // Called after children's ready
  void virtual _process(float deltaTime); // Called every frame
};
