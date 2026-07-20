#pragma once

class Node {
public:
  void init();
  void process();
private:
  std::vector<Node> components;
  void _init();
  void _process();
};
