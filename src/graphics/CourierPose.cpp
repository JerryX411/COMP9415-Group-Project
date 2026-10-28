#include "graphics/RelayModels.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace relay {
namespace {
constexpr float pi=3.14159265359F;
float ease(float x) { x=std::clamp(x,0.F,1.F); return x*x*(3-2*x); }
struct Key { float time,value; };
float curve(float time,std::initializer_list<Key> keys) {
  auto a=keys.begin(); if(time<=a->time) return a->value;
  for(auto b=a+1;b!=keys.end();a=b++) if(time<=b->time)
    return glm::mix(a->value,b->value,ease((time-a->time)/(b->time-a->time)));
  return a->value;
}
struct Foot { float z=0,lift=0,pitch=0; };
Foot gait(float phase,float sprint) {
  const float cycle=phase/(2*pi)-std::floor(phase/(2*pi));
  const float stance=glm::mix(.34F,.26F,sprint);
  const float stride=2*pi/glm::mix(3.8F,2.6F,sprint),reach=stride*stance*.5F;
  if(cycle<stance) {
    const float t=cycle/stance;
    return {glm::mix(-reach,reach,t),0,curve(t,{{0,.12F},{.18F,0},{.75F,0},{1,-.18F}})};
  }
  const float t=(cycle-stance)/(1-stance);
  const float lift=std::sin(pi*t);
  return {glm::mix(reach,-reach,ease(t)),lift*lift*glm::mix(.13F,.18F,sprint),curve(t,{{0,-.18F},{.3F,-.26F},{.8F,.18F},{1,.12F}})};
}
float ankleHeight(const Foot& foot) {
  // Rotate about the ankle while keeping the entire boot sole above its target.
  return foot.lift+.155F*std::cos(foot.pitch)+(foot.pitch>=0?.125F:.275F)*std::abs(std::sin(foot.pitch))+.006F;
}
}

void SceneModels::poseRobot(const Player& p) {
  robot_.nodes=bindPose_;
  auto& n=robot_.nodes; const auto& a=p.motion;
  const float move=std::clamp(p.moveBlend,0.F,1.F),air=p.airBlend,run=p.sprintBlend;
  const float ground=1-air, gaitWeight=move*ground;
  const float start=curve(a.startAge,{{0,0},{.055F,1},{.18F,.15F},{.3F,0}})*ground;
  const float brake=curve(a.stopAge,{{0,0},{.07F,1},{.13F,1},{.25F,.3F},{.39F,-.18F},{.54F,0}})*a.stopStrength*ground;
  const float pivot=curve(a.pivotAge,{{0,0},{.075F,1},{.23F,.4F},{.4F,0}})*ground;
  const float hard=std::clamp((a.landStrength-.75F)/.8F,0.F,1.F);
  const float landTime=a.landAge/(1+.4F*hard);
  const float land=curve(landTime,{{0,0},{.045F,1},{.105F,1},{.20F,.42F},{.30F,-.12F},{.44F,0}})*a.landStrength*ground;
  const float launch=curve(a.launchAge,{{0,0},{.05F,1},{.12F,.15F},{.20F,0}});
  const float joy=curve(a.celebrateAge,{{0,0},{.19F,0},{.34F,1},{.55F,.8F},{.85F,0}})*(1-p.hitBlend);
  const float death=ease(p.deathTime/1.05F);
  const float inspect=curve(std::fmod(std::max(0.F,a.idleFor-2),9.F),{{0,0},{.5F,1},{1.5F,1},{2.1F,0},{9,0}})*(1-move)*ground;
  const float scan=std::sin(std::max(0.F,a.idleFor-4)*1.3F)*ease(a.idleFor-4)*(1-move);
  const float victorious=a.victorious?ease(a.victoryAge/.22F):0;
  const float cheer=std::sin(a.victoryAge*8.F)*victorious;

  Foot left=gait(p.walkPhase,run),right=gait(p.walkPhase+pi,run);
  for(auto* foot:{&left,&right}) { foot->z*=gaitWeight; foot->lift*=gaitWeight; foot->pitch*=gaitWeight; }
  // Upward tuck, apex hang, then extend the feet to meet the ground.
  const float rise=ease((a.verticalVelocity+1)/7.F),fall=ease(-a.verticalVelocity/7.F);
  left.z=glm::mix(left.z,-.15F-.08F*rise,air); right.z=glm::mix(right.z,.09F+.08F*rise,air);
  left.lift=glm::mix(left.lift,.08F+.14F*rise-.04F*fall,air);
  right.lift=glm::mix(right.lift,.13F+.11F*rise-.08F*fall,air);
  left.pitch=glm::mix(left.pitch,-.16F*rise,air); right.pitch=glm::mix(right.pitch,-.22F*rise,air);
  const float stopStep=curve(a.stopAge,{{0,0},{.12F,.10F},{.29F,.03F},{.4F,0}})*a.stopStrength*ground;
  right.lift+=stopStep; left.z-=.045F*brake; right.z+=.045F*brake;
  left.z-=pivot*.05F; right.lift+=pivot*.06F;
  if(a.victorious) { left.lift+=std::max(0.F,cheer)*.09F; right.lift+=std::max(0.F,-cheer)*.09F; }
  left.z=glm::mix(left.z,-.22F,death); right.z=glm::mix(right.z,-.22F,death);
  left.lift=glm::mix(left.lift,.015F,death); right.lift=glm::mix(right.lift,.015F,death);
  left.pitch*=1-death; right.pitch*=1-death;

  float pelvis=.64F-.095F*gaitWeight+.025F*std::sin(2*p.walkPhase)*gaitWeight;
  pelvis-=.13F*land+.055F*start+.075F*brake+.07F*launch;
  if(a.wakeTime>=0) pelvis-=.17F*(1-ease((a.wakeTime-.65F)/1.1F));
  pelvis+=.025F*joy+.015F*std::sin(p.idlePhase*2.2F)*(1-move)*ground;
  pelvis=glm::mix(pelvis,.40F,death);
  for(const auto& foot:{left,right}) pelvis=std::min(pelvis,ankleHeight(foot)+std::sqrt(std::max(.01F,.49F*.49F-foot.z*foot.z)));
  n[pelvis_].offset.y=pelvis;
  n[pelvis_].offset.x=std::sin(p.walkPhase)*(.022F+.012F*run)*gaitWeight;

  // Analytic two-link IK: planted ankle targets, forward-pointing knees and
  // independent ankle roll. Both hips inherit the same moving pelvis.
  const auto leg=[&](int hip,int knee,int ankle,const Foot& foot) {
    const float down=pelvis-ankleHeight(foot);
    const float distance=std::clamp(std::hypot(down,foot.z),.08F,.49F);
    const float bend=-std::acos(std::clamp((distance*distance-.125F)/.125F,-1.F,1.F));
    const float swing=std::atan2(-foot.z,down)-std::atan2(.25F*std::sin(bend),.25F+.25F*std::cos(bend));
    n[hip].rotation.x=swing; n[knee].rotation.x=bend; n[ankle].rotation.x=foot.pitch-swing-bend;
  };
  leg(hipL_,kneeL_,ankleL_,left); leg(hipR_,kneeR_,ankleR_,right);
  n[hipL_].rotation.y=-a.torsoYaw*.32F;
  n[hipR_].rotation.y=-a.torsoYaw*.32F;
  n[ankleL_].rotation.y=a.torsoYaw*.2F; n[ankleR_].rotation.y=a.torsoYaw*.2F;

  const float swing=std::sin(p.walkPhase)*gaitWeight;
  const float bank=std::clamp(a.bank,-.13F,.13F);
  n[body_].rotation={std::clamp(a.lean,-.28F,.22F)-.07F*land-.08F*start+.16F*brake-.06F*inspect,
    std::sin(p.walkPhase)*(.075F+.035F*run)*gaitWeight+a.torsoYaw,bank+std::sin(p.walkPhase)*.035F*gaitWeight};
  n[body_].rotation.x=glm::mix(n[body_].rotation.x,-.08F*rise+.10F*fall,air);
  n[head_].rotation={-n[body_].rotation.x*.5F-.16F*inspect,std::clamp(a.lookYaw,-.6F,.6F)-a.torsoYaw*.5F+scan*.32F+.16F*inspect,-bank*.65F+.10F*inspect};
  n[head_].offset.y+=std::sin(p.walkPhase-.55F)*.012F*gaitWeight;
  n[head_].rotation.x+=std::sin(2*p.walkPhase-.5F)*.024F*gaitWeight;
  n[shoulderL_].rotation={-.55F*swing+.10F*start,0,.03F+.07F*air};
  n[shoulderR_].rotation={.55F*swing+.10F*start,0,-.03F-.07F*air};
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,.6F*rise-.15F*fall,air);
  n[shoulderL_].rotation.x+=std::sin(2*p.walkPhase-.4F)*.07F*run*gaitWeight;
  n[shoulderR_].rotation.x-=std::sin(2*p.walkPhase+.4F)*.07F*run*gaitWeight;
  n[elbowL_].rotation.x=.18F+.38F*run+.16F*std::max(0.F,swing);
  n[elbowR_].rotation.x=.18F+.38F*run+.16F*std::max(0.F,-swing);
  n[handL_].rotation.z=.10F*swing; n[handR_].rotation.z=-.10F*swing;
  n[shoulderL_].rotation.x+=.28F*brake+.18F*land;
  n[shoulderR_].rotation.x+=.28F*brake+.18F*land;
  n[shoulderR_].rotation.x=glm::mix(n[shoulderR_].rotation.x,.80F,inspect);
  n[elbowR_].rotation.x=glm::mix(n[elbowR_].rotation.x,.25F,inspect);
  n[handR_].rotation.z+=inspect*.2F;
  float grasp=0;
  if(p.interactionTime>0) {
    const float t=1-p.interactionTime/.46F;
    const float reach=curve(t,{{0,0},{.12F,-.15F},{.40F,1},{.61F,1},{1,0}});
    grasp=curve(t,{{0,0},{.35F,0},{.54F,1},{.8F,1},{1,0}});
    n[body_].rotation.x-=reach*.09F; n[head_].rotation.x-=reach*.14F;
    n[shoulderR_].rotation.x=glm::mix(n[shoulderR_].rotation.x,.88F,std::max(0.F,reach));
    n[elbowR_].rotation.x=glm::mix(n[elbowR_].rotation.x,-.34F,std::max(0.F,reach));
    n[shoulderL_].rotation.x-=reach*.18F;
  }
  n[shoulderR_].rotation.x=glm::mix(n[shoulderR_].rotation.x,1.62F,joy);
  n[elbowR_].rotation.x=glm::mix(n[elbowR_].rotation.x,1.35F,ease(joy*2));
  n[head_].rotation.z-=joy*.10F; n[body_].rotation.x+=joy*.06F;
  if(a.victorious) {
    for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,1.40F+.12F*cheer,victorious);
    for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,1.35F,ease(victorious*2));
    n[head_].rotation.z+=cheer*.10F; n[body_].rotation.z+=cheer*.04F;
  }
  n[body_].rotation.x+=p.hitBlend*.24F; n[body_].rotation.z+=p.hitBlend*a.hitSide*.11F;
  n[head_].rotation.x-=p.hitBlend*.10F;
  n[shoulderL_].rotation.x-=p.hitBlend*.3F; n[shoulderR_].rotation.x-=p.hitBlend*.2F;
  for(int i=0;i<6;++i) n[fingers_[i]].rotation.x=-(.15F*run+(i>=3?grasp*.85F+joy*.7F:0));
  n[antenna_].rotation={a.antenna,0,-bank*.8F+std::sin(p.walkPhase-.6F)*.055F*gaitWeight};

  const float blinkPhase=std::fmod(p.idlePhase+.4F,3.7F);
  const float blink=curve(blinkPhase,{{0,1},{.065F,.08F},{.115F,.08F},{.20F,1},{3.7F,1}});
  const float happy=std::max(joy,victorious)*(1-death)*(1-p.hitBlend);
  for(int eye:{eyeL_,eyeR_}) {
    n[eye].dimensions.y=(.085F+.025F*air-.023F*run)*blink*(1-.75F*p.hitBlend);
    n[eye].offset.x+=std::clamp(a.lookYaw,-.4F,.4F)*.035F;
    n[eye].visible=happy<.55F;
    n[eye].rotation.z=(eye==eyeL_?1.F:-1.F)*p.hitBlend*.35F;
  }
  n[browL_].rotation.z=.15F*run-.12F*happy; n[browR_].rotation.z=-.15F*run+.12F*happy;
  for(int i=0;i<4;++i) { n[smileEyes_[i]].visible=happy>=.55F; n[smileEyes_[i]].rotation.z=i%2==0?.42F:-.42F; }
  if(a.wakeTime>=0) {
    const float t=a.wakeTime,stand=ease((t-.65F)/1.1F);
    const float look=curve(t,{{0,0},{1.3F,0},{1.7F,.32F},{2.15F,-.28F},{2.65F,0},{4,0}});
    const float stretch=curve(t,{{0,0},{1.1F,0},{1.55F,1},{1.9F,1},{2.5F,0},{4,0}});
    n[body_].rotation.x=-.25F*(1-stand);
    n[head_].rotation.x=-.38F*(1-ease((t-.5F)/.65F));
    n[head_].rotation.y=look;
    n[shoulderL_].rotation.x=.26F*stretch; n[shoulderR_].rotation.x=.34F*stretch;
    n[elbowL_].rotation.x=.4F*stretch; n[elbowR_].rotation.x=.5F*stretch;
    const float power=ease((t-.3F)/.4F);
    for(std::size_t i=0;i<n.size();++i) if(bindPose_[i].emission>0) {
      n[i].emission=bindPose_[i].emission*power;
      n[i].colour=glm::mix(glm::vec3(.035F,.05F,.06F),bindPose_[i].colour,power);
    }
    for(int eye:{eyeL_,eyeR_}) n[eye].dimensions.y=bindPose_[eye].dimensions.y*(.12F+.88F*ease((t-.35F)/.45F));
  }
  if(p.health<=0||p.deathTime>0) {
    n[body_].rotation=glm::mix(n[body_].rotation,glm::vec3(-.60F,0,0),death);
    n[head_].rotation=glm::mix(n[head_].rotation,glm::vec3(-.32F,0,.07F),death);
    for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation=glm::mix(n[shoulder].rotation,glm::vec3(.40F,0,0),death);
    for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,.32F,death);
    for(std::size_t i=0;i<n.size();++i) if(bindPose_[i].emission>0) {
      n[i].emission=bindPose_[i].emission*(1-death);
      n[i].colour=glm::mix(bindPose_[i].colour,glm::vec3(.075F,.13F,.16F),death);
    }
  }
}

Player SceneModels::previewPlayer(float time,int motion) {
  Player p; p.position={0,0,0}; p.yaw=3.6F; p.idlePhase=time;
  const float cycle=std::fmod(time,2.4F);
  if(motion==0||motion==1) {
    p.moveBlend=1; p.sprintBlend=motion==1?1.F:0.F; p.motion.lean=motion==1?-.22F:-.13F;
    p.walkPhase=time*(motion==1?Game::sprintSpeed*2.6F:Game::speed*3.8F);
  } else if(motion==2) {
    const float t=std::fmod(time,1.6F);
    constexpr float touchdown=9.6F/14.F;
    p.motion.launchAge=t;
    if(t<touchdown) { p.position.y=std::max(0.F,9.6F*t-14.F*t*t); p.velocity.y=9.6F-28.F*t; p.airBlend=1-std::exp(-20.F*t); p.grounded=false; }
    else { p.motion.landAge=t-touchdown; p.motion.landStrength=1; p.airBlend=std::exp(-20.F*(t-touchdown)); p.velocity.y=-9.6F*std::exp(-18.F*(t-touchdown)); }
  } else if(motion==3) {
    p.motion.stopAge=cycle; p.motion.stopStrength=1; p.moveBlend=1-ease(cycle/.25F); p.walkPhase=2.1F;
    p.motion.lean=-.22F*(1-ease(cycle/.12F));
  } else if(motion==4) {
    p.moveBlend=1; p.walkPhase=time*24.F; p.motion.pivotAge=cycle; p.motion.bank=.13F*std::sin(time*3);
    const float rotate=curve(cycle,{{0,0},{.25F,0},{.70F,1.5708F},{1.15F,1.5708F},{1.65F,3.14159F},{2.4F,3.14159F}});
    p.motion.lookYaw=.45F*std::sin(time*3); p.motion.torsoYaw=.20F*std::sin(time*3); p.yaw+=rotate+std::floor(time/2.4F)*pi;
  } else if(motion==5) { p.motion.idleFor=time+2; }
  else if(motion==6) {
    if(cycle<.46F) { p.interactionTime=.46F-cycle; p.interaction=Interaction::Core; }
    if(cycle>.23F) p.motion.celebrateAge=cycle-.23F;
  } else if(motion==7) {
    p.hitBlend=curve(cycle,{{0,0},{.05F,1},{.25F,1},{.65F,0},{2.4F,0}});
    p.airBlend=p.hitBlend*.65F; p.motion.hitSide=.8F; p.position.y=std::sin(std::min(cycle/.65F,1.F)*pi)*.3F;
  } else if(motion==8) { p.motion.victorious=true; p.motion.victoryAge=time; }
  if(motion==9) {
    const float t=std::fmod(time,2.8F),touchdown=std::sqrt(7.F/18.F);
    if(t<touchdown) { p.position.y=7-18*t*t; p.velocity.y=-36*t; p.airBlend=ease(t/.14F); p.grounded=false; }
    else { p.motion.landAge=t-touchdown; p.motion.landStrength=1.55F; p.airBlend=std::exp(-22.F*(t-touchdown)); p.velocity.y=-22.F*std::exp(-18.F*(t-touchdown)); }
  }
  if(motion==10) p.motion.wakeTime=std::fmod(time,4.8F);
  p.motion.verticalVelocity=p.velocity.y;
  return p;
}
}
