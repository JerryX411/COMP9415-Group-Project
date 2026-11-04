#pragma once
#include <filesystem>
#include <glm/mat4x4.hpp>
#include "graphics/Shader.h"

namespace relay {
// Loads a horizontal 4x3 cross into a real six-face OpenGL cubemap.
// The fullscreen triangle reconstructs view rays; no world geometry is added.
class Skybox {
 public:
  explicit Skybox(const std::filesystem::path& crossImage);
  ~Skybox();
  Skybox(const Skybox&) = delete;
  Skybox& operator=(const Skybox&) = delete;
  // Call after clearing the framebuffer and before drawing scene geometry/UI.
  // Translation cancels between near/far points, keeping the sky at infinity.
  void draw(const glm::mat4& viewProjection) const;
 private:
  craft::graphics::Shader shader_;
  unsigned cubemap_ = 0, vao_ = 0;
};
} // namespace relay
