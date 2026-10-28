#pragma once
#include <algorithm>
#include "core/RelayGame.h"
namespace relay {
// Flat-bottom vertical cylinder: rounded X/Z corners without a spherical sole.
inline bool cylinderIntersectsBox(glm::vec3 feet,float radius,float height,const Bounds& box) {
  constexpr float skin=.0001F;
  if(feet.y>=box.max.y-skin || feet.y+height<=box.min.y+skin) return false;
  const float x=std::clamp(feet.x,box.min.x,box.max.x);
  const float z=std::clamp(feet.z,box.min.z,box.max.z);
  const auto d=glm::vec2(feet.x-x,feet.z-z);
  return glm::dot(d,d)<(radius-skin)*(radius-skin);
}
}
