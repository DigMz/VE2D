#include "scene_serializer.hpp"

#include "nodes/node2D.hpp"
#include "nodes/sprite.hpp"
#include "utils/utils.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace {

json serializeChildren(const Node& node) {
  json children = json::array();
  for (const std::unique_ptr<Node>& child : node.children) {
    children.push_back(serializeNode(*child));
  }
  return children;
}

} // namespace

json serializeNode(const Node& node) {
  if (const Sprite* sprite = dynamic_cast<const Sprite*>(&node)) {
    return {
      {"type", "Sprite"},
      {"name", sprite->name},
      {"position", {sprite->position.x, sprite->position.y, sprite->position.z}},
      {"scale", {sprite->scale.x, sprite->scale.y}},
      {"rotation", sprite->rotation.x},
      {"texturePath", sprite->texturePath},
      {"children", serializeChildren(node)}
    };
  }
  if (const Node2D* node2D = dynamic_cast<const Node2D*>(&node)) {
    return {
      {"type", "Node2D"},
      {"name", node2D->name},
      {"position", {node2D->position.x, node2D->position.y, node2D->position.z}},
      {"scale", {node2D->scale.x, node2D->scale.y}},
      {"rotation", node2D->rotation.x},
      {"children", serializeChildren(node)}
    };
  }
  return {
    {"type", "Node"},
    {"name", node.name},
    {"children", serializeChildren(node)}
  };
}

std::unique_ptr<Node> deserializeNode(const json& j) {
  std::vector<std::unique_ptr<Node>> children;
  if (j.contains("children")) {
    for (const json& childJson : j.at("children")) {
      children.push_back(deserializeNode(childJson));
    }
  }

  const std::string type = j.value("type", std::string("Node"));
  const std::string name = j.value("name", type);

  if (type == "Node") {
    return std::make_unique<Node>(name, std::move(children));
  }

  glm::vec3 position {
    j.at("position")[0].get<float>(),
    j.at("position")[1].get<float>(),
    j.at("position")[2].get<float>()
  };
  glm::vec2 scale {
    j.at("scale")[0].get<float>(),
    j.at("scale")[1].get<float>()
  };
  glm::vec1 rotation { j.at("rotation").get<float>() };

  if (type == "Sprite") {
    return std::make_unique<Sprite>(
      std::move(children), position, scale, rotation,
      j.value("texturePath", std::string(DEFAULT_TEXTURE_PATH)), name
    );
  }
  if (type == "Node2D") {
    return std::make_unique<Node2D>(std::move(children), position, scale, rotation, name);
  }

  throw std::runtime_error("deserializeNode: unknown node type '" + type + "'");
}

void saveSceneToFile(const Node& root, const std::string& path) {
  std::filesystem::path filePath(path);
  if (filePath.has_parent_path()) {
    std::filesystem::create_directories(filePath.parent_path());
  }

  std::ofstream file(filePath);
  if (!file.is_open()) {
    throw std::runtime_error("saveSceneToFile: failed to open '" + path + "' for writing");
  }
  file << serializeNode(root).dump(2);
}

std::unique_ptr<Node> loadSceneFromFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("loadSceneFromFile: failed to open '" + path + "' for reading");
  }

  json j;
  try {
    file >> j;
  } catch (const json::parse_error& e) {
    throw std::runtime_error("loadSceneFromFile: failed to parse '" + path + "': " + std::string(e.what()));
  }

  return deserializeNode(j);
}
