#include "core/FollowCamera.h"
#include <algorithm>
#include <cmath>
namespace relay {
void FollowCamera::reset(const Game& game) {
  yaw_=.4F; pitch_=.52F; distance_=actualDistance_=8;
  target_=game.player().position+glm::vec3(0,1.1F,0);
  update(game,1.F);
}
void FollowCamera::orbit(float x,float y) {
  yaw_=std::remainder(yaw_-x*.0016F,6.2831853F);
  pitch_=std::clamp(pitch_+y*.0016F,.12F,1.22F);
}
void FollowCamera::zoom(float wheel) { distance_=std::clamp(distance_-wheel*.7F,3.5F,13.F); }
glm::vec2 FollowCamera::movement(float right,float forward) const {
  return glm::vec2(std::cos(yaw_),-std::sin(yaw_))*right-glm::vec2(std::sin(yaw_),std::cos(yaw_))*forward;
}
void FollowCamera::update(const Game& game,float dt) {
  const auto& a=game.player().motion;
  const float impact=a.landAge<.65F?std::sin(std::min(1.F,a.landAge/.32F)*3.14159265F)*std::exp(-a.landAge*3.F)*a.landStrength:0;
  const float brake=a.stopAge<.5F?std::sin(std::min(1.F,a.stopAge/.5F)*3.14159265F)*a.stopStrength:0;
  const auto desired=game.player().position+glm::vec3(0,1.1F-.15F*impact-.035F*brake,0);
  target_=glm::mix(target_,desired,1.F-std::exp(-18.F*std::max(0.F,dt)));
  // If the smoothed target would be inside terrain, catch up immediately.
  if(game.collidesWithScene({target_-glm::vec3(.18F),target_+glm::vec3(.18F)})) target_=desired;
  const glm::vec3 direction(std::sin(yaw_)*std::cos(pitch_),std::sin(pitch_),std::cos(yaw_)*std::cos(pitch_));
  float clear=distance_;
  for(float d=.15F;d<=distance_+.05F;d+=.1F) {
    auto p=target_+direction*std::min(d,distance_);
    const bool hit=game.collidesWithScene({p-glm::vec3(.20F),p+glm::vec3(.20F)});
    if(hit) { clear=std::max(.05F,d-.15F); break; }
  }
  // Retract immediately at walls, extend smoothly after the obstruction clears.
  actualDistance_=clear<actualDistance_?clear:glm::mix(actualDistance_,clear,1.F-std::exp(-9.F*std::max(0.F,dt)));
  eye_=target_+direction*actualDistance_;
}
}
