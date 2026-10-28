#pragma once
#include <glm/glm.hpp>

namespace relay {
// Presentation-only signals. None delays input or changes the collision shape.
struct CourierMotion {
  float startAge=10, stopAge=10, pivotAge=10, launchAge=10, landAge=10;
  float celebrateAge=10, victoryAge=0, idleFor=0;
  float stopStrength=0, landStrength=0, lastSpeed=0;
  float lean=0, leanVelocity=0, bank=0, bankVelocity=0;
  float lookYaw=0, lookVelocity=0, antenna=0, antennaVelocity=0;
  // Last hit in the courier's frame: +side pushed it right, +forward pushed it ahead.
  // hitKind 0 is blunt, 1 electric; shockAge drives the electrocution jitter.
  float hitSide=0, hitForward=0, hitAge=10, shockAge=10;
  int hitKind=0;
  float verticalVelocity=0;
  float yawVelocity=0, torsoYaw=0, torsoVelocity=0, turnWeight=0;
  float wakeTime=-1;
  // Footfall pulse: time since the last plant, its weight (sprint > walk > crouch) and side.
  float stepAge=10, stepStrength=0, stepSide=0;
  // Shutdown topple: tilt angle toward the push (rad), its angular speed, time since the body slapped flat.
  float fallAngle=0, fallSpin=0, slapAge=10;
  // Time since the courier beamed in at a checkpoint (drives the materialise effect).
  float spawnAge=10;
  // Time since the last slide began (drop-in squash, camera punch).
  float slideAge=10;
  // Time since standing out of a slide (pop into the stride) and since a speed-pad launch.
  float popAge=10, boostAge=10;
  // Wall bounce: time since the courier rebounded off a wall, and how hard it hit.
  float bounceAge=10, bounceStrength=0;
  // Speed pads: which launch fired last (0 stride, 1 flight, 2 slide), time since a flight took
  // off and since it touched down (a dive into a slide counts as a touchdown).
  int boostKind=0;
  float takeoffAge=10, touchAge=10;
  // Slide surge, 1 at its start fading to 0: a boosted slide or a pad-boosted turbo slide.
  float surge=0;
  // Power-ups: time since one was picked up, and since a shield burst absorbing a hit.
  float pickupAge=10, shieldBreakAge=10;
  // Tricks between moves: 1 a barrel roll (flight into slide), 2 a front flip (slide into
  // flight, boosted jumps), 3 a spin (run or stride into flight), 4 a kip-up (slide into a run),
  // 5 a spinning kip-up (the same, boosted), 6 a scissor-kick twist (jump out of a slide), 7 a
  // corkscrew (boosted jump out of a slide), 8 a wall kick (boosted wall jump); trickAge is the
  // time since it began.
  int trickKind=0;
  float trickAge=10;
  // Menu and workshop only: head pitch toward a point of interest, greeting weight.
  float lookPitch=0, wave=0;
  bool movingIntent=false, victorious=false;
};

inline void motionSpring(float& value,float& velocity,float target,float frequency,float damping,float dt) {
  // Called by the <= 1/120-second simulation, never by rendering.
  velocity+=(frequency*frequency*(target-value)-2*damping*frequency*velocity)*dt;
  value+=velocity*dt;
}
}
