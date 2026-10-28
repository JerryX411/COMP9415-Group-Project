#pragma once
#include <vector>
#include "graphics/RelayModels.h"
#include "graphics/Shader.h"
#include "core/FollowCamera.h"

namespace relay {
struct RenderOptions {
  bool normalMapping = true, optimised = true, whitebox = false;
  bool gallery = false, diagnostics = false;
  bool menu = false, overview = false;
  bool collisionOverlay = false;
  int motionPreview = 0;
  float wakeTime = -1;
};
struct RenderStats {
  int voxels = 0, totalFaces = 0, visibleFaces = 0;
  int submittedTriangles = 0, instances = 0, drawCalls = 0;
};
class Renderer {
 public:
  Renderer();
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  // Geometry preparation is outside the measured render boundary.
  void prepare(const Game& game, const RenderOptions& options, float galleryTime);
  void draw(int width, int height, const RenderOptions& options, const FollowCamera& camera);
  glm::vec2 project(glm::vec3 point, float width, float height) const;
  const RenderStats& stats() const { return stats_; }
 private:
  void rebuildTerrain(const Game& game, bool optimised, bool whitebox);
  craft::graphics::Shader shader_;
  SceneModels models_;
  std::vector<Instance> instances_;
  unsigned cubeVao_ = 0, cubeBuffer_ = 0, instanceBuffer_ = 0;
  unsigned terrainVao_ = 0, terrainBuffer_ = 0, terrainInstance_ = 0;
  unsigned albedo_ = 0, normal_ = 0;
  int terrainVertices_ = 0, revision_ = -1;
  int modelVertices_ = 0;
  std::array<int,3> meshStart_{}, meshCount_{}, instanceStart_{}, instanceCount_{};
  glm::vec3 playerPosition_{};
  const Game* sceneGame_ = nullptr;
  bool lastOptimised_ = false, lastWhitebox_ = false;
  glm::mat4 viewProjection_{1};
  RenderStats stats_;
};
}
