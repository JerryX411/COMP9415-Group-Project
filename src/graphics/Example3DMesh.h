#pragma once

namespace craft::graphics {

// Owns one small tetrahedron used only to prove that the starter renders 3D.
// It is not a voxel, block, chunk, or assignment mesh abstraction.
class Example3DMesh {
 public:
  Example3DMesh();
  ~Example3DMesh();

  Example3DMesh(const Example3DMesh&) = delete;
  Example3DMesh& operator=(const Example3DMesh&) = delete;

  void draw() const;

 private:
  unsigned int vertexArray_ = 0;
  unsigned int vertexBuffer_ = 0;
};

}  // namespace craft::graphics
