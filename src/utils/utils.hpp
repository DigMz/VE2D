#pragma once

#include <cmath>
#include <fstream>
#include <glm/ext/vector_float2.hpp>

const std::string DEFAULT_TEXTURE_PATH = "assets/textures/default.png";

static std::vector<char> readFile(const std::string& filename) {
  // std::cout << std::filesystem::current_path() << std::endl;
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  return buffer;
}

static std::array<float, 2> rotate_vector(std::array<float, 2> vector, float rotation) {
  return {
    vector[0] * cosf(rotation) - vector[1] * sinf(rotation),
    vector[0] * sinf(rotation) + vector[1] * cosf(rotation)
  };
}

static glm::vec2 rotate_vector(glm::vec2 vector, glm::vec1 rotation) {
  return {
    vector.x * cosf(rotation.x) - vector.y * sinf(rotation.x),
    vector.x * sinf(rotation.x) + vector.y * cosf(rotation.x)
  };
}
