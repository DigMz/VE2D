#include "scene.hpp"

#include "nodes/node.hpp"
#include "nodes/sprite.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>

void _funcTree(std::unique_ptr<Node> &root, std::function<void(std::unique_ptr<Node>&)> func) {
  func(root);
  for (std::unique_ptr<Node> &node : root->children) {
    _funcTree(node, func);
  }
}
void Scene::funcTree(std::function<void(std::unique_ptr<Node>&)> func) { _funcTree(root, func); }

void Scene::pushSpriteDataToRenderer(Sprite &sprite) {
  if (!textureIdRef.contains(sprite.texturePath)) {
    textureIdRef[sprite.texturePath] = renderer->addTexture(sprite.texturePath);
  }
  sprite.textureId = textureIdRef[sprite.texturePath];

  sprite.quadIndex = renderer->addQuadObject({
    .position = {sprite.position.x, sprite.position.y, sprite.position.z},
    .rotation = sprite.rotation.x,
    .scale = {sprite.scale.x, sprite.scale.y},
    .color = {1.0f, 1.0f, 1.0f},
    .textureIndex = static_cast<uint32_t>(sprite.textureId)
  });
}

void Scene::updateSpriteDataOnRenderer(Sprite &sprite) {
  if (!textureIdRef.contains(sprite.texturePath)) {
    textureIdRef[sprite.texturePath] = renderer->addTexture(sprite.texturePath);
    sprite.textureId = textureIdRef[sprite.texturePath];
  }

  renderer->updateQuadObject({
    .position = {sprite.position.x, sprite.position.y, sprite.position.z},
    .rotation = sprite.rotation.x,
    .scale = {sprite.scale.x, sprite.scale.y},
    .color = {1.0f, 1.0f, 1.0f},
    .textureIndex = static_cast<uint32_t>(sprite.textureId)
  }, sprite.quadIndex);
}

void Scene::init() {
  std::cout << "Initializing Scene" << std::endl;

  // Go through every node, add unique textures to the renderer, and map its id
  funcTree( [&](std::unique_ptr<Node> &node) -> void {
    try {
      Sprite &sprite = dynamic_cast<Sprite&>(*node);
      pushSpriteDataToRenderer(sprite);
    } catch (const std::bad_cast&) { }
  });

  for (const auto& [tex, id] : textureIdRef) {
    std::cout << "Texture: " << tex << " Id: " << id << std::endl;
  }

  root->init();
  root->ready();
}

void Scene::process(float deltaTime) {
  root->process(deltaTime);

  // Go through every node, add unique textures to the renderer, and map its id
  funcTree( [&](std::unique_ptr<Node> &node) -> void {
    try {
      Sprite &sprite = dynamic_cast<Sprite&>(*node);
      if (sprite.dirty) {
        updateSpriteDataOnRenderer(sprite);
      }
    } catch (const std::bad_cast&) { }
  });
}

void Scene::recordFrame(
  vk::raii::CommandBuffer& commandBuffer,
  vk::Image swapChainImage,
  vk::Extent2D swapChainExtent,
  vk::raii::ImageView& swapChainImageView,
  vk::raii::Image& depthImage,
  vk::raii::ImageView& depthImageView,
  uint32_t frameIndex,
  unsigned int imageIndex
) {
  renderer->recordFrame(
   commandBuffer,
   swapChainImage,
   swapChainExtent,
   swapChainImageView,
   depthImage,
   depthImageView,
   frameIndex,
   imageIndex
  );
}

template<std::derived_from<Node> T, class... Args>
void Scene::addNodeToRoot(Args&&... args) {
  auto child = std::make_unique<T>(std::forward<Args>(args)...);
  root->children.push_back(std::move(child));
}

void Scene::addSprite(std::unique_ptr<Sprite> sprite) {
  pushSpriteDataToRenderer(*sprite);
  root->children.push_back(std::move(sprite));
}
