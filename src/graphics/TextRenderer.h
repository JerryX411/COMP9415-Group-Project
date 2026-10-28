#pragma once

#include <filesystem>
#include <string_view>

#include <glm/vec4.hpp>

#include "graphics/Shader.h"
#include "graphics/Texture.h"

namespace craft::graphics {

// Draws screen-space text with the bitmap font atlas (assets/textures/font.png)
// and solid rectangles. Coordinates are framebuffer pixels with the origin at
// the top-left corner.
//
// The atlas is a 16 x 6 grid of printable ASCII glyphs starting at ' ', where
// every cell is twice as tall as it is wide.
class TextRenderer {
 public:
  TextRenderer(const std::filesystem::path& fontPath,
               const std::filesystem::path& vertexPath,
               const std::filesystem::path& fragmentPath);
  ~TextRenderer();

  TextRenderer(const TextRenderer&) = delete;
  TextRenderer& operator=(const TextRenderer&) = delete;

  // Sets up blending and the pixel projection. Call before any draw.
  void begin(int framebufferWidth, int framebufferHeight);
  // Restores the depth test and blending state used by the 3D scene.
  void end() const;

  // Width in pixels of a line drawn with drawText and the same arguments.
  [[nodiscard]] static float measure(std::string_view text, float glyphWidth,
                                     float letterSpacing = 0.0F);

  // Draws one line of text. glyphWidth is the cell width in pixels (the cell
  // height is twice that), and letterSpacing adds that fraction of a cell
  // between neighbouring glyphs.
  void drawText(std::string_view text, float x, float y, float glyphWidth,
                const glm::vec4& colour, float letterSpacing = 0.0F) const;

  void drawRect(float x, float y, float width, float height,
                const glm::vec4& colour) const;

 private:
  void drawQuads(const float* vertices, int quadCount, const glm::vec4& colour,
                 bool textured) const;

  Shader shader_;
  Texture font_;
  unsigned int vertexArray_ = 0;
  unsigned int vertexBuffer_ = 0;
};

}  // namespace craft::graphics
