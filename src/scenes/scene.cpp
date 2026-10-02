#include "scene.hpp"

#include "nodes/node.hpp"
#include "nodes/node2D.hpp"
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
    .position = {-sprite.get_global_position().x, sprite.get_global_position().y, -sprite.get_global_position().z},
    .rotation = sprite.get_global_rotation().x,
    .scale = {sprite.get_global_scale().x, sprite.get_global_scale().y},
    .color = {1.0f, 1.0f, 1.0f},
    .textureIndex = static_cast<uint32_t>(sprite.textureId)
  });
}

void Scene::updateSpriteDataOnRenderer(Sprite &sprite) {
  if (!textureIdRef.contains(sprite.texturePath)) {
    textureIdRef[sprite.texturePath] = renderer->addTexture(sprite.texturePath);
  }
  sprite.textureId = textureIdRef[sprite.texturePath];

  renderer->updateQuadObject({
    .position = {-sprite.get_global_position().x, sprite.get_global_position().y, -sprite.get_global_position().z},
    .rotation = -sprite.get_global_rotation().x,
    .scale = {sprite.get_global_scale().x, sprite.get_global_scale().y},
    .color = {1.0f, 1.0f, 1.0f},
    .textureIndex = static_cast<uint32_t>(sprite.textureId)
  }, sprite.quadIndex);
}

void Scene::init() {
  std::cout << "Initializing Scene Tree" << std::endl;

  // Parent pointers must exist, and global transforms must be computed,
  // before we read global position/scale/rotation to push initial data.
  root->init();
  updateNode2DTransforms(*root);

  // Go through every node, add unique textures to the renderer, and map its id
  funcTree( [&](std::unique_ptr<Node> &node) -> void {
    if (Sprite* sprite = dynamic_cast<Sprite*>(node.get())) {
      pushSpriteDataToRenderer(*sprite);
    }
  });

  for (const auto& [tex, id] : textureIdRef) {
    std::cout << "Texture: " << tex << " Id: " << id << std::endl;
  }

  root->ready();
}

void Scene::process(float deltaTime) {
  root->process(deltaTime);
  updateNode2DTransforms(*root);

  // Go through every node, add unique textures to the renderer, and map its id
  funcTree( [&](std::unique_ptr<Node> &node) -> void {
    if (Sprite* sprite = dynamic_cast<Sprite*>(node.get())) {
      if (sprite->dirty) {
        updateSpriteDataOnRenderer(*sprite);
        sprite->dirty = false;
      }
    }
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
  unsigned int imageIndex,
  std::function<void(vk::raii::CommandBuffer&)> overlayDraw,
  bool sceneToViewport
) {
  renderer->recordFrame(
   commandBuffer,
   swapChainImage,
   swapChainExtent,
   swapChainImageView,
   depthImage,
   depthImageView,
   frameIndex,
   imageIndex,
   overlayDraw,
   sceneToViewport
  );
}

void Scene::addSprite(std::unique_ptr<Sprite> sprite) {
  pushSpriteDataToRenderer(*sprite);
  root->children.push_back(std::move(sprite));
}
