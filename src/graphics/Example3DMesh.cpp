#include "graphics/Example3DMesh.h"

#include <array>

#include "graphics/OpenGL.h"

namespace craft::graphics {

namespace {

// Four coloured triangular faces form a tetrahedron. The example deliberately
// has no indices, normals, UVs, materials, voxel data, or face generation.
constexpr std::array<float, 72> kVertices = {
    // clang-format off
    // position              // colour
    0.0F,   0.8F,  0.0F,     0.95F, 0.35F, 0.30F,
    -0.75F, -0.6F, 0.55F,    0.95F, 0.35F, 0.30F,
    0.75F,  -0.6F, 0.55F,    0.95F, 0.35F, 0.30F,

    0.0F,   0.8F,  0.0F,     0.25F, 0.70F, 0.95F,
    0.75F,  -0.6F, 0.55F,    0.25F, 0.70F, 0.95F,
    0.0F,   -0.6F, -0.75F,   0.25F, 0.70F, 0.95F,

    0.0F,   0.8F,  0.0F,     0.40F, 0.85F, 0.45F,
    0.0F,   -0.6F, -0.75F,   0.40F, 0.85F, 0.45F,
    -0.75F, -0.6F, 0.55F,    0.40F, 0.85F, 0.45F,

    -0.75F, -0.6F, 0.55F,    0.95F, 0.80F, 0.25F,
    0.0F,   -0.6F, -0.75F,   0.95F, 0.80F, 0.25F,
    0.75F,  -0.6F, 0.55F,    0.95F, 0.80F, 0.25F,
    // clang-format on
};

}  // namespace

Example3DMesh::Example3DMesh() {
  glGenVertexArrays(1, &vertexArray_);
  glGenBuffers(1, &vertexBuffer_);

  glBindVertexArray(vertexArray_);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(kVertices.size() * sizeof(float)),
               kVertices.data(), GL_STATIC_DRAW);

  constexpr GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<const void*>(3 * sizeof(float)));
  glBindVertexArray(0);
}

Example3DMesh::~Example3DMesh() {
  if (vertexBuffer_ != 0) {
    glDeleteBuffers(1, &vertexBuffer_);
  }
  if (vertexArray_ != 0) {
    glDeleteVertexArrays(1, &vertexArray_);
  }
}

void Example3DMesh::draw() const {
  glBindVertexArray(vertexArray_);
  glDrawArrays(GL_TRIANGLES, 0, 12);
}

}  // namespace craft::graphics
