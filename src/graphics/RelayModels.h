#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "core/RelayGame.h"

namespace relay {

enum class Primitive : int { Box, Sphere, Cylinder };

struct Instance {
  glm::mat4 transform{1.0F};
  glm::vec4 colour{1.0F};
  // x: metal, y: emission, z: specular strength, w: texture repetition.
  glm::vec4 material{0.0F, 0.0F, .12F, 1.0F};
  Primitive primitive = Primitive::Box;
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
};

class SceneModels {
 public:
  SceneModels();
  void append(const Game& game, std::vector<Instance>& output, float wakeTime = -1);
  void appendGallery(float time, std::vector<Instance>& output, int motion = 0);
  static Player previewPlayer(float time, int motion);
  void appendMenu(float time, std::vector<Instance>& output);
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
  void appendFacility(const Game& game, std::vector<Instance>& output);
  ArticulatedModel robot_, gate_;
  int hipL_ = 0, hipR_ = 0, shoulderL_ = 0, shoulderR_ = 0;
  int head_ = 0, body_ = 0, doorL_ = 0, doorR_ = 0, lamp_ = 0;
  int elbowL_ = 0, elbowR_ = 0, kneeL_ = 0, kneeR_ = 0, ankleL_ = 0, ankleR_ = 0;
  int handR_ = 0;
  int pelvis_=0, eyeL_=0, eyeR_=0, browL_=0, browR_=0, antenna_=0, handL_=0;
  std::array<int,6> fingers_{};
  std::array<int,4> smileEyes_{};
  std::vector<ModelNode> bindPose_;
};

void appendBox(std::vector<Instance>& output, glm::vec3 position,
               glm::vec3 dimensions, glm::vec3 colour,
               float emission = 0, float metal = 0, float angle = 0);

}  // namespace relay
