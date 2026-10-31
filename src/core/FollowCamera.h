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
  // Shake mostly moves the aim point, so the eye stays clear of walls; the impact
  // kick moves both and springs back.
  glm::vec3 eye() const { return eye_ + shake_ * .35F + kick_ + drift_; }
  glm::vec3 target() const { return target_ + shake_ + kick_ * .4F; }
  // Up vector with the shake roll applied about the view axis.
  glm::vec3 up() const;
  // Vertical field of view in degrees, punched in on pickups and out on hits.
  float fov() const { return 58.F + fovKick_ + fovSprint_; }
  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float distance() const { return distance_; }
 private:
  glm::vec3 eye_{},target_{};
  float yaw_=.4F,pitch_=.52F,distance_=8.F,actualDistance_=8.F;
  // Trauma rises on hits, pickups, heavy landings and shutdown, then decays; shake is its square.
  float trauma_=0,shakeClock_=0,lastHurt_=0,lastLandAge_=10,lastCelebrate_=10,lastStepAge_=10,lastLaunchAge_=10,lastSlap_=10,lastHitAge_=10,lastSlideAge_=10,lastPopAge_=10,lastBoostAge_=10,lastBounceAge_=10,lastTakeoffAge_=10,lastTrickAge_=10,lastShieldBreak_=10,lastJetAge_=10,lastPushAge_=10,roll_=0;
  bool wasLost_=false;
  // Tunnel shot weight (0 normal view, 1 lowest shot behind the courier).
  float tunnel_=0;
  // Lowering is steady over tunnelTime; coming back up is quick (tunnelReturn).
  static constexpr float tunnelTime=1.2F,tunnelReturn=.1F;
  // Mouse tilt inside the shot, and seconds since the player last turned the view.
  float tunnelLook_=0,orbitAge_=10;
  // Seconds after leaving a gap during which the wall just left does not hold the view back.
  float exitHold_=0;
  // Extra pitch on a hole exit, so the raised view sees over the block just left.
  float exitLift_=0;
  glm::vec3 shake_{0};
  // Impact springs: positional kick and field-of-view punch, both settling to zero.
  glm::vec3 kick_{0},kickVelocity_{0};
  float fovKick_=0,fovVelocity_=0,rollKick_=0,rollVelocity_=0,fovSprint_=0;
  // Handheld drift and running sway move only the eye, so the aim stays on the courier.
  glm::vec3 drift_{0};
};
}
