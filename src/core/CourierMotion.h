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
  float hitSide=0;
  float verticalVelocity=0;
  float yawVelocity=0, torsoYaw=0, torsoVelocity=0, turnWeight=0;
  float wakeTime=-1;
  bool movingIntent=false, victorious=false;
};

inline void motionSpring(float& value,float& velocity,float target,float frequency,float damping,float dt) {
  // Called by the <= 1/120-second simulation, never by rendering.
  velocity+=(frequency*frequency*(target-value)-2*damping*frequency*velocity)*dt;
  value+=velocity*dt;
}
}
