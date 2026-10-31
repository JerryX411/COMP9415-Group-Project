#include "core/FollowCamera.h"
#include <algorithm>
#include <cmath>
namespace relay {
void FollowCamera::reset(const Game& game) {
  // Start behind the courier's facing (the missions face north, training faces east).
  yaw_=game.player().yaw+.4F; pitch_=.52F; distance_=actualDistance_=8;
  trauma_=0; shake_=glm::vec3(0); lastHurt_=game.player().hurtCooldown; lastLandAge_=10; lastHitAge_=game.player().motion.hitAge; lastSlideAge_=game.player().motion.slideAge; lastPopAge_=game.player().motion.popAge; lastBoostAge_=game.player().motion.boostAge; lastBounceAge_=game.player().motion.bounceAge; lastTakeoffAge_=game.player().motion.takeoffAge; lastTrickAge_=game.player().motion.trickAge; lastShieldBreak_=game.player().motion.shieldBreakAge; lastJetAge_=game.player().motion.jetAge; lastPushAge_=game.player().motion.pushAge; tunnel_=tunnelLook_=exitHold_=exitLift_=0; orbitAge_=10; wasLost_=game.state()==RunState::Lost;
  lastCelebrate_=game.player().motion.celebrateAge; kick_=kickVelocity_=drift_=glm::vec3(0); fovKick_=fovVelocity_=roll_=rollKick_=rollVelocity_=fovSprint_=0;
  lastStepAge_=lastLaunchAge_=10;
  target_=game.player().position+glm::vec3(0,1.1F,0);
  update(game,1.F);
}
void FollowCamera::orbit(float x,float y) {
  if(x!=0||y!=0) orbitAge_=0;
  // Inside a tunnel shot vertical mouse input tilts the low view directly.
  if(tunnel_>0) tunnelLook_=std::clamp(tunnelLook_+y*.0016F,-.06F,.4F);
  yaw_=std::remainder(yaw_-x*.0016F,6.2831853F);
  pitch_=std::clamp(pitch_+y*.0016F,.12F,1.22F);
}
void FollowCamera::zoom(float wheel) { distance_=std::clamp(distance_-wheel*.7F,3.5F,13.F); }
glm::vec2 FollowCamera::movement(float right,float forward) const {
  return glm::vec2(std::cos(yaw_),-std::sin(yaw_))*right-glm::vec2(std::sin(yaw_),std::cos(yaw_))*forward;
}
glm::vec3 FollowCamera::up() const {
  const auto forward=glm::normalize(target()-eye());
  const auto right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0)));
  return glm::normalize(glm::cross(right,forward)*std::cos(roll_)+right*std::sin(roll_));
}
void FollowCamera::update(const Game& game,float dt) {
  const auto& a=game.player().motion;
  const bool lost=game.state()==RunState::Lost;
  // Impacts: hits punch the view outward along the knockback, pickups punch it in,
  // heavy landings drop it, shutdown does all of it harder.
  // Every hit restarts hitAge, so chained hits on different hazards each kick the view.
  if(a.hitAge<lastHitAge_&&a.hitAge<.1F) {
    // Blunt blows land heavy and low; shocks are lighter but buzz (below).
    const bool electric=a.hitKind==1;
    trauma_+=electric?.3F:.85F; fovVelocity_+=electric?35.F:80.F;
    const glm::vec2 push=game.player().knockback;
    kickVelocity_+=(glm::length(push)>.01F?glm::vec3(push.x,0,push.y)/glm::length(push)*(electric?3.F:7.5F):glm::vec3(0))+glm::vec3(0,electric?1.5F:-4.5F,0);
  }
  if(a.celebrateAge<lastCelebrate_&&a.celebrateAge<.2F) { trauma_+=.42F; fovVelocity_-=85.F; kickVelocity_.y+=2.6F; }
  // Movement weight: every landing drops the view, take-off lifts it, footfalls bob and sway it.
  if(a.landAge<lastLandAge_) { kickVelocity_.y-=1.5F+2.6F*a.landStrength; if(a.landStrength>.9F) trauma_+=.32F*(a.landStrength-.6F); }
  if(a.launchAge<lastLaunchAge_) { kickVelocity_.y+=2.4F; fovVelocity_+=28.F; }
  if(a.stepAge<lastStepAge_) { kickVelocity_.y-=1.1F*a.stepStrength; rollVelocity_+=a.stepSide*.16F*a.stepStrength; }
  // Slide drop-in: the view ducks and widens with a short jolt.
  if(a.slideAge<lastSlideAge_&&a.slideAge<.1F) {
    const bool boosted=a.surge>0;
    kickVelocity_.y-=boosted?4.2F:3.2F; fovVelocity_+=boosted?85.F:50.F; trauma_+=boosted?.26F:.16F; rollVelocity_+=.25F;
    if(boosted) kickVelocity_-=glm::normalize(eye_-target_+glm::vec3(0,.001F,0))*1.8F;
  }
  // Popping out of a slide lifts the view; a speed pad punches it wide and snaps it back.
  if(a.popAge<lastPopAge_&&a.popAge<.1F) { kickVelocity_.y+=2.2F; fovVelocity_+=35.F; rollVelocity_-=.2F; }
  if(a.boostAge<lastBoostAge_&&a.boostAge<.1F) {
    const glm::vec2 v=game.player().locomotion;
    // A slide burst shakes hardest, a flight launch lifts, a stride just nudges.
    trauma_+=a.boostKind==2?.38F:a.boostKind==1?.22F:.08F; fovVelocity_+=a.boostKind==0?40.F:a.boostKind==1?80.F:95.F;
    if(a.boostKind==2) rollVelocity_+=.5F;
    // Dolly zoom on flights and slide bursts: the lens punches wide while the eye lunges in.
    if(a.boostKind>0) { fovVelocity_+=45.F; kickVelocity_-=glm::normalize(eye_-target_+glm::vec3(0,.001F,0))*2.4F; }
    if(glm::length(v)>.1F) kickVelocity_-=glm::vec3(v.x,0,v.y)/glm::length(v)*3.5F;
  }
  // Wall bounce: a small shake and a brief lens squeeze. No positional kick: near a wall that
  // read as the view suddenly zooming in.
  if(a.bounceAge<lastBounceAge_&&a.bounceAge<.1F) { trauma_+=.12F*a.bounceStrength; fovVelocity_-=20.F*a.bounceStrength; }
  if(a.takeoffAge<lastTakeoffAge_&&a.takeoffAge<.1F) { kickVelocity_.y+=2.6F; fovVelocity_+=30.F; trauma_+=.12F; }
  lastTakeoffAge_=a.takeoffAge;
  // Tricks between moves: the view rolls with the turn and lifts a little; boosted ones also
  // punch the lens and jolt.
  if(a.trickAge<lastTrickAge_&&a.trickAge<.1F) {
    const int k=a.trickKind;
    const bool boosted=k==5||k>=7||game.player().boost!=Boost::None;
    rollVelocity_+=k==1?.6F:k==3||k==5||k==6||k==7?.35F:.15F; kickVelocity_.y+=1.2F;
    if(boosted) { fovVelocity_+=45.F; trauma_+=.12F; }
  }
  lastTrickAge_=a.trickAge;
  // Shield break: a light shake, a quick lens squeeze and a small drop.
  if(a.shieldBreakAge<lastShieldBreak_&&a.shieldBreakAge<.1F) { trauma_+=.3F; fovVelocity_-=30.F; kickVelocity_.y-=1.6F; rollVelocity_+=.3F; }
  lastShieldBreak_=a.shieldBreakAge;
  // Jetpack burst: a hard punch outward, the lens thrown wide and the eye pulled back.
  if(a.jetAge<lastJetAge_&&a.jetAge<.1F) {
    trauma_+=.32F; fovVelocity_+=110.F; kickVelocity_.y+=1.4F;
    const glm::vec2 v=game.player().locomotion;
    if(glm::length(v)>.1F) kickVelocity_-=glm::vec3(v.x,0,v.y)/glm::length(v)*4.F;
  }
  lastJetAge_=a.jetAge;
  // A wall shove nudges the view back.
  if(a.pushAge<lastPushAge_&&a.pushAge<.1F) { kickVelocity_.y-=.8F; trauma_+=.08F; fovVelocity_-=15.F; }
  lastPushAge_=a.pushAge;
  // The rest of a jet boost keeps the view shaking a little.
  if(game.jetting()) trauma_=std::max(trauma_,.22F);
  lastStepAge_=a.stepAge; lastLaunchAge_=a.launchAge; lastSlideAge_=a.slideAge; lastPopAge_=a.popAge; lastBoostAge_=a.boostAge; lastBounceAge_=a.bounceAge;
  if(lost&&!wasLost_) { trauma_+=1.F; fovVelocity_+=110.F; kickVelocity_.y-=5.F; }
  if(lost&&a.slapAge<lastSlap_&&a.slapAge<.1F) { trauma_+=.4F; kickVelocity_.y-=4.5F; }
  lastSlap_=a.slapAge;
  lastHurt_=game.player().hurtCooldown; lastHitAge_=a.hitAge; lastLandAge_=a.landAge; lastCelebrate_=a.celebrateAge; wasLost_=lost;
  const float step=std::min(std::max(0.F,dt),1.F/30);
  for(int axis=0;axis<3;++axis) motionSpring(kick_[axis],kickVelocity_[axis],0,17.F,.42F,step);
  motionSpring(fovKick_,fovVelocity_,0,15.F,.45F,step);
  motionSpring(rollKick_,rollVelocity_,0,12.F,.5F,step);
  // The view widens with speed: sprint, slide, and most of all a speed-pad launch.
  const float rush=std::clamp((glm::length(game.player().locomotion)-Game::sprintSpeed)/5.F,0.F,1.F);
  fovSprint_+=(std::max({7.F*game.player().sprintBlend,10.F*game.player().slideBlend+8.F*a.surge,rush>0?7.F+7.F*rush:0.F})-fovSprint_)*(1.F-std::exp(-4.F*std::max(0.F,dt)));
  fovKick_=std::clamp(fovKick_,-9.F,14.F);
  trauma_=std::clamp(trauma_-1.8F*std::max(0.F,dt),0.F,1.F); shakeClock_+=std::max(0.F,dt);
  const float amplitude=trauma_*trauma_*.26F,t=shakeClock_;
  shake_=glm::vec3(std::sin(t*47.F)+.5F*std::sin(t*91.F+1.3F),std::sin(t*53.F+.7F)+.5F*std::sin(t*83.F),std::sin(t*41.F+2.1F))*amplitude;
  roll_=(std::sin(t*29.F)+.5F*std::sin(t*61.F+.4F))*trauma_*trauma_*.045F+rollKick_;
  // Electrocution buzz: a small, very fast jitter while the current runs.
  const float buzz=a.shockAge<.8F?std::clamp((.8F-a.shockAge)/.3F,0.F,1.F):0;
  shake_+=glm::vec3(std::sin(t*157.F),std::sin(t*173.F+1.F),std::sin(t*191.F+2.F))*.035F*buzz;
  roll_+=std::sin(t*143.F)*.018F*buzz;
  // While sliding the floor rumbles through the view, and it leans into the trailing hand.
  const float slide=game.player().slideBlend*std::clamp(glm::length(game.player().locomotion)/Game::sprintSpeed,0.F,1.F);
  shake_+=glm::vec3(std::sin(t*97.F),std::sin(t*113.F+.5F),std::sin(t*89.F+1.5F))*.014F*slide;
  roll_+=.03F*game.player().slideBlend;
  // Surge: the floor roars through the view.
  shake_+=glm::vec3(std::sin(t*109.F),std::sin(t*127.F+.7F),std::sin(t*101.F+1.9F))*.025F*a.surge;
  // Flying: a soft engine hum in the view and a lean into turns.
  const float flight=game.player().flightBlend;
  shake_+=glm::vec3(std::sin(t*71.F),std::sin(t*83.F+1.F),std::sin(t*67.F+2.F))*.02F*flight;
  roll_-=std::clamp(a.yawVelocity/6.F,-1.F,1.F)*.07F*flight;
  const auto& walker=game.player();
  drift_=glm::vec3(std::sin(t*.63F)*.03F+std::sin(t*1.7F)*.012F,std::sin(t*.81F+1.F)*.022F,std::sin(t*.57F+2.F)*.03F)
    +glm::vec3(std::cos(yaw_),0,-std::sin(yaw_))*std::sin(walker.walkPhase)*.05F*walker.moveBlend*(1-walker.airBlend);
  const float impact=a.landAge<.65F?std::sin(std::min(1.F,a.landAge/.32F)*3.14159265F)*std::exp(-a.landAge*3.F)*a.landStrength:0;
  const float brake=a.stopAge<.5F?std::sin(std::min(1.F,a.stopAge/.5F)*3.14159265F)*a.stopStrength:0;
  const auto& player=game.player();
  // The aim leads the courier slightly along its velocity.
  const float lead=.09F+.06F*player.flightBlend;
  const auto desired=player.position+glm::vec3(player.velocity.x*lead,1.1F-.15F*impact-.035F*brake-.45F*std::max(player.crouchBlend,tunnel_)-.12F*tunnel_+.3F*player.flightBlend,player.velocity.z*lead);
  target_=glm::mix(target_,desired,1.F-std::exp(-18.F*std::max(0.F,dt)));
  // If the smoothed target would be inside terrain, catch up immediately.
  if(game.collidesWithScene({target_-glm::vec3(.18F),target_+glm::vec3(.18F)})) target_=desired;
  const auto direction=[](float yaw,float pitch) { return glm::vec3(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)); };
  // The view ignores the map boundary: the outer walls (just outside room()) count as empty for
  // every camera test, so standing at the edge never pulls the view in. The renderer cuts away
  // the boundary wall on the side the camera has passed (relay.frag), so it never hides the courier.
  const Bounds room=game.room();
  const auto solid=[&](glm::vec3 lo,glm::vec3 hi) {
    lo.x=std::max(lo.x,room.min.x+.001F); lo.z=std::max(lo.z,room.min.z+.001F);
    hi.x=std::min(hi.x,room.max.x-.001F); hi.z=std::min(hi.z,room.max.z-.001F);
    return lo.x<hi.x&&lo.z<hi.z&&game.collidesWithScene({lo,hi});
  };
  // Clear length of a line from origin, probing with a box of the given half-size from start on.
  // The hit is refined by bisection, so the length changes smoothly as the line sweeps across an
  // edge instead of in 0.1 steps (steps the view would chase back and forth).
  const auto clearFrom=[&](glm::vec3 origin,glm::vec3 along,float reach,glm::vec3 radius=glm::vec3(.20F),float start=.15F) {
    const auto blocked=[&](float d) { const auto p=origin+along*std::min(d,reach); return solid(p-radius,p+radius); };
    for(float d=start;d<=reach+.05F;d+=.1F) {
      if(!blocked(d)) continue;
      float lo=std::max(start,d-.1F),hi=d;
      if(lo<d&&!blocked(lo)) for(int i=0;i<6;++i) { const float mid=.5F*(lo+hi); (blocked(mid)?hi:lo)=mid; }
      else hi=d;
      return std::max(.05F,hi-.15F);
    }
    return reach;
  };
  // Tunnel shot: crouched or sliding near a one-voxel gap (open below, blocked at head height),
  // the view lowers behind the courier only as far as the gap needs, at a steady pace, and goes
  // straight back once the line to the courier is clear again. Turning the real yaw keeps WASD
  // aligned with the view; the mouse can still look around inside the shot.
  const float frame=std::max(0.F,dt);
  orbitAge_+=frame;
  const auto lowGap=[&](glm::vec2 q) {
    const float y=player.position.y;
    return game.collidesWithScene({{q.x-.25F,y+1.15F,q.y-.25F},{q.x+.25F,y+1.6F,q.y+.25F}})&&
          !game.collidesWithScene({{q.x-.25F,y+.15F,q.y-.25F},{q.x+.25F,y+.85F,q.y+.25F}});
  };
  // The look-ahead scales with speed, so a descent can start about tunnelTime before the gap
  // whether the courier creeps or slides in; sprinting straight at one counts too. The shot also
  // looks just behind (where the view is), and keeps the aim low while it lasts, so standing up
  // at the exit cannot lift the view into the wall.
  bool gap=false; float gapAhead=-1,gapEnd=-1;
  const float speed=glm::length(player.locomotion);
  const glm::vec2 at(player.position.x,player.position.z),ahead(-std::sin(player.yaw),-std::cos(player.yaw));
  if((player.crouching||speed>Game::sprintSpeed*.9F||tunnel_>0)&&game.state()==RunState::Playing) {
    const float reach=std::max(3.2F,speed*tunnelTime);
    // Find the first gap ahead (or the one the courier is in) and how far it runs.
    bool inRun=false;
    for(float d=player.crouching||tunnel_>0?-3.F:0.F;d<=reach+.01F;d+=.4F) {
      const bool hit=lowGap(at+ahead*d);
      if(hit) { gap=true; if(d>=-.01F) { if(gapAhead<0) gapAhead=std::max(0.F,d); gapEnd=d; inRun=true; } }
      else if(inRun) break;
    }
  }
  const auto pitchFor=[&](float w) { return glm::mix(pitch_,std::clamp(.03F+tunnelLook_,.0F,pitch_),w); };
  const auto reachFor=[&](float w) { return glm::mix(distance_,2.6F,w); };
  // How low the view must go: the least weight whose line keeps a clear stretch (enough to frame
  // the courier, and at least to where the eye is now, so rising never cuts through the wall)
  // from the aim now and from just inside and at the far end of the gap ahead, so the depth a
  // long tunnel needs is known (and reached) before the courier gets there.
  float need=0;
  // Out of the gap (it is only behind now): the shot ends at once and the view goes straight
  // back up, ignoring the wall just left while that gap is still close behind (exitHold_).
  if(gap&&gapEnd<0&&(tunnel_>.05F||exitHold_>0)) exitHold_=.3F;
  exitHold_=std::max(0.F,exitHold_-frame);
  if(gap&&gapEnd>=0) {
    const auto aimAt=[&](float d) { const glm::vec2 q=at+ahead*d; return glm::vec3(q.x,player.position.y+1.1F-.57F,q.y); };
    const glm::vec3 entryAim=aimAt(std::min(gapAhead+.6F,gapEnd+.2F)),deepAim=aimAt(gapEnd+.2F);
    for(need=0;need<1.F;need+=.1F) {
      const auto v=direction(yaw_,pitchFor(need)); const float r=reachFor(need),enough=std::min(r,std::max(2.4F,actualDistance_));
      // A taller probe than the eye's leaves a margin, so shake cannot graze the ceiling.
      const glm::vec3 margin(.22F,.32F,.22F);
      if(clearFrom(target_,v,r,margin)>=enough&&(gapEnd<0||(clearFrom(entryAim,v,r,margin)>=enough&&clearFrom(deepAim,v,r,margin)>=enough))) break;
    }
    need=std::min(need,1.F);
  }
  // Lower at the steady rate (or, arriving fast, the steady rate that lands just before the gap);
  // come back up quickly once less is needed.
  if(need>tunnel_) {
    float rise=1.F/tunnelTime;
    if(gapAhead>0&&speed>.5F) rise=std::max(rise,(need-tunnel_)/std::max(.15F,gapAhead/speed-.2F));
    tunnel_=std::min(need,tunnel_+rise*frame);
  } else tunnel_=std::max(need,tunnel_-frame/tunnelReturn);
  if(tunnel_<=0) tunnelLook_=0;
  // Swing behind the courier while the shot is still lowering into place (pausing while the
  // player turns the view); once it has settled the mouse turns the view freely.
  // Arriving fast, the swing is paced to finish just before the gap, like the descent.
  if(need>tunnel_+.01F&&orbitAge_>.15F) {
    const float behind=std::remainder(player.yaw-yaw_,6.2831853F);
    float swing=2.6F;
    if(gapAhead>0&&speed>.5F) swing=std::max(swing,std::abs(behind)/std::max(.15F,gapAhead/speed-.2F));
    yaw_=std::remainder(yaw_+std::clamp(behind,-swing*frame,swing*frame),6.2831853F);
  }
  // Chase cam while flying: the view drops lower and trails further behind as the speed builds.
  const float chase=game.player().flightBlend*std::clamp(speed/Game::padSpeed,0.F,1.F);
  const float basePitch=std::max(.02F,pitchFor(tunnel_)-.09F*chase),viewDistance=reachFor(tunnel_)+1.3F*chase;
  // Leaving a gap the view goes straight up, steep enough to see over the block just left (the
  // least extra pitch that clears the line), and settles to the player's pitch as the courier
  // runs on. Only here: near ordinary walls the view keeps its angle.
  float lift=0;
  if(exitHold_>0) for(;lift<.9F&&clearFrom(target_,direction(yaw_,std::min(1.3F,basePitch+lift)),viewDistance,glm::vec3(.08F),.9F)<viewDistance-.05F;lift+=.05F) {}
  exitLift_=glm::mix(exitLift_,lift,1.F-std::exp(-(lift>exitLift_?18.F:3.F)*frame));
  const float viewPitch=std::min(1.3F,basePitch+exitLift_);
  const auto view=direction(yaw_,viewPitch);
  // Distance. Hugging a wall changes nothing: the line test is thin and starts clear of the
  // courier, so only a wall truly between the view and the courier brings the view in, and then
  // smoothly. The eye may never sit inside geometry or outside the room; when it would, it backs
  // off just to that boundary (which moves smoothly), so it glides along walls instead of jumping.
  // Right after leaving a gap the wall just left is ignored, so the view swings straight back up.
  float clear=exitHold_>0?viewDistance:clearFrom(target_,view,viewDistance,glm::vec3(.08F),.9F);
  // The limit (the farthest spot outside geometry, searched back from the full distance and refined
  // by bisection) caps the distance the view eases toward. Stepping back only after easing past
  // it made the view ease out and snap back in every frame: a visible tremble at some angles.
  float limit=viewDistance;
  const auto forbidden=[&](float d) { const auto p=target_+view*d; return solid(p-glm::vec3(.12F),p+glm::vec3(.12F)); };
  // The farthest allowed spot at or below d (refined to a few millimetres).
  const auto backOff=[&](float d) {
    if(!forbidden(d)) return d;
    float hi=d,lo=d;
    while(lo>.3F&&forbidden(lo)) { hi=lo; lo=std::max(.3F,lo-.05F); }
    if(forbidden(lo)) return .3F;
    for(int i=0;i<6;++i) { const float mid=.5F*(lo+hi); (forbidden(mid)?hi:lo)=mid; }
    return lo;
  };
  if(exitHold_<=0) { limit=backOff(viewDistance); clear=std::min(clear,limit); }
  actualDistance_=glm::mix(actualDistance_,clear,1.F-std::exp(-(clear<actualDistance_?5.F:exitHold_>0?12.F:4.F)*frame));
  // Never past the boundary (it moves smoothly with the aim, so holding to it is steady), and
  // never inside geometry nearer in.
  actualDistance_=std::min(actualDistance_,limit);
  if(exitHold_<=0) actualDistance_=backOff(actualDistance_);
  eye_=target_+view*actualDistance_;
}
}
