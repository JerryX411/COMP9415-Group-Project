#include "graphics/RelayModels.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <glm/gtc/matrix_transform.hpp>

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
Foot gait(float phase,float sprint,float stride=0) {
  const float cycle=phase/(2*pi)-std::floor(phase/(2*pi));
  // A short sprint stance leaves the swing leg more time, keeping knee and ankle speeds down.
  const float stance=glm::mix(.34F,.22F,sprint);
  // Must match the cadence in Game::updateAnimation (a boosted stride takes longer steps).
  const float length=2*pi/glm::mix(glm::mix(3.8F,2.6F,sprint),3.F,stride),reach=length*stance*.5F;
  if(cycle<stance) {
    const float t=cycle/stance;
    return {glm::mix(-reach,reach,t),0,curve(t,{{0,.12F},{.18F,0},{.75F,0},{1,-.18F}})};
  }
  const float t=(cycle-stance)/(1-stance);
  const float lift=std::sin(pi*t);
  return {glm::mix(reach,-reach,ease(t)),lift*lift*glm::mix(.13F,.18F,sprint),curve(t,{{0,-.18F},{.3F,-.26F},{.8F,.18F},{1,.12F}})};
}
// Tricks: duration by kind and the eased 0..1 progress (0 when none is playing).
float trickLength(int kind) {
  constexpr float lengths[]{0,.42F,.5F,.38F,.36F,.42F,.45F,.55F,.5F,.62F,.5F,.55F};
  return kind>0&&kind<12?lengths[kind]:.4F;
}
// How high each trick lifts the body (on top of any jump).
float trickLift(int kind) {
  constexpr float lifts[]{0,.2F,.38F,.1F,.24F,.24F,.2F,.34F,.3F,.42F,.22F,.14F};
  return kind>0&&kind<12?lifts[kind]:0;
}
float trickPhase(const CourierMotion& a) { return a.trickKind>0&&a.trickAge<trickLength(a.trickKind)?a.trickAge/trickLength(a.trickKind):-1; }
// Trick turn: a small wind-up against the turn, a hard snap through it and a slight overshoot
// that settles, instead of an even ease, so the move reads as driven rather than floated.
float trickTurn(float t) { return curve(t,{{0,0},{.14F,-.04F},{.64F,1.045F},{.84F,.99F},{1,1}}); }
// Boosted tricks (the spinning kip-up, the corkscrew, the wall kick, anything while boosted) hit
// harder: bigger wind-up, squash, stretch and follow-through.
float trickPower(const Player& p) {
  const int kind=p.motion.trickKind;
  return kind==5||kind>=7||p.boost!=Boost::None?1.45F:1.F;
}
// After a trick: the plant (a squash of weight as it lands, 0..1) and the follow-through (a
// damped overshoot along the trick's axis, -1..1), both over about a third of a second.
float trickPlant(const CourierMotion& a) {
  if(a.trickKind<=0) return 0;
  const float u=a.trickAge-trickLength(a.trickKind);
  return u>=0&&u<.3F?curve(u,{{0,0},{.04F,1},{.2F,0}}):0;
}
float trickSettle(const CourierMotion& a) {
  if(a.trickKind<=0) return 0;
  const float u=a.trickAge-trickLength(a.trickKind);
  return u>=0&&u<.4F?std::exp(-9.F*u)*std::sin(u*26.F):0;
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
  // Sliding replaces the gait with a fixed skid pose (lead leg out, hand trailing).
  const float slide=std::clamp(p.slideBlend,0.F,1.F);
  // Speed-pad states: a stride is a longer, livelier walk; a flight hovers with the whole body
  // tipped forward (see fallTransform), legs trailing and arms swept back.
  const float stride=std::clamp(p.strideBlend,0.F,1.F),flight=std::clamp(p.flightBlend,0.F,1.F);
  const float ground=1-air, gaitWeight=move*ground*(1-slide)*(1-flight);
  // Trick: a tuck (flips, rolls, wall kick; half a tuck for a kip-up) or an open spin, peaking
  // mid-trick; the twist out of a slide also scissors the legs.
  const float trickT=trickPhase(a),trick=trickT>=0?std::sin(trickT*pi):0;
  const int kind=a.trickKind;
  const float tuckTrick=kind==1||kind==2||kind==7||kind==8||kind==10||kind==11?trick:kind==4||kind==5?.6F*trick:0;
  // The running wall flip is laid out instead: body long and arched, legs together, arms flung
  // up and wide, which reads as the flip with the most tension.
  const float layout=kind==9?trick:0;
  const float spinTrick=kind==3||kind==5||kind==6?trick:0,scissor=kind==6?trick:0;
  // Wall kick: both legs start driven back into the wall, then tuck.
  const float wallPush=kind>=8&&kind<=11&&trickT>=0?curve(trickT,{{0,1},{kind==9?.16F:.12F,1},{.35F,0}}):0;
  // Wall shove (walking): both palms drive into the wall, then the arms give as the body is pushed back.
  const float shove=a.pushAge<1?curve(a.pushAge,{{0,0},{.05F,1},{.16F,.75F},{.42F,0}}):0;
  // Kip-up: the legs snap through under the body, then drive into the first stride.
  const float kipDrive=(kind==4||kind==5)&&trickT>=0?curve(trickT,{{0,0},{.3F,0},{.55F,1},{1,0}}):0;
  // Weight around a trick: a wind-up dip and arm pull-back as it starts, a plant as it lands,
  // then the follow-through (see trickPlant / trickSettle); boosted tricks hit harder.
  const float power=trickPower(p);
  const float windup=trickT>=0?curve(trickT,{{0,0},{.07F,1},{.16F,0}})*power:0;
  const float plant=trickPlant(a)*power,settle=trickSettle(a)*power;
  // Crouch is a mechanical tuck sized for one-voxel gaps: see the end of the torso block.
  const float crouch=std::clamp(p.crouchBlend,0.F,1.F);
  // Hits: h is the recoil weight; f and sd are the push direction in the courier's frame.
  const float h=p.hitBlend,hitF=std::clamp(a.hitForward,-1.F,1.F),hitS=std::clamp(a.hitSide,-1.F,1.F);
  const float shock=a.shockAge<1?curve(a.shockAge,{{0,1},{.5F,1},{.8F,0}}):0;
  const auto jitter=[&](float k) { return std::sin(p.idlePhase*71.F+k)*.6F+std::sin(p.idlePhase*113.F+k*2.3F)*.4F; };
  const float start=curve(a.startAge,{{0,0},{.055F,1},{.18F,.15F},{.3F,0}})*ground;
  const float brake=curve(a.stopAge,{{0,0},{.07F,1},{.13F,1},{.25F,.3F},{.39F,-.18F},{.54F,0}})*a.stopStrength*ground;
  const float pivot=curve(a.pivotAge,{{0,0},{.075F,1},{.23F,.4F},{.4F,0}})*ground;
  const float hard=std::clamp((a.landStrength-.75F)/.8F,0.F,1.F);
  const float landTime=a.landAge/(1+.4F*hard);
  const float land=curve(landTime,{{0,0},{.045F,1},{.105F,1},{.20F,.42F},{.30F,-.12F},{.44F,0}})*a.landStrength*ground;
  const float launch=curve(a.launchAge,{{0,0},{.05F,1},{.12F,.15F},{.20F,0}});
  const float joy=curve(a.celebrateAge,{{0,0},{.19F,0},{.34F,1},{.55F,.8F},{.85F,0}})*(1-p.hitBlend);
  const float death=ease(p.deathTime/.6F);
  const float inspect=curve(std::fmod(std::max(0.F,a.idleFor-2),9.F),{{0,0},{.5F,1},{1.5F,1},{2.1F,0},{9,0}})*(1-move)*ground*(1-crouch);
  // Idle head gestures pause while crouched so the head stays under low ceilings.
  const float scan=std::sin(std::max(0.F,a.idleFor-4)*1.3F)*ease(a.idleFor-4)*(1-move)*(1-crouch);
  const float victorious=a.victorious?ease(a.victoryAge/.22F):0;
  const float cheer=std::sin(a.victoryAge*8.F)*victorious;

  Foot left=gait(p.walkPhase,run,stride),right=gait(p.walkPhase+pi,run,stride);
  for(auto* foot:{&left,&right}) { foot->z*=gaitWeight*(1-.75F*crouch); foot->lift*=gaitWeight*(1-.6F*crouch); foot->pitch*=gaitWeight; }
  // Upward tuck, apex hang, then extend the feet to meet the ground.
  const float rise=ease((a.verticalVelocity+1)/7.F),fall=ease(-a.verticalVelocity/7.F);
  // Take-off: a two-frame compression, then the legs drive out straight with toes pointed
  // (push) before the tuck takes over, so the jump reads as a shove off the floor.
  const float compress=curve(a.launchAge,{{0,0},{.035F,1},{.05F,1},{.12F,0}});
  const float launchWindow=curve(a.launchAge,{{0,1},{.36F,1},{.5F,0}});
  const float push=curve(a.launchAge,{{0,0},{.03F,0},{.17F,1},{.24F,1},{.36F,0}});
  const float tuck=air*(1-push);
  left.z=glm::mix(left.z,-.15F-.08F*rise,tuck); right.z=glm::mix(right.z,.09F+.08F*rise,tuck);
  left.lift=glm::mix(left.lift,.08F+.14F*rise-.04F*fall,tuck);
  right.lift=glm::mix(right.lift,.13F+.11F*rise-.08F*fall,tuck);
  left.pitch=glm::mix(left.pitch,-.16F*rise,tuck); right.pitch=glm::mix(right.pitch,-.22F*rise,tuck);
  left.z=glm::mix(left.z,.06F,push); right.z=glm::mix(right.z,.10F,push);
  left.lift=glm::mix(left.lift,0.F,push); right.lift=glm::mix(right.lift,.02F,push);
  left.pitch=glm::mix(left.pitch,-.55F,push); right.pitch=glm::mix(right.pitch,-.6F,push);
  const float stopStep=curve(a.stopAge,{{0,0},{.12F,.10F},{.29F,.03F},{.4F,0}})*a.stopStrength*ground;
  right.lift+=stopStep; left.z-=.045F*brake; right.z+=.045F*brake;
  left.z-=pivot*.05F; right.lift+=pivot*.06F;
  if(a.victorious) { left.lift+=std::max(0.F,cheer)*.09F; right.lift+=std::max(0.F,-cheer)*.09F; }
  left.z=glm::mix(left.z,-.02F,death); right.z=glm::mix(right.z,.02F,death);
  left.lift=glm::mix(left.lift,0.F,death); right.lift=glm::mix(right.lift,0.F,death);
  left.pitch*=1-death; right.pitch*=1-death;
  // Flying after a lethal blow, the legs trail toward the source.
  left.z-=.16F*hitF*air*death; right.z-=.12F*hitF*air*death; left.lift+=.08F*air*death;
  // Stagger: feet trail against the push and the foot on the pushed side lifts to catch it.
  left.z+=h*.09F*hitF; right.z+=h*.09F*hitF;
  left.lift+=h*.09F*std::max(0.F,-hitS); right.lift+=h*.09F*std::max(0.F,hitS);
  // Flight: legs together and trailing, toes pointed, with a little alternating kick. Take-off
  // tucks them first; a touchdown swings them down to meet the floor.
  const float takeoff=a.takeoffAge<1?curve(a.takeoffAge,{{0,0},{.05F,1},{.22F,.3F},{.35F,0}}):0;
  const float touch=a.touchAge<1?curve(a.touchAge,{{0,0},{.04F,1},{.2F,.4F},{.32F,0}}):0;
  const float kick=std::sin(p.idlePhase*11.F)*.04F;
  left.z=glm::mix(left.z,.04F,flight); right.z=glm::mix(right.z,.08F,flight);
  left.lift=glm::mix(left.lift,.05F+kick+.12F*takeoff,flight); right.lift=glm::mix(right.lift,.05F-kick+.12F*takeoff,flight);
  left.pitch=glm::mix(left.pitch,-.75F,flight); right.pitch=glm::mix(right.pitch,-.75F,flight);
  left.z-=.06F*touch; right.z+=.05F*touch;
  // Trick tuck: knees snap up toward the chest; a spin draws one knee up like a skater.
  left.lift+=.16F*tuckTrick; right.lift+=.16F*tuckTrick; left.z-=.08F*tuckTrick; right.z-=.08F*tuckTrick;
  right.lift+=.12F*spinTrick; right.z-=.06F*spinTrick;
  left.z+=.16F*scissor; right.z-=.14F*scissor; left.pitch-=.5F*scissor; right.pitch+=.3F*scissor;
  for(auto* foot:{&left,&right}) { foot->z=glm::mix(foot->z,.2F,wallPush); foot->lift=glm::mix(foot->lift,.1F,wallPush); foot->pitch=glm::mix(foot->pitch,-.7F,wallPush); }
  for(auto* foot:{&left,&right}) { foot->z=glm::mix(foot->z,.02F,layout); foot->lift=glm::mix(foot->lift,.02F,layout); foot->pitch=glm::mix(foot->pitch,-.85F,layout); }
  left.z+=.06F*shove; right.z+=.08F*shove;  // the feet brace back
  left.z-=.1F*kipDrive; right.z+=.12F*kipDrive; left.pitch-=.4F*kipDrive;
  // Slide: the right leg shoots out straight on its heel, the left folds under the body.
  right.z=glm::mix(right.z,-.46F,slide); right.lift=glm::mix(right.lift,0.F,slide); right.pitch=glm::mix(right.pitch,.50F,slide);
  left.z=glm::mix(left.z,-.02F,slide); left.lift=glm::mix(left.lift,.04F,slide); left.pitch=glm::mix(left.pitch,-.35F,slide);

  // Crouched, the deep squat replaces the gait dip.
  float pelvis=.64F-.095F*gaitWeight*(1-crouch)+.025F*std::sin(2*p.walkPhase)*gaitWeight*(1-.5F*crouch);
  // Footfalls nod the torso (sprint steps hardest); bodyScale adds the squash.
  const float footfall=curve(a.stepAge,{{0,0},{.05F,1},{.16F,0}})*a.stepStrength*gaitWeight;
  pelvis-=.13F*land+.055F*start+.075F*brake+.02F*launch+.07F*compress;
  // Trick wind-up and plant, the plant of a kip-up into the stride, and the shove of a pad.
  const float popPlant=a.popAge<1?curve(a.popAge,{{0,0},{.04F,1},{.15F,0}}):0;
  pelvis-=(.05F*windup+.07F*plant+.06F*popPlant)*ground;
  pelvis-=.29F*crouch;
  if(a.wakeTime>=0) pelvis-=.17F*(1-ease((a.wakeTime-.65F)/1.1F));
  pelvis+=.025F*joy+.015F*std::sin(p.idlePhase*2.2F)*(1-move)*ground;
  pelvis=glm::mix(pelvis,.62F,death);  // Legs straight while lying.
  for(const auto& foot:{left,right}) pelvis=std::min(pelvis,ankleHeight(foot)+std::sqrt(std::max(.01F,.49F*.49F-foot.z*foot.z)));
  n[pelvis_].offset.y=pelvis;
  n[pelvis_].offset.x=std::sin(p.walkPhase)*(.022F+.012F*run)*gaitWeight;
  n[hipL_].rotation.z=-.18F*slide;  // The folded knee splays out.

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
    std::sin(p.walkPhase)*(.075F+.035F*run)*gaitWeight+a.torsoYaw,bank+std::sin(p.walkPhase)*.05F*gaitWeight};
  n[body_].rotation.x=glm::mix(n[body_].rotation.x,-.08F*rise+.10F*fall,air);
  n[head_].rotation={-n[body_].rotation.x*.5F-.16F*inspect+a.lookPitch,std::clamp(a.lookYaw,-.6F,.6F)-a.torsoYaw*.5F+scan*.32F+.16F*inspect,-bank*.65F+.10F*inspect};
  n[head_].offset.y+=std::sin(p.walkPhase-.55F)*(.012F+.008F*(1-crouch))*gaitWeight;
  n[head_].rotation.z-=std::sin(p.walkPhase)*.05F*gaitWeight*(1-crouch);  // The head counters the body rock.
  n[body_].rotation.x-=.05F*footfall+.08F*compress; n[head_].rotation.x+=.05F*footfall+.08F*compress+.12F*push;
  n[head_].rotation.x+=std::sin(2*p.walkPhase-.5F)*.024F*gaitWeight;
  // Tuck: the waist telescopes, the head sinks between the shoulders, the antenna
  // retracts and the torso leans in; the head stays level to keep looking ahead.
  n[body_].offset.y-=.20F*crouch; n[head_].offset.y-=.27F*crouch;
  n[antenna_].offset.y-=.25F*crouch; n[antennaSocket_].offset.y-=.12F*crouch;
  n[pack_].offset.y-=.15F*crouch;  // The pack slides down the back as the torso tips forward.
  n[body_].rotation.x-=.80F*crouch; n[head_].rotation.x+=.80F*crouch;
  // Slide: lean back against the skid with the head up and ahead, banked and twisted
  // toward the trailing hand.
  const float surge=std::clamp(a.surge,0.F,1.F)*slide;
  n[body_].rotation.x=glm::mix(n[body_].rotation.x,.38F+.14F*surge,slide); n[head_].rotation.x=glm::mix(n[head_].rotation.x,-.38F-.12F*surge,slide);
  n[body_].rotation.z+=.10F*slide; n[body_].rotation.y+=.22F*slide; n[head_].rotation.y-=.18F*slide;
  // Drop-in jolt: momentum pitches the body forward for an instant before it snaps back into
  // the skid lean (twice as hard on a boosted slide).
  const float dropIn=a.slideAge<1?curve(a.slideAge,{{0,0},{.04F,1},{.15F,0}})*slide*(1+std::clamp(a.surge,0.F,1.F)):0;
  n[body_].rotation.x-=.22F*dropIn; n[head_].rotation.x+=.14F*dropIn;
  // Pop out of a slide: a forward lunge with a big opposite-arm pump (arms below). Rush, above
  // sprint speed: the body tips into the wind and both arms sweep back.
  const float pop=a.popAge<1?curve(a.popAge,{{0,0},{.05F,1},{.2F,.45F},{.36F,0}}):0;
  const float boost=a.boostAge<1?curve(a.boostAge,{{0,0},{.06F,1},{.35F,.3F},{.6F,0}}):0;
  const float rush=std::max(boost,std::clamp((glm::length(glm::vec2(p.velocity.x,p.velocity.z))-Game::sprintSpeed)/4.F,0.F,1.F)*ground*(1-slide));
  n[body_].rotation.x-=.28F*pop*(kind==5?1.3F:1.F)+.16F*rush; n[head_].rotation.x+=.14F*pop+.10F*rush;
  // Speed-pad whiplash: the shove snaps the body back for an instant before it whips forward into
  // the rush; the head lags the other way (arms below).
  const float whip=a.boostAge<1?curve(a.boostAge,{{0,0},{.035F,1},{.1F,-.7F},{.22F,.15F},{.34F,0}}):0;
  n[body_].rotation.x+=.24F*whip; n[head_].rotation.x-=.16F*whip;
  // Trick follow-through: the body overshoots along the trick's axis and settles, the head
  // counters it.
  if(settle!=0) {
    if(kind==2||kind==4||kind==5||kind==7||kind==8) { n[body_].rotation.x-=.2F*settle; n[head_].rotation.x+=.13F*settle; }
    if(kind==1) { n[body_].rotation.z+=.24F*settle; n[head_].rotation.z-=.15F*settle; }
    if(kind==3||kind==5||kind==6||kind==7) { n[body_].rotation.y+=.3F*settle; n[head_].rotation.y-=.2F*settle; }
  }
  n[body_].rotation.x-=.06F*windup; n[head_].rotation.x+=.06F*windup;
  n[body_].rotation.x+=.28F*layout; n[head_].rotation.x-=.22F*layout;  // arched
  n[body_].rotation.x+=.2F*shove; n[head_].rotation.x-=.1F*shove;     // pushed back off the wall
  // Stride: a touch more lean and a bigger, bouncier arm swing. Flight: the head lifts to look
  // ahead of the tipped body.
  n[body_].rotation.x-=.08F*stride*move; n[head_].rotation.x+=.05F*stride*move+.62F*flight;
  n[head_].rotation.x-=.2F*touch; n[body_].rotation.x+=.12F*touch;
  // Wall bounce: knocked back upright, head snapping back, arms thrown up to brace.
  const float rebound=a.bounceAge<1?curve(a.bounceAge,{{0,0},{.04F,1},{.18F,.5F},{.4F,0}})*std::min(a.bounceStrength,1.2F):0;
  n[body_].rotation.x+=.30F*rebound; n[head_].rotation.x-=.20F*rebound;
  // Big, springy arm swing, larger behind than in front (as runners do).
  const auto armAngle=[&](float forward) { return (forward>0?.55F:.8F-.25F*crouch)*forward*(1+.45F*stride); };
  n[shoulderL_].rotation={armAngle(-swing)+.10F*start,0,.03F+.07F*air};
  n[shoulderR_].rotation={armAngle(swing)+.10F*start,0,-.03F-.07F*air};
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,.6F*rise-.15F*fall,air);
  // Arms swing back while compressing, then both fists drive up with bent elbows
  // (a power-up pose that keeps the hands inside the body envelope).
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(glm::mix(n[shoulder].rotation.x,-.25F,compress),.92F,push);
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,1.75F,push);
  for(int i=0;i<6;++i) n[fingers_[i]].rotation.x-=.9F*push;
  // Arms flare out on take-off and flutter on the way down for balance.
  const float flare=(.26F*rise+.20F*fall)*air*(1-launchWindow),flutter=std::sin(a.launchAge*21.F)*fall*air*.11F;
  n[shoulderL_].rotation.z-=flare+flutter; n[shoulderR_].rotation.z+=flare-flutter;
  n[elbowL_].rotation.x+=.22F*rise*air; n[elbowR_].rotation.x+=.22F*rise*air;
  n[shoulderL_].rotation.x+=std::sin(2*p.walkPhase-.4F)*.07F*run*gaitWeight;
  n[shoulderR_].rotation.x-=std::sin(2*p.walkPhase+.4F)*.07F*run*gaitWeight;
  n[elbowL_].rotation.x=.18F+.38F*run+.16F*std::max(0.F,swing);
  n[elbowR_].rotation.x=.18F+.38F*run+.16F*std::max(0.F,-swing);
  n[handL_].rotation.z=.10F*swing; n[handR_].rotation.z=-.10F*swing;
  n[shoulderL_].rotation.x+=.28F*brake+.18F*land;
  n[shoulderR_].rotation.x+=.28F*brake+.18F*land;
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x+=.18F*crouch;
  n[shoulderL_].rotation.x+=.85F*pop+.6F*rebound; n[shoulderR_].rotation.x-=.65F*pop-.6F*rebound;
  n[shoulderL_].rotation.z-=.3F*rebound; n[shoulderR_].rotation.z+=.3F*rebound;
  // Trick arms: hugged in for a flip or roll, flung wide for a spin.
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,.75F,tuckTrick)+.2F*spinTrick;
  n[shoulderL_].rotation.z-=.9F*spinTrick-.15F*tuckTrick; n[shoulderR_].rotation.z+=.9F*spinTrick-.15F*tuckTrick;
  // Flight arms: swept back along the body and a little out, opening on take-off; on
  // touchdown they swing forward for balance.
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,-1.15F+.5F*takeoff,flight)+.55F*touch;
  n[shoulderL_].rotation.z-=(.32F+.25F*takeoff)*flight; n[shoulderR_].rotation.z+=(.32F+.25F*takeoff)*flight;
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,-.85F,.8F*rush);
  n[shoulderL_].rotation.z-=.25F*rush; n[shoulderR_].rotation.z+=.25F*rush;
  // Arms in the transitions: thrown back by a pad's shove, pulled back in a trick's wind-up, and
  // flung through (then recovering) in its follow-through.
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x-=.55F*std::max(0.F,whip)+.45F*windup-.35F*settle;
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(glm::mix(n[shoulder].rotation.x,2.3F,layout),1.45F,shove);
  n[shoulderL_].rotation.z-=.9F*layout+.12F*shove; n[shoulderR_].rotation.z+=.9F*layout+.12F*shove;
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,.12F,std::max(layout,shove));
  n[shoulderL_].rotation.z-=.18F*std::abs(whip)+.12F*plant; n[shoulderR_].rotation.z+=.18F*std::abs(whip)+.12F*plant;
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x+=.75F*crouch+.5F*pop+.25F*stride*move;
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,.15F,flight);
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,1.6F,tuckTrick)*(1-.7F*spinTrick);
  n[head_].rotation.x+=.25F*tuckTrick;  // chin tucked
  // Slide arms: the left hand trails on the floor behind, the right reaches ahead for balance.
  n[shoulderL_].rotation=glm::mix(n[shoulderL_].rotation,glm::vec3(-.75F,0,-.55F),slide);
  n[elbowL_].rotation.x=glm::mix(n[elbowL_].rotation.x,.15F,slide);
  n[shoulderR_].rotation=glm::mix(n[shoulderR_].rotation,glm::vec3(1.05F,0,.30F),slide);
  n[elbowR_].rotation.x=glm::mix(n[elbowR_].rotation.x,.95F,slide);
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
  // Power-up absorbed: a quick flex, chest out and head up, elbows bent and fists clenched, arms
  // flared; the eyes take on the power-up's colour (below).
  const float charged=a.pickupAge<1?curve(a.pickupAge,{{0,0},{.07F,1},{.35F,.5F},{.6F,0}})*(1-death)*(1-slide):0;
  n[body_].rotation.x+=.1F*charged; n[head_].rotation.x-=.12F*charged;
  n[shoulderL_].rotation.z-=.35F*charged; n[shoulderR_].rotation.z+=.35F*charged;
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,1.3F,charged);
  if(a.victorious) {
    for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,1.40F+.12F*cheer,victorious);
    for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,1.35F,ease(victorious*2));
    n[head_].rotation.z+=cheer*.10F; n[body_].rotation.z+=cheer*.04F;
  }
  if(a.wave>0) {
    // Greeting: right arm raised forward and out, forearm swinging from the elbow.
    const float w=ease(a.wave),beat=std::sin(p.idlePhase*10.F);
    // The elbow folds while the arm rises, keeping the hand inside the body envelope.
    const float rising=std::sin(3.14159F*w);
    n[shoulderR_].rotation=glm::mix(n[shoulderR_].rotation,glm::vec3(2.7F,0,.06F),w)-glm::vec3(0,0,.30F*rising);
    n[elbowR_].rotation=glm::mix(n[elbowR_].rotation,glm::vec3(.18F,0,.32F*beat),w)+glm::vec3(1.25F*rising,0,0);
    n[handR_].rotation.z+=.2F*beat*w;
    n[head_].rotation.z+=.10F*w*std::sin(p.idlePhase*5.F); n[body_].rotation.z-=.05F*w;
  }
  // Directional recoil: the body bends away from the blow, the head whips the other way,
  // and the arm on the struck side flails out. Blunt blows bend further than shocks.
  const float bend=a.hitKind==0?1.F:.6F;
  n[body_].rotation.x+=h*bend*(.06F-.30F*hitF); n[body_].rotation.z-=h*bend*.22F*hitS;
  n[head_].rotation.x+=h*(.22F*hitF-.06F); n[head_].rotation.z+=h*.16F*hitS; n[head_].rotation.y+=h*.22F*hitS;
  for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x-=h*(.20F+.45F*hitF);
  n[shoulderL_].rotation.z-=h*.40F*std::max(0.F,hitS); n[shoulderR_].rotation.z+=h*.40F*std::max(0.F,-hitS);
  for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x+=h*.3F;
  if(shock>0) {
    // Electrocution: rigid splayed arms and fingers, and a high-frequency shiver everywhere.
    n[shoulderL_].rotation=glm::mix(n[shoulderL_].rotation,glm::vec3(.35F,0,-.2F),shock*.85F)+glm::vec3(jitter(7),0,jitter(8))*.05F*shock;
    n[shoulderR_].rotation=glm::mix(n[shoulderR_].rotation,glm::vec3(.35F,0,.2F),shock*.85F)+glm::vec3(jitter(9),0,jitter(10))*.05F*shock;
    for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,.05F,shock);
    for(int i=0;i<6;++i) n[fingers_[i]].rotation.x=glm::mix(n[fingers_[i]].rotation.x,.35F,shock);
    n[body_].rotation+=glm::vec3(jitter(1),jitter(2)*.5F,jitter(3))*.06F*shock;
    n[head_].rotation+=glm::vec3(jitter(4)-1.2F,jitter(5),jitter(6))*.11F*shock;
  }
  for(int i=0;i<6;++i) n[fingers_[i]].rotation.x=-(.15F*run+(i>=3?grasp*.85F+joy*.7F:0));
  n[antenna_].rotation={a.antenna+jitter(11)*.5F*shock+.55F*slide+.4F*rush+.5F*flight+.35F*std::clamp(a.surge,0.F,1.F),0,-bank*.8F+std::sin(p.walkPhase-.6F)*.055F*gaitWeight+jitter(12)*.5F*shock};

  const float blinkPhase=std::fmod(p.idlePhase+.4F,3.7F);
  const float blink=curve(blinkPhase,{{0,1},{.065F,.08F},{.115F,.08F},{.20F,1},{3.7F,1}});
  const float happy=std::max({joy,victorious,ease(a.wave*1.6F-.3F)})*(1-death)*(1-p.hitBlend);
  for(int eye:{eyeL_,eyeR_}) {
    n[eye].dimensions.y=(.15F+.035F*air-.04F*run)*blink*(1-.75F*p.hitBlend)*(1-.3F*crouch)*(1-.25F*slide);
    n[eye].colour=glm::mix(n[eye].colour,a.hitKind==1?glm::vec3(.75F,.95F,1):glm::vec3(1,.36F,.20F),p.hitBlend);
    if(shock>0&&std::sin(p.idlePhase*57.F)>.2F) n[eye].dimensions.y*=.25F;
    n[eye].offset.x+=std::clamp(a.lookYaw,-.4F,.4F)*.035F;
    n[eye].visible=happy<.55F;
    n[eye].rotation.z=(eye==eyeL_?1.F:-1.F)*p.hitBlend*.35F;
  }
  n[browL_].rotation.z=.15F*(run+slide)-.12F*happy; n[browR_].rotation.z=-.15F*(run+slide)+.12F*happy;
  for(int i=0;i<4;++i) { n[smileEyes_[i]].visible=happy>=.55F; n[smileEyes_[i]].rotation.z=i%2==0?.42F:-.42F; }
  // Face: catch-lights follow the eyes, the smile widens when happy and flips to a frown
  // on a hit, and the cheeks glow brighter when pleased.
  for(int i=0;i<2;++i) {
    auto& glint=n[std::size_t(glints_[std::size_t(i)])]; const auto& eye=n[std::size_t(i==0?eyeL_:eyeR_)];
    glint.visible=eye.visible&&blink>.5F&&shock<=0&&p.hitBlend<.5F;
    glint.offset.x=eye.offset.x+.032F;
    n[std::size_t(cheeks_[std::size_t(i)])].emission=.35F+.55F*happy;
  }
  const float grin=.45F+.35F*happy-1.1F*p.hitBlend;
  n[std::size_t(mouth_[1])].rotation.z=-grin; n[std::size_t(mouth_[2])].rotation.z=grin;
  n[std::size_t(mouth_[0])].dimensions.x=.05F+.03F*happy;
  for(int i:mouth_) n[std::size_t(i)].colour=glm::mix(n[std::size_t(i)].colour,glm::vec3(1,.4F,.25F),p.hitBlend);
  for(int i:mouth_) n[std::size_t(i)].visible=shock<=0||std::sin(p.idlePhase*47.F)>0;
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
    // Shake off the dust, then a quick "ready" nod with raised fists and happy eyes.
    const float shake=curve(t,{{0,0},{2.45F,0},{2.56F,1},{2.85F,1},{3.F,0},{4,0}})*std::sin(t*44.F);
    n[body_].rotation.z+=.07F*shake; n[head_].rotation.z-=.16F*shake; n[head_].rotation.y+=.20F*shake;
    n[antenna_].rotation.z+=.45F*shake;
    const float ready=curve(t,{{0,0},{2.95F,0},{3.12F,1},{3.6F,1},{3.92F,0},{4,0}});
    n[head_].rotation.x+=-.14F*curve(t,{{0,0},{3.0F,0},{3.12F,1},{3.3F,0},{4,0}});
    for(int shoulder:{shoulderL_,shoulderR_}) n[shoulder].rotation.x=glm::mix(n[shoulder].rotation.x,.75F,ready);
    for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,1.25F,ready);
    for(int i=0;i<6;++i) n[fingers_[i]].rotation.x-=.8F*ready;
    if(ready>.55F) { n[eyeL_].visible=n[eyeR_].visible=false; for(int eye:smileEyes_) n[eye].visible=true; }
  }
  // Backpack light strip: brighter and cooler while the back glows (sprint held, or boosted).
  // Kept below 1: the shader reads emission above 1 as an unlit halo, which would dim the strip.
  n[packLight_].emission=std::min(.98F,bindPose_[packLight_].emission+.53F*p.backGlow);
  n[packLight_].colour=glm::mix(bindPose_[packLight_].colour,glm::vec3(.8F,.97F,1),p.backGlow);
  for(int i=0;i<6;++i) n[fingers_[i]].rotation.x-=.9F*charged;
  if(a.pickupAge<1&&p.deathTime<=0) {
    // The lights (eyes, pack strip, cheeks...) flush with the power-up's colour and fade back.
    const float flush=curve(a.pickupAge,{{0,0},{.05F,1},{.45F,.6F},{1,0}});
    const glm::vec3 colour=pickupColour(PowerUp(a.pickupKind));
    for(std::size_t i=0;i<n.size();++i) if(n[i].emission>0) n[i].colour=glm::mix(n[i].colour,colour,.8F*flush);
  }
  if(a.hackAge<.9F&&p.deathTime<=0) {
    // Crimson bite, "hacked": the lights pulse red and black (slowly, not a strobe), the eyes narrow to red slits on the
    // dark beats, and the head and body twitch.
    const float hack=curve(a.hackAge,{{0,0},{.03F,1},{.6F,.75F},{.9F,0}});
    const bool dark=std::sin(p.idlePhase*17.F)+std::sin(p.idlePhase*7.F)>.6F;
    for(std::size_t i=0;i<n.size();++i) if(n[i].emission>0) {
      n[i].colour=glm::mix(n[i].colour,dark?glm::vec3(.05F,0,.01F):glm::vec3(1,.08F,.14F),hack);
      if(dark) n[i].emission*=1-.8F*hack;
    }
    for(int eye:{eyeL_,eyeR_}) { n[eye].colour=glm::mix(n[eye].colour,glm::vec3(1,.1F,.15F),hack); if(dark) n[eye].dimensions.y*=1-.7F*hack; }
    n[head_].rotation+=glm::vec3(jitter(21),jitter(22),jitter(23))*.07F*hack;
    n[body_].rotation+=glm::vec3(jitter(24),jitter(25),jitter(26))*.03F*hack;
    n[antenna_].rotation.z+=jitter(27)*.4F*hack;
  }
  if(p.health<=0||p.deathTime>0) {
    // Go limp, bending with the fall (the whole body topples in fallTransform).
    // Go limp and lie flat: arms relaxed along the sides, legs straight, head rolled aside.
    n[body_].rotation=glm::mix(n[body_].rotation,glm::vec3(0,0,0),death);
    n[head_].rotation=glm::mix(n[head_].rotation,glm::vec3(0,.45F,.2F),death);
    n[shoulderL_].rotation=glm::mix(n[shoulderL_].rotation,glm::vec3(.05F,0,-.12F),death);
    n[shoulderR_].rotation=glm::mix(n[shoulderR_].rotation,glm::vec3(.05F,0,.12F),death);
    // In flight the limbs trail back toward the blow and flail.
    const float flight=air*death,flail=std::sin(p.deathTime*17.F);
    n[shoulderL_].rotation+=flight*glm::vec3(-1.3F*hitF+.25F*flail,0,-.35F);
    n[shoulderR_].rotation+=flight*glm::vec3(-1.3F*hitF-.25F*flail,0,.35F);
    for(int elbow:{elbowL_,elbowR_}) n[elbow].rotation.x=glm::mix(n[elbow].rotation.x,.12F,death);
    for(std::size_t i=0;i<n.size();++i) if(bindPose_[i].emission>0) {
      n[i].emission=bindPose_[i].emission*(1-death);
      n[i].colour=glm::mix(bindPose_[i].colour,glm::vec3(.075F,.13F,.16F),death);
    }
  }
}

namespace {
// The run's own hop between footfalls (no hover or trick lift): it alone drives the run stretch.
float runHop(const Player& p) {
  return std::abs(std::sin(p.walkPhase))*(.085F+.08F*p.sprintBlend+.03F*p.strideBlend)*p.moveBlend*(1-p.airBlend)*(1-p.crouchBlend)*(1-p.flightBlend)*(p.deathTime>0?0.F:1.F);
}
}
float SceneModels::bodyHop(const Player& p) {
  // Bouncy run: the body hops between footfalls, landing on each plant (multiples of pi).
  // A flight hovers (with a slow bob); take-off springs it up past the hover first.
  const float takeoff=p.motion.takeoffAge<1?curve(p.motion.takeoffAge,{{0,0},{.05F,-.08F},{.18F,.22F},{.4F,0}}):0;
  const float hover=p.flightBlend*(.38F+.05F*std::sin(p.idlePhase*6.F))+takeoff*p.flightBlend;
  const float trickT=trickPhase(p.motion),lift=trickT>=0?std::sin(trickT*pi)*trickLift(p.motion.trickKind):0;
  return lift+hover+runHop(p);
}
glm::mat4 SceneModels::fallTransform(const Player& p) {
  // Trick: a turn about the body's middle (eased in and out), wrapped around whatever else
  // the body is doing. Roll: a full turn about forward. Flip: about the side. Spin: about up.
  // Kip-up: rocks up from the slide's lean-back into a forward lunge (and spins when boosted).
  // Twist: a spin leaning into it. Corkscrew: a flip and a spin at once. Wall kick: a back
  // flip while the half turn away from the wall unwinds.
  glm::mat4 trick(1);
  if(const float t=trickPhase(p.motion);t>=0&&p.deathTime<=0) {
    const float k=trickTurn(t),turn=6.2831853F*k;
    const glm::vec3 pivot(0,.62F*Game::modelScale,0),x(1,0,0),y(0,1,0),z(0,0,1);
    glm::mat4 r(1);
    switch(p.motion.trickKind) {
      case 1: r=glm::rotate(r,turn,z); break;
      case 2: r=glm::rotate(r,-turn,x); break;
      case 3: r=glm::rotate(r,turn,y); break;
      case 5: r=glm::rotate(r,turn,y); [[fallthrough]];
      case 4: r=glm::rotate(r,curve(t,{{0,.6F},{.4F,-.6F},{.75F,.1F},{1,0}}),x); break;
      case 6: r=glm::rotate(glm::rotate(r,turn,y),.3F*std::sin(t*3.14159F),z); break;
      case 7: r=glm::rotate(glm::rotate(r,turn,y),-turn,x); break;
      case 8: r=glm::rotate(glm::rotate(r,3.14159F*(k-1),y),turn,x); break;
      // Wall action: a laid-out back flip off a run (the half turn unwinding), a low aerial
      // cartwheel off a slide, and a flight's flip turn (a forward roll with the half twist).
      case 9: r=glm::rotate(glm::rotate(r,3.14159F*(k-1),y),turn,x); break;
      case 10: r=glm::rotate(glm::rotate(r,3.14159F*(k-1),y),turn,z); break;
      case 11: r=glm::rotate(glm::rotate(r,3.14159F*(k-1),y),-turn,x); break;
      default: break;
    }
    trick=glm::translate(glm::translate(glm::mat4(1),pivot)*r,-pivot);
  }
  // Shutdown topple: tip about the feet edge toward the push by the simulated fall angle.
  if(p.deathTime<=0&&p.flightBlend>.001F) {
    // Flight: the whole body tips forward about the pelvis and banks into turns.
    const glm::vec3 pivot(0,.64F*Game::modelScale,0);
    auto m=glm::translate(glm::mat4(1),pivot);
    m=glm::rotate(m,-.85F*p.flightBlend,{1,0,0});
    m=glm::rotate(m,std::clamp(p.motion.yawVelocity/6.F,-1.F,1.F)*.35F*p.flightBlend,{0,0,1});
    return glm::translate(m,-pivot)*trick;
  }
  if(p.deathTime<=0||p.motion.fallAngle<=0) return trick;
  glm::vec3 push(std::clamp(p.motion.hitSide,-1.F,1.F),0,-std::clamp(p.motion.hitForward,-1.F,1.F));
  if(glm::length(push)<.2F) push={0,0,-1};
  push=glm::normalize(push);
  // Pivot at the shell's extent along the fall: back (backpack), front (face) or side (arm).
  const float reach=(std::abs(push.z)>.7F?(push.z>0?.44F:.36F):.47F)*Game::modelScale;
  const glm::vec3 pivot=push*reach;
  auto m=glm::translate(glm::mat4(1),pivot);
  m=glm::rotate(m,p.motion.fallAngle,glm::normalize(glm::cross(glm::vec3(0,1,0),push)));
  return glm::translate(m,-pivot);
}
glm::vec3 SceneModels::bodyScale(const Player& p) {
  // Volume-preserving: stretch on take-off and fast falls, squash on landing.
  const auto& a=p.motion;
  // Take-off squashes for two frames, then springs into a 12% stretch.
  // Slide drop-in: a quick squash.
  const float slideIn=a.slideAge<1?curve(a.slideAge,{{0,0},{.05F,1},{.2F,0}})*.08F*(1+.9F*std::clamp(a.surge,0.F,1.F)):0;
  // Tricks: squash in the wind-up, stretch through the snap, squash on the plant. A pad's shove
  // squashes the body then springs it long.
  const float trickT=trickPhase(a),power=trickPower(p);
  const float trickPop=trickT>=0?curve(trickT,{{0,0},{.08F,-1},{.3F,1},{.7F,0},{1,0}})*.07F*power:0;
  const float trickPlanted=trickPlant(a)*.08F*power;
  const float shove=a.boostAge<1?curve(a.boostAge,{{0,0},{.03F,-1},{.1F,1},{.25F,0}})*.08F:0;
  const float popStretch=a.popAge<1?curve(a.popAge,{{0,0},{.05F,1},{.22F,0}})*.07F:0;
  // Take-off squashes then stretches; a touchdown squashes.
  const float takeoff=a.takeoffAge<1?curve(a.takeoffAge,{{0,0},{.04F,-1},{.14F,1},{.3F,0}})*.1F:0;
  const float touchdown=a.touchAge<1?curve(a.touchAge,{{0,0},{.04F,1},{.16F,0}})*.08F:0;
  const float bonk=a.bounceAge<1?curve(a.bounceAge,{{0,0},{.03F,1},{.12F,.3F},{.25F,0}})*.07F*std::min(a.bounceStrength,1.2F):0;
  const float launch=a.launchAge<1?curve(a.launchAge,{{0,-1},{.025F,-1},{.06F,1},{.16F,.35F},{.28F,0}})*.12F:0;
  const float falling=std::clamp(-a.verticalVelocity/22.F,0.F,1.F)*.07F*p.airBlend;
  const float land=curve(a.landAge,{{0,0},{.035F,1},{.10F,.55F},{.19F,-.28F},{.32F,0}})*std::min(a.landStrength,1.4F)*.09F;
  const float footfall=curve(a.stepAge,{{0,0},{.04F,1},{.16F,0}})*a.stepStrength*.05F*(1-p.airBlend)*p.moveBlend;
  // The run hop stretches the body at its top. Flight only adds a slight 4% stretch along the
  // tipped body; the hover height itself must not count, or the courier stretches to 2.5x.
  const float hop=runHop(p)/(.085F+.08F*p.sprintBlend+1e-4F),flightStretch=.04F*std::clamp(p.flightBlend,0.F,1.F);
  const float y=1+launch+falling-land*(1-p.airBlend)-footfall-slideIn-bonk-touchdown+takeoff+popStretch+.06F*hop*hop+flightStretch+trickPop-trickPlanted+shove,xz=1/std::sqrt(y);
  // Blunt hits dent the shell along the blow for a moment.
  const float dent=a.hitAge<1?curve(a.hitAge,{{0,0},{.03F,1},{.12F,.4F},{.25F,0}})*(a.hitKind==0?1.F:.3F):0;
  // Beam-in: the body pops from nothing with a little overshoot.
  const float appear=(a.spawnAge<1?curve(a.spawnAge,{{0,0},{.16F,1.18F},{.28F,.94F},{.4F,1}}):1)
    *(a.teleportAge<.45F?curve(a.teleportAge,{{0,.15F},{.14F,1.2F},{.26F,.93F},{.45F,1}}):1);  // a portal arrival pops in too
  return glm::vec3(xz*(1-.16F*dent*std::abs(a.hitSide)),y*(1+.05F*dent),xz*(1-.16F*dent*std::abs(a.hitForward)))*appear;
}

Player SceneModels::previewPlayer(float time,int motion) {
  Player p; p.position={0,0,0}; p.yaw=3.6F; p.idlePhase=time;
  const float cycle=std::fmod(time,2.4F);
  if(motion==0||motion==1) {
    p.moveBlend=1; p.sprintBlend=motion==1?1.F:0.F; p.motion.lean=motion==1?-.22F:-.13F;
    p.walkPhase=time*(motion==1?Game::sprintSpeed*2.6F:Game::speed*3.8F)/Game::modelScale;
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
    p.airBlend=p.hitBlend*.65F; p.position.y=std::sin(std::min(cycle/.65F,1.F)*pi)*.3F;
    // Cycle front, back, left and right blows.
    const int side=int(time/2.4F)%4; p.motion.hitAge=cycle; p.motion.hitKind=0;
    p.motion.hitForward=side==0?-1.F:side==1?1.F:0.F; p.motion.hitSide=side==2?1.F:side==3?-1.F:0.F;
  } else if(motion==8) { p.motion.victorious=true; p.motion.victoryAge=time; }
  if(motion==9) {
    const float t=std::fmod(time,2.8F),touchdown=std::sqrt(7.F/18.F);
    if(t<touchdown) { p.position.y=7-18*t*t; p.velocity.y=-36*t; p.airBlend=ease(t/.14F); p.grounded=false; }
    else { p.motion.landAge=t-touchdown; p.motion.landStrength=1.55F; p.airBlend=std::exp(-22.F*(t-touchdown)); p.velocity.y=-22.F*std::exp(-18.F*(t-touchdown)); }
  }
  if(motion==10) p.motion.wakeTime=std::fmod(time,4.8F);
  if(motion==12) {
    p.moveBlend=1; p.crouchBlend=1; p.crouching=true; p.motion.lean=-.06F;
    p.walkPhase=time*Game::crouchSpeed*3.8F/Game::modelScale;
  }
  if(motion==13) {
    const float t=std::fmod(time,1.8F);
    p.motion.shockAge=t; p.motion.hitAge=t; p.motion.hitKind=1; p.motion.hitForward=-.5F;
    p.hitBlend=curve(t,{{0,0},{.05F,1},{.35F,1},{.8F,0},{1.8F,0}}); p.airBlend=p.hitBlend*.5F;
    p.position.y=std::sin(std::min(t/.6F,1.F)*pi)*.35F;
  }
  if(motion==14) {
    // Slide: sprint in, drop into the skid, bleed off, recover.
    const float t=std::fmod(time,2.4F);
    p.moveBlend=1; p.motion.slideAge=std::max(0.F,t-.3F);
    p.slideBlend=curve(t,{{0,0},{.3F,0},{.4F,1},{1.5F,1},{1.85F,0},{2.4F,0}}); p.sliding=t>.3F&&t<1.5F;
    p.crouchBlend=curve(t,{{0,0},{.3F,0},{.4F,1},{1.6F,1},{1.95F,0},{2.4F,0}}); p.crouching=p.crouchBlend>.5F;
    p.sprintBlend=1-p.crouchBlend; p.motion.lean=-.22F*(1-p.crouchBlend);
    p.walkPhase=time*Game::sprintSpeed*2.6F/Game::modelScale;
    p.yaw+=1.2F;  // Three-quarter side view shows the lead leg and trailing hand.
  }
  if(motion==15) {
    // Low flight: sprint in, take off, hover with a weave, touch down.
    const float t=std::fmod(time,3.2F);
    p.moveBlend=1; p.motion.takeoffAge=std::max(0.F,t-.4F); p.motion.touchAge=t>2.6F?t-2.6F:10.F;
    p.flightBlend=curve(t,{{0,0},{.4F,0},{.55F,1},{2.6F,1},{2.8F,0},{3.2F,0}}); p.boost=p.flightBlend>.5F?Boost::Flight:Boost::None;
    p.sprintBlend=1-p.flightBlend; p.motion.lean=-.22F*(1-p.flightBlend);
    p.motion.yawVelocity=std::sin(time*1.7F)*3.F*p.flightBlend; p.walkPhase=time*Game::sprintSpeed*2.6F/Game::modelScale;
    if(t>2.6F) { p.motion.landAge=t-2.6F; p.motion.landStrength=.7F; }
    p.yaw+=1.4F;  // Side view: the tipped body and trailing legs read in profile.
  }
  if(motion==16) {
    // Boosted stride: a quicker, longer walk.
    p.moveBlend=1; p.strideBlend=1; p.boost=Boost::Stride; p.motion.lean=-.2F;
    p.walkPhase=time*Game::strideSpeed*3.F/Game::modelScale;
  }
  if(motion==11) p.motion.wave=curve(std::fmod(time,3.2F),{{0,0},{.3F,1},{2.4F,1},{2.85F,0},{3.2F,0}});
  p.motion.verticalVelocity=p.velocity.y;
  return p;
}
}
