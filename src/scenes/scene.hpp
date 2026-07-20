#pragma once

#include "renderer/renderer.hpp"
#include "utils/vulkan_utils.hpp"
#include <cstdint>

class Scene {
public:
  void process(float deltaTime);
private:
  void init();
  Renderer* renderer = nullptr;
  std::vector<QuadObject> objects;
  uint32_t addObject(QuadObject);
  void removeObject(uint32_t);
};
