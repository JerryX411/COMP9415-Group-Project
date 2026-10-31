#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "core/RelayGame.h"

namespace relay {

// Flat is an unbevelled cube for large architectural panels.
// Blob is a low-poly sphere for small soft effects (dust, motes, halos).
enum class Primitive : int { Box, Sphere, Cylinder, Flat, Blob };
// Opaque parts draw first; soft effects follow without writing depth.
enum class Blend : int { Opaque, Alpha, Additive };

struct Instance {
  glm::mat4 transform{1.0F};
  glm::vec4 colour{1.0F};
  // x: metal, y: emission, z: specular strength, w: texture repetition.
  // Emission above 1 is unlit glow; x then softens sphere edges instead.
  glm::vec4 material{0.0F, 0.0F, .12F, 1.0F};
  Primitive primitive = Primitive::Box;
  Blend blend = Blend::Opaque;
};

struct ModelNode {
  std::string name;
  int parent = -1;
  glm::vec3 offset{}, rotation{}, dimensions{1.0F}, colour{1.0F};
  float emission = 0.0F;
  bool visible = true;
  Primitive primitive = Primitive::Box;
};

// Mesh dimensions are applied only when drawing a node, never inherited by
// siblings or children. Joint transforms carry translation and rotation.
class ArticulatedModel {
 public:
  int add(ModelNode node);
  std::vector<glm::mat4> transforms(const glm::mat4& root) const;
  void append(const glm::mat4& root, std::vector<Instance>& output) const;
  std::vector<ModelNode> nodes;
  float scale = 1.0F;  // Uniform scale about the root, applied before the first node.
};

class SceneModels {
 public:
  SceneModels();
  void append(const Game& game, std::vector<Instance>& output, float wakeTime = -1);
  void appendGallery(float time, std::vector<Instance>& output, int motion = 0);
  static Player previewPlayer(float time, int motion);
  // Render-only squash and stretch about the feet; collision is unchanged.
  static glm::vec3 bodyScale(const Player& player);
  // Render-only: the running hop above the ground and the topple after shutdown.
  static float bodyHop(const Player& player);
  static glm::mat4 fallTransform(const Player& player);
  void appendMenu(float time, std::vector<Instance>& output, glm::vec2 look = {});
  void poseRobot(const Player& player);
  const ArticulatedModel& robot() const { return robot_; }
  const ArticulatedModel& gate() const { return gate_; }
 private:
  void appendGate(glm::vec3 position, float openness, bool unlocked,
                  std::vector<Instance>& output);
  void appendCore(glm::vec3 position, float time, std::vector<Instance>& output,
                  float floorHeight = 1.02F);
  void appendSwitch(glm::vec3 position, bool active, float press,
                    std::vector<Instance>& output);
  void appendCheckpoint(const Checkpoint& checkpoint, float time, std::vector<Instance>& output);
  void appendSpeedPad(const SpeedPad& pad, float time, std::vector<Instance>& output);
  void appendDrainTile(const DrainTile& tile, float time, std::vector<Instance>& output);
  void appendPlate(const PressurePlate& plate, float doorHold, float time, std::vector<Instance>& output);
  void appendDoor(const BlastDoor& door, float time, std::vector<Instance>& output);
  void appendPickup(const Pickup& pickup, float time, float respawnTime, std::vector<Instance>& output);
  void appendPortal(const PortalLink& link, float time, std::vector<Instance>& output);
  void appendFacility(const Game& game, std::vector<Instance>& output);
  ArticulatedModel robot_, gate_;
  int hipL_ = 0, hipR_ = 0, shoulderL_ = 0, shoulderR_ = 0;
  int head_ = 0, body_ = 0, doorL_ = 0, doorR_ = 0, lamp_ = 0;
  int elbowL_ = 0, elbowR_ = 0, kneeL_ = 0, kneeR_ = 0, ankleL_ = 0, ankleR_ = 0;
  int handR_ = 0;
  int packLight_=0;
  int pelvis_=0, eyeL_=0, eyeR_=0, browL_=0, browR_=0, antenna_=0, antennaSocket_=0, handL_=0, pack_=0;
  std::array<int,6> fingers_{};
  std::array<int,4> smileEyes_{};
  std::array<int,2> glints_{}, cheeks_{};
  std::array<int,3> mouth_{};
  std::vector<ModelNode> bindPose_;
  glm::vec2 menuLook_{0};
  float menuLookTime_ = -1;
};

void appendBox(std::vector<Instance>& output, glm::vec3 position,
               glm::vec3 dimensions, glm::vec3 colour,
               float emission = 0, float metal = 0, float angle = 0);
// Unlit, soft-edged additive sphere for halos, flashes and motes.
void appendGlow(std::vector<Instance>& output, glm::vec3 position, glm::vec3 size,
                glm::vec3 colour, float alpha, float intensity = 1);

}  // namespace relay
