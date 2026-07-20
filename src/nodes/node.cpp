#include "node.hpp"

void Node::init() {
  for (Node component : components) {
    component.init();
  }
  _init();
}

void Node::process() {
  for (Node component : components) {
    component.process();
  }
  _process();
}
