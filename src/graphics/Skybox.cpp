#include "graphics/Skybox.h"
#include "core/AssetPath.h"
#include "graphics/OpenGL.h"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <glm/gtc/matrix_inverse.hpp>
#include <stb_image.h>

namespace relay {
Skybox::Skybox(const std::filesystem::path& crossImage)
    : shader_(craft::core::assetPath("shaders/skybox.vert"),
              craft::core::assetPath("shaders/skybox.frag")) {
  int width = 0, height = 0, channels = 0;
  stbi_set_flip_vertically_on_load(0);
  const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
      stbi_load(crossImage.string().c_str(), &width, &height, &channels, 4),
      stbi_image_free);
  if (!pixels) {
    const char* reason = stbi_failure_reason();
    throw std::runtime_error("Skybox image load failed: " + crossImage.string() +
                             " (" + (reason ? reason : "unknown error") + ")");
  }
  if (width <= 0 || height <= 0 || width % 4 || height % 3 || width / 4 != height / 3)
    throw std::runtime_error("Skybox requires a 4x3 horizontal cross with square faces: " + crossImage.string());
  const int faceSize = width / 4;
  GLint maxSize = 0;
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxSize);
  if (faceSize > maxSize) throw std::runtime_error("Skybox face exceeds GPU texture limit");

  // Cross, top-to-bottom: [ ][+Y][ ][ ] / [-X][+Z][+X][-Z] / [ ][-Y][ ][ ].
  // Keep source rows top-to-bottom; cubemap image axes differ from 2D UVs.
  const std::array<glm::ivec2, 6> tiles{{{2,1},{0,1},{1,0},{1,2},{1,1},{3,1}}};
  GLint binding = 0, alignment = 0, rowLength = 0, skipPixels = 0, skipRows = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
  glGenTextures(1, &cubemap_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  for (unsigned face = 0; face < tiles.size(); ++face) {
    const auto tile = tiles[face];
    const auto offset = (std::size_t(tile.y * faceSize) * width + tile.x * faceSize) * 4;
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
                 faceSize, faceSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get() + offset);
  }
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  for (auto axis : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R})
    glTexParameteri(GL_TEXTURE_CUBE_MAP, axis, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
  glBindTexture(GL_TEXTURE_CUBE_MAP, unsigned(binding));
  glGenVertexArrays(1, &vao_);
  std::cout << "skybox_loaded face_size=" << faceSize << " source=" << crossImage.string() << '\n';
}

Skybox::~Skybox() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteTextures(1, &cubemap_);
}

void Skybox::draw(const glm::mat4& viewProjection) const {
  // Preserve the caller's GL state, including the texture unit used by materials.
  GLint program = 0, vao = 0, activeTexture = 0, binding = 0;
  GLboolean writeDepth = GL_TRUE;
  const bool depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE);
  const bool blend = glIsEnabled(GL_BLEND), seamless = glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &writeDepth);
  glActiveTexture(GL_TEXTURE2);
  glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
  glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  shader_.use();
  shader_.setMat4("uInverseViewProjection", glm::inverse(viewProjection));
  shader_.setInt("uSkybox", 2);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_);
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(unsigned(vao));
  glBindTexture(GL_TEXTURE_CUBE_MAP, unsigned(binding));
  glActiveTexture(GLenum(activeTexture));
  glUseProgram(unsigned(program));
  glDepthMask(writeDepth);
  if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
  if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
  if (!seamless) glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}
} // namespace relay
