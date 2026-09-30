#pragma once

#include "renderer/renderer.hpp"
#include "nodes/node.hpp"
#include "nodes/sprite.hpp"
#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

class Scene {
public:
  Scene(
    std::unique_ptr<Node> root,
    std::unique_ptr<Renderer> renderer
  ):
    root(std::move(root)),
    renderer(std::move(renderer))
  {
    init();
  }
  void process(float deltaTime);
  void recordFrame(
    vk::raii::CommandBuffer& commandBuffer,
    vk::Image swapChainImage,
    vk::Extent2D swapChainExtent,
    vk::raii::ImageView& swapChainImageView,
    vk::raii::Image& depthImage,
    vk::raii::ImageView& depthImageView,
    uint32_t frameIndex,
    unsigned int imageIndex,
    std::function<void(vk::raii::CommandBuffer&)> overlayDraw = nullptr
  );

  void funcTree(std::function<void(std::unique_ptr<Node>&)> func);
  void pushSpriteDataToRenderer(Sprite &sprite);
  void updateSpriteDataOnRenderer(Sprite &sprite);

  size_t getQuadCount() const { return renderer->getQuadCount(); }
  size_t getTextureCount() const { return renderer->getTextureCount(); }

  template<std::derived_from<Node> T, class... Args>
  void addNodeToRoot(Args&&... args) {
    root->children.push_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

  void addSprite(std::unique_ptr<Sprite> canvasItem);

  void printDebug() {
    renderer->printDebug();
  }

private:
  std::unordered_map<std::string, uint32_t> textureIdRef = {
    {"assets/textures/texture.jpg", 0}
  };
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<Node> root;
  void init();
};
