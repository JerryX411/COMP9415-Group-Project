#pragma once
#include "core/RelayGame.h"
namespace relay {
// Orbit camera: target follows the courier; input movement uses its horizontal basis.
class FollowCamera {
 public:
  void reset(const Game& game);
  void orbit(float pixelsX,float pixelsY);
  void zoom(float wheel);
  void update(const Game& game,float dt);
  glm::vec2 movement(float right,float forward) const;
  glm::vec3 eye() const { return eye_; }
  glm::vec3 target() const { return target_; }
  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float distance() const { return distance_; }
 private:
  glm::vec3 eye_{},target_{};
  float yaw_=.4F,pitch_=.52F,distance_=8.F,actualDistance_=8.F;
};
}
