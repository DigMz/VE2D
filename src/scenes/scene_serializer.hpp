#pragma once

#include "nodes/node.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

nlohmann::json serializeNode(const Node& node);
std::unique_ptr<Node> deserializeNode(const nlohmann::json& j);

// Throws std::runtime_error on I/O failure.
void saveSceneToFile(const Node& root, const std::string& path);
// Throws std::runtime_error on I/O or parse failure.
std::unique_ptr<Node> loadSceneFromFile(const std::string& path);
