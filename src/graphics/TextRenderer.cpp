#include "graphics/TextRenderer.h"

#include <array>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>

#include "graphics/OpenGL.h"

namespace craft::graphics {

namespace {

constexpr int kAtlasColumns = 16;
constexpr int kAtlasRows = 8;  // The bottom two rows of the atlas are empty.
constexpr char kFirstGlyph = ' ';
constexpr char kLastGlyph = '~';

// Two triangles per quad, each vertex holding position (x, y) and UV (u, v).
constexpr int kFloatsPerVertex = 4;
constexpr int kFloatsPerQuad = 6 * kFloatsPerVertex;

void appendQuad(std::vector<float>& out, float x0, float y0, float x1, float y1,
                float u0, float v0, float u1, float v1) {
  const std::array<float, kFloatsPerQuad> quad = {
      // clang-format off
      x0, y0, u0, v0,   x1, y0, u1, v0,   x1, y1, u1, v1,
      x0, y0, u0, v0,   x1, y1, u1, v1,   x0, y1, u0, v1,
      // clang-format on
  };
  out.insert(out.end(), quad.begin(), quad.end());
}

}  // namespace

TextRenderer::TextRenderer(const std::filesystem::path& fontPath,
                           const std::filesystem::path& vertexPath,
                           const std::filesystem::path& fragmentPath)
    : shader_(vertexPath, fragmentPath), font_(fontPath) {
  glGenVertexArrays(1, &vertexArray_);
  glGenBuffers(1, &vertexBuffer_);

  glBindVertexArray(vertexArray_);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
  constexpr GLsizei stride = kFloatsPerVertex * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<const void*>(2 * sizeof(float)));
  glBindVertexArray(0);
}

TextRenderer::~TextRenderer() {
  if (vertexBuffer_ != 0) {
    glDeleteBuffers(1, &vertexBuffer_);
  }
  if (vertexArray_ != 0) {
    glDeleteVertexArrays(1, &vertexArray_);
  }
}

void TextRenderer::begin(int framebufferWidth, int framebufferHeight) {
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  shader_.use();
  // Flipping top and bottom puts the origin at the top-left, like the atlas.
  shader_.setMat4("uProjection",
                  glm::ortho(0.0F, static_cast<float>(framebufferWidth),
                             static_cast<float>(framebufferHeight), 0.0F));
  shader_.setInt("uFont", 0);
  font_.bind(0);
}

void TextRenderer::end() const {
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

float TextRenderer::measure(std::string_view text, float glyphWidth,
                            float letterSpacing) {
  if (text.empty()) {
    return 0.0F;
  }
  const auto count = static_cast<float>(text.size());
  return count * glyphWidth + (count - 1.0F) * letterSpacing * glyphWidth;
}

void TextRenderer::drawText(std::string_view text, float x, float y,
                            float glyphWidth, const glm::vec4& colour,
                            float letterSpacing) const {
  constexpr float cellU = 1.0F / kAtlasColumns;
  constexpr float cellV = 1.0F / kAtlasRows;
  const float glyphHeight = 2.0F * glyphWidth;
  const float advance = glyphWidth * (1.0F + letterSpacing);

  std::vector<float> vertices;
  vertices.reserve(text.size() * kFloatsPerQuad);
  int quadCount = 0;
  for (const char c : text) {
    if (c > kFirstGlyph && c <= kLastGlyph) {
      const int index = c - kFirstGlyph;
      const float u0 = static_cast<float>(index % kAtlasColumns) * cellU;
      const float v0 = static_cast<float>(index / kAtlasColumns) * cellV;
      appendQuad(vertices, x, y, x + glyphWidth, y + glyphHeight, u0, v0,
                 u0 + cellU, v0 + cellV);
      ++quadCount;
    }
    x += advance;
  }

  drawQuads(vertices.data(), quadCount, colour, true);
}

void TextRenderer::drawRect(float x, float y, float width, float height,
                            const glm::vec4& colour) const {
  std::vector<float> vertices;
  appendQuad(vertices, x, y, x + width, y + height, 0.0F, 0.0F, 0.0F, 0.0F);
  drawQuads(vertices.data(), 1, colour, false);
}

void TextRenderer::drawQuads(const float* vertices, int quadCount,
                             const glm::vec4& colour, bool textured) const {
  if (quadCount == 0) {
    return;
  }
  shader_.setVec4("uColour", colour);
  shader_.setFloat("uUseTexture", textured ? 1.0F : 0.0F);

  glBindVertexArray(vertexArray_);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(quadCount * kFloatsPerQuad *
                                       sizeof(float)),
               vertices, GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, quadCount * 6);
  glBindVertexArray(0);
}

}  // namespace craft::graphics
