#include "core/RelayGame.h"
#include "core/ButtonState.h"
#include "core/Collision.h"
#include "graphics/RelayModels.h"
#include "core/FollowCamera.h"
#include "core/ArrivalSequence.h"
#include "core/LevelTransition.h"
#include "core/KeyBindings.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <glm/gtc/matrix_transform.hpp>
using namespace relay;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(std::string(#x)+" at line "+std::to_string(__LINE__)); } while(false)
bool near(float a,float b,float e=.015F) { return std::abs(a-b)<e; }
void tick(Game& g,float duration,Input input={}) {
  const int n=int(std::ceil(duration*120));
  for(int i=0;i<n;++i) { g.update(duration/float(n),input); input.jumpPressed=input.interactPressed=input.crouchPressed=input.sprintPressed=false; }
}
void settle(Game& g,glm::vec3 p) { g.warpPlayer(p); tick(g,.025F); }
// The eye inside level geometry. The boundary walls (just outside room()) do not count: the view
// may pass the map edge, where they are see-through.
bool eyeInGeometry(const Game& g,glm::vec3 eye) {
  const Bounds r=g.room(); glm::vec3 lo=eye-glm::vec3(.05F),hi=eye+glm::vec3(.05F);
  lo.x=std::max(lo.x,r.min.x+.001F); lo.z=std::max(lo.z,r.min.z+.001F); hi.x=std::min(hi.x,r.max.x-.001F); hi.z=std::min(hi.z,r.max.z-.001F);
  return lo.x<hi.x&&lo.z<hi.z&&g.collidesWithScene({lo,hi});
}
void collectAll(Game& g) { const auto cores=g.cores(); for(auto c:cores) { settle(g,{c.position.x,1,c.position.z}); tick(g,.5F,{{0,0},false,true}); } }
void routeTo(Game& g,glm::vec3 target) {
  // Half-unit grid over the whole 64-unit world, searched inside room().
  const int size=128; const auto room=g.room();
  auto index=[](glm::vec3 p){ return int(std::floor(p.z*2))*128+int(std::floor(p.x*2)); };
  const int start=index(g.player().position), goal=index(target);
  std::vector<int> parent(size*size,-1); std::queue<int> q; q.push(start); parent[std::size_t(start)]=start;
  while(!q.empty()&&parent[std::size_t(goal)]<0) {
    int current=q.front(); q.pop(); int x=current%size,z=current/size;
    for(auto d:std::vector<glm::ivec2>{{1,0},{-1,0},{0,1},{0,-1}}) {
      int nx=x+d.x,nz=z+d.y;
      if(nx<int(room.min.x*2)||nx>=int(room.max.x*2)||nz<int(room.min.z*2)||nz>=int(room.max.z*2)) continue;
      int next=nz*size+nx; if(parent[std::size_t(next)]>=0) continue;
      glm::vec3 feet((nx+.5F)*.5F,1,(nz+.5F)*.5F);
      if(g.world().collides({feet+glm::vec3(-Game::playerRadius,0,-Game::playerRadius),feet+glm::vec3(Game::playerRadius,Game::playerHeight,Game::playerRadius)})) continue;
      // Walk around relay pedestals, leaving a conservative clearance margin.
      bool nearRelay=false;
      for(const auto& sw:g.switches()) if(glm::distance(glm::vec2(feet.x,feet.z),glm::vec2(sw.position.x,sw.position.z))<1.3F) nearRelay=true;
      // Checkpoint beacons are solid too: keep clear of the pad.
      for(const auto& cp:g.checkpoints()) if(glm::distance(glm::vec2(feet.x,feet.z),glm::vec2(cp.position.x,cp.position.z))<1.75F) nearRelay=true;
      // Sealed blast doors are walls to the route (their plates are optional shortcuts).
      const Bounds body{feet+glm::vec3(-Game::playerRadius,0,-Game::playerRadius),feet+glm::vec3(Game::playerRadius,Game::playerHeight,Game::playerRadius)};
      for(const auto& d:g.doors()) if(d.open<.99F&&body.intersects(d.bounds())) nearRelay=true;
      if(nearRelay) continue;
      parent[std::size_t(next)]=current; q.push(next);
    }
  }
  CHECK(parent[std::size_t(goal)]>=0);
  std::vector<int> path; int cell=goal;
  while(cell!=start) { path.push_back(cell); cell=parent[std::size_t(cell)]; }
  path.push_back(start); std::reverse(path.begin(),path.end());
  int waits=0;
  for(int next:path) {
    const glm::vec2 destination((next%size+.5F)*.5F,(next/size+.5F)*.5F);
    int attempts=0;
    while(g.state()==RunState::Playing) {
      auto p=g.player().position; auto delta=destination-glm::vec2(p.x,p.z);
      if(glm::length(delta)<.055F) break;
      // Wait at the edge while the next cell is over a live trap or near a passing hazard.
      const auto danger=[&](glm::vec2 q) {
        for(std::size_t t=0;t<g.traps().size();++t) {
          const auto& a=g.traps()[t].area; const auto phase=g.trapPhase(t);
          if((phase==TrapPhase::Active||phase==TrapPhase::Warning)&&q.x>a.min.x-.35F&&q.x<a.max.x+.35F&&q.y>a.min.z-.35F&&q.y<a.max.z+.35F) return true;
        }
        for(const auto& m:g.movers()) if(std::abs(m.position.x-q.x)<1.6F&&std::abs(m.position.z-q.y)<1.6F) return true;
        return false;
      };
      if(danger(destination)&&!danger({p.x,p.z})&&waits<6000) { g.update(1.F/120.F,{}); ++waits; continue; }
      g.update(1.F/120.F,{glm::normalize(delta),false,false});
      CHECK(!g.playerIntersectsScene());
      if(++attempts>=360) throw std::runtime_error("route stuck: target="+std::to_string(destination.x)+","+std::to_string(destination.y)+" player="+std::to_string(p.x)+","+std::to_string(p.z)+" health="+std::to_string(g.player().health));
    }
    if(g.state()==RunState::Lost) throw std::runtime_error("route lost at "+std::to_string(destination.x)+","+std::to_string(destination.y)+" bounces="+std::to_string(g.player().motion.bounceAge)+" hit="+std::to_string(g.player().motion.hitAge));
  }
}
int main(int argc,char** argv) {
  const std::map<std::string,std::function<void()>> tests{
    {"turn_body",[]{
      Game g; tick(g,.3F,{{0,-1}}); const float start=g.player().yaw;
      g.update(1.F/120,{{1,0}}); CHECK(std::abs(g.player().yaw-start)<.02F);
      tick(g,.12F,{{1,0}}); CHECK(g.player().yaw<-.25F&&g.player().yaw>-1.45F);
      CHECK(g.player().motion.lookYaw<-.1F&&g.player().motion.torsoYaw<-.05F);
      tick(g,.65F,{{1,0}}); CHECK(std::abs(g.player().yaw+1.570796F)<.035F);
      float previous=g.player().yaw;
      for(int i=0;i<100;++i) { g.update(1.F/120,{{-1,0}}); const float step=std::remainder(g.player().yaw-previous,6.2831853F); CHECK(std::abs(step)<.068F); previous=g.player().yaw; }
      CHECK(std::abs(std::remainder(g.player().yaw-1.570796F,6.2831853F))<.04F);
    }},
    {"weighted_impact",[]{
      const auto land=[](float height) { Game g; g.warpPlayer({7,height,24}); for(int i=0;i<300&&!g.player().grounded;++i) g.update(1.F/120,{}); CHECK(g.player().grounded); return g; };
      auto low=land(2.F),high=land(8.F);
      CHECK(high.player().motion.landStrength>low.player().motion.landStrength+.65F);
      SceneModels m; int pelvis=-1; for(std::size_t i=0;i<m.robot().nodes.size();++i) if(m.robot().nodes[i].name=="pelvis_root") pelvis=int(i);
      CHECK(pelvis>=0); tick(high,.10F); tick(low,.10F);
      m.poseRobot(high.player()); const float crouch=m.robot().nodes[pelvis].offset.y;
      m.poseRobot(low.player()); CHECK(crouch<m.robot().nodes[pelvis].offset.y-.07F);
      tick(high,.75F); m.poseRobot(high.player()); CHECK(m.robot().nodes[pelvis].offset.y>crouch+.12F);
      CHECK(high.player().health==5&&!high.playerIntersectsScene());
    }},
    {"arrival",[]{
      ArrivalSequence a; CHECK(!a.active());
      a.start(); a.update(1,false,false); CHECK(a.elapsed==0&&a.active());
      a.update(.5F,false,true); CHECK(near(a.elapsed,.5F));
      a.update(.01F,true,true); CHECK(!a.active());
      a.start(); for(int i=0;i<241;++i) a.update(1.F/60,false,true); CHECK(!a.active());
      a.start(); a.cancel(); CHECK(!a.active());
      SceneModels m; Player p; p.motion.wakeTime=0; m.poseRobot(p);
      float dark=0,bright=0; for(const auto& n:m.robot().nodes) dark+=n.emission;
      p.motion.wakeTime=3.9F; m.poseRobot(p); for(const auto& n:m.robot().nodes) bright+=n.emission;
      CHECK(bright>dark+1.F);
    }},
    {"expressive_bounds",[]{
      SceneModels m;
      for(int clip=0;clip<14;++clip) for(int f=0;f<270;++f) {
        auto p=SceneModels::previewPlayer(f/60.F,clip); m.poseRobot(p);
        const auto tr=m.robot().transforms(glm::mat4(1));
        for(std::size_t n=0;n<tr.size();++n) if(m.robot().nodes[n].visible)
          for(float x:{-.5F,.5F}) for(float y:{-.5F,.5F}) for(float z:{-.5F,.5F}) {
            const auto v=glm::vec3(tr[n]*glm::vec4(glm::vec3(x,y,z)*m.robot().nodes[n].dimensions,1));
            if(glm::length(glm::vec2(v.x,v.z))>Game::playerRadius+.012F||v.y<-.015F||v.y>Game::playerHeight+.01F)
              throw std::runtime_error("clip="+std::to_string(clip)+" frame="+std::to_string(f)+" part="+m.robot().nodes[n].name+" xyz="+std::to_string(v.x)+","+std::to_string(v.y)+","+std::to_string(v.z));
          }
      }
    }},
    {"animation_events",[]{
      Game g; tick(g,.2F,{{0,-1}}); CHECK(g.player().motion.startAge<.25F);
      tick(g,.05F,{{0,1}}); CHECK(g.player().motion.pivotAge<.1F);
      tick(g,.15F,{{0,1}}); tick(g,.1F); CHECK(g.player().motion.stopAge<.15F);
      tick(g,.7F,{{0,0},true,false,false,true}); CHECK(g.player().motion.launchAge<.75F);
      tick(g,.1F); CHECK(g.player().motion.landAge<.25F&&g.player().motion.landStrength>.5F);
      settle(g,{9,1,22}); tick(g,.3F,{{0,0},false,true}); CHECK(g.player().motion.celebrateAge<.12F);
      g.reset(0); CHECK(g.player().motion.celebrateAge>=10&&!g.player().motion.victorious);
      collectAll(g); settle(g,{25,1,9}); tick(g,.8F); tick(g,1,{{0,-1}});
      CHECK(g.state()==RunState::Won&&g.player().motion.victorious);
      const auto timer=g.timeRemaining(); const auto before=g.player().motion.victoryAge;
      tick(g,.5F); CHECK(near(g.timeRemaining(),timer)); CHECK(g.player().motion.victoryAge>before+.45F);
    }},
    {"foot_plant",[]{
      SceneModels m; int left=-1,right=-1;
      for(std::size_t i=0;i<m.robot().nodes.size();++i) { if(m.robot().nodes[i].name=="left_ankle") left=int(i); if(m.robot().nodes[i].name=="right_ankle") right=int(i); }
      CHECK(left>=0&&right>=0); Game g; tick(g,.4F,{{0,-1}});
      std::array<glm::vec3,2> previous{}; std::array<bool,2> wasStance{}; int checked=0;
      for(int f=0;f<100;++f) {
        g.update(1.F/120,{{0,-1}}); m.poseRobot(g.player());
        const auto tr=m.robot().transforms(glm::translate(glm::mat4(1),g.player().position));
        for(int i=0;i<2;++i) {
          const float phase=std::fmod(g.player().walkPhase+i*3.14159265F,6.2831853F)/6.2831853F;
          const bool stance=phase>.08F&&phase<.30F;
          const glm::vec3 ankle(tr[std::size_t(i==0?left:right)][3]);
          if(stance&&wasStance[i]) { CHECK(std::abs(ankle.z-previous[i].z)<.005F); ++checked; }
          previous[i]=ankle; wasStance[i]=stance;
        }
      }
      CHECK(checked>10);
    }},
    {"input_focus",[]{
      ButtonState b; ButtonState::Snapshot keys{}; b.update(keys,true);
      keys[87]=true; b.update(keys,true); CHECK(b.held(87)&&b.pressed(87));
      b.update(keys,true); CHECK(b.held(87)&&!b.pressed(87));
      b.update(keys,false); CHECK(!b.held(87)&&!b.pressed(87)&&!b.released(87));
      keys[32]=true; b.update(keys,true); CHECK(b.held(32)&&!b.pressed(32));
      keys.fill(false); b.update(keys,true); CHECK(b.released(32)&&!b.held(87));
      keys[32]=true; b.update(keys,true); CHECK(b.pressed(32));
    }},
    {"control_feel",[]{
      Game g; g.update(1.F/60,{{1,0}}); CHECK(g.player().velocity.x>0&&g.player().velocity.x<2.F);
      tick(g,.15F,{{1,0}}); CHECK(near(g.player().velocity.x,Game::speed));
      const auto before=g.player().position; tick(g,.1F); CHECK(near(g.player().velocity.x,0));
      CHECK(g.player().position.x-before.x<.20F); CHECK(g.player().moveBlend<.5F);
      tick(g,.2F,{{1,0}}); tick(g,.1F,{{-1,0}}); CHECK(g.player().velocity.x<0);
      tick(g,.15F,{{-1,0}}); CHECK(near(g.player().velocity.x,-Game::speed));
      tick(g,.5F); CHECK(g.player().moveBlend<.01F);
    }},
    {"jump_buffer",[]{
      Game g; g.warpPlayer({7,2,24});
      while(g.player().position.y>1.22F) g.update(1.F/120,{});
      CHECK(!g.player().grounded); g.update(1.F/120,{{0,0},true,false,false,true});
      bool rebounded=false;
      for(int i=0;i<15;++i) { g.update(1.F/120,{{0,0},false,false,false,true}); if(g.player().velocity.y>8) rebounded=true; }
      CHECK(rebounded); CHECK(g.player().position.y>1.3F);
    }},
    {"coyote",[]{
      Game g; settle(g,{11.5F,2,19.5F}); CHECK(g.player().grounded);
      for(int i=0;i<150&&g.player().grounded;++i) g.update(1.F/120,{{0,1}});
      CHECK(!g.player().grounded&&g.player().coyoteTime>0);
      tick(g,.04F,{{0,1}}); g.update(1.F/120,{{0,1},true,false,false,true});
      CHECK(g.player().velocity.y>8); CHECK(g.player().jumpActive);
      Game late; settle(late,{11.5F,2,19.5F});
      for(int i=0;i<150&&late.player().grounded;++i) late.update(1.F/120,{{0,1}});
      tick(late,.14F,{{0,1}}); late.update(1.F/120,{{0,1},true,false,false,true}); CHECK(late.player().velocity.y<0);
    }},
    {"frame_rates",[]{
      glm::vec3 reference{};
      for(int fps:{30,60,144}) {
        Game g; float apex=1;
        for(int i=0;i<fps/2;++i) { g.update(1.F/fps,{{1,0},i==0,false,false,true}); apex=std::max(apex,g.player().position.y); }
        if(fps==30) reference=g.player().position;
        else CHECK(glm::distance(g.player().position,reference)<.05F);
        CHECK(apex>2.69F&&apex<2.79F);
      }
    }},
    {"trap_edges",[]{
      Game g; const auto trap=g.traps()[0];
      CHECK(!trap.touchesFeet({14.70F,1,12.5F},Game::footRadius));
      CHECK(trap.touchesFeet({14.76F,1,12.5F},Game::footRadius));
      CHECK(!trap.touchesFeet({14.80F,1,11.80F},Game::footRadius));
      CHECK(!trap.touchesFeet({17,1.12F,13},Game::footRadius));
      CHECK(trap.touchesFeet({17,1.10F,13},Game::footRadius));
      tick(g,4.1F); g.warpPlayer({14.70F,1,12.5F}); tick(g,.02F); CHECK(g.player().health==5);
      g.warpPlayer({14.76F,1,12.5F}); tick(g,.01F); CHECK(g.player().health==4);
    }},
    {"rounded_collision",[]{
      const Bounds box{{0,0,0},{1,2,1}};
      CHECK(!cylinderIntersectsBox({1.55F,0,1.55F},.7F,2,box));
      CHECK(cylinderIntersectsBox({1.45F,0,1.45F},.7F,2,box));
      CHECK(!cylinderIntersectsBox({.5F,2.001F,.5F},.7F,2,box));
      Game g; settle(g,{14.55F,1,20.55F}); CHECK(!g.playerIntersectsScene());
      tick(g,.3F,{{-1,-1}}); CHECK(!g.playerIntersectsScene());
    }},
    {"mover_parts",[]{
      MovingHazard h; h.position={0,1.55F,0}; h.halfSize={.65F,.55F,.65F};
      CHECK(!h.intersectsPlayer({1.18F,1,1.18F},Game::playerRadius,Game::playerHeight));
      CHECK(h.intersectsPlayer({1.10F,1,1.10F},Game::playerRadius,Game::playerHeight));
      // The empty space above a body corner is not part of the centre lamp.
      CHECK(!h.intersectsPlayer({.6F,2.12F,.6F},.04F,2));
      CHECK(h.intersectsPlayer({0,2.12F,0},.04F,2));
      const auto parts=h.collisionParts(); CHECK(near(parts[0].max.y,2.1F)); CHECK(near(parts[2].max.y,2.17F));
    }},
    {"pose_transitions",[]{
      Game g; SceneModels models; models.poseRobot(g.player()); auto previous=models.robot().nodes;
      float maxDelta=0; std::string worst;
      for(int i=0;i<180;++i) {
        Input input; input.movement=i<85?glm::vec2(1,0):glm::vec2(0); input.sprintHeld=i>20&&i<70;
        input.jumpPressed=i==35; input.jumpHeld=i>=35&&i<65;
        g.update(1.F/120,input); models.poseRobot(g.player());
        const auto& nodes=models.robot().nodes;
        for(std::size_t n=0;n<nodes.size();++n) { const float d=glm::length(nodes[n].rotation-previous[n].rotation); if(d>maxDelta) { maxDelta=d; worst=nodes[n].name+" frame="+std::to_string(i); } }
        previous=nodes;
      }
      // Shorter legs cycle faster for the same ground speed, so the pop limit scales with the model.
      if(maxDelta>=.22F/Game::modelScale) throw std::runtime_error("pose jump="+std::to_string(maxDelta)+" "+worst); CHECK(g.player().moveBlend<.01F);
    }},
    {"crouch",[]{
      Game walk,sneak; Input hold; hold.crouchHeld=true;
      tick(sneak,.2F,hold); CHECK(sneak.player().crouching); CHECK(sneak.bodyHeight()<Game::playerHeight-.5F);
      Input move; move.movement={1,0}; Input creep=move; creep.crouchHeld=true; creep.sprintHeld=true;
      tick(walk,.5F,move); tick(sneak,.5F,creep);
      CHECK(!sneak.player().sprinting); CHECK(sneak.player().position.x-7<.6F*(walk.player().position.x-7));
      tick(sneak,.3F); CHECK(!sneak.player().crouching); CHECK(sneak.bodyHeight()==Game::playerHeight); CHECK(!sneak.playerIntersectsScene());
      tick(sneak,.2F,hold); Input jump=hold; jump.jumpPressed=true; tick(sneak,.05F,jump); CHECK(!sneak.player().crouching&&!sneak.player().grounded);
    }},
    {"checkpoint",[]{
      Game g; CHECK(g.checkpoints().size()==2); const auto checkpoint=g.checkpoints()[0];
      settle(g,checkpoint.respawn); tick(g,.1F);
      CHECK(g.availableInteraction()==Interaction::Checkpoint);
      tick(g,.01F,{{0,0},false,true}); tick(g,.5F);
      CHECK(g.checkpoints()[0].active); CHECK(g.activeCheckpoint()==0); CHECK(g.availableInteraction()!=Interaction::Checkpoint);
      // Exhaust the armour on the trap; after the fall the courier reboots at the checkpoint.
      for(int hit=0;hit<5;++hit) { g.warpPlayer({7,1,24}); tick(g,1.2F); while(g.trapPhase(0)!=TrapPhase::Active) tick(g,.05F); g.warpPlayer({17,1,13}); tick(g,.01F); }
      CHECK(g.state()==RunState::Lost); CHECK(g.respawning()); const float before=g.timeRemaining();
      tick(g,Game::respawnDelay+.1F);
      CHECK(g.state()==RunState::Playing); CHECK(g.player().health==5); CHECK(!g.playerIntersectsScene());
      CHECK(glm::distance(g.player().position,checkpoint.respawn)<.3F);
      CHECK(near(g.timeRemaining(),before-Game::respawnPenalty,.2F));
      CHECK(g.collectedCount()==0&&g.checkpoints()[0].active);
    }},
    {"checkpoint_solid_return",[]{
      Game g; const auto checkpoint=g.checkpoints()[0];
      // Walking straight at the pylon stops at the pad's edge instead of passing through.
      settle(g,checkpoint.position+glm::vec3(-3,0,0)); tick(g,1.5F,{{1,0}});
      CHECK(g.player().position.x<checkpoint.position.x-.76F-Game::playerRadius+.05F); CHECK(!g.playerIntersectsScene());
      // Return to the entry point (none active), then to the checkpoint once activated; armour is kept.
      settle(g,checkpoint.respawn); tick(g,.01F,{{0,0},false,true}); tick(g,.5F); CHECK(g.activeCheckpoint()==0);
      settle(g,{9,1,23}); const float clock=g.timeRemaining(); const int health=g.player().health;
      g.returnToCheckpoint(); tick(g,.05F);
      CHECK(glm::distance(g.player().position,checkpoint.respawn)<.3F); CHECK(g.player().health==health);
      CHECK(near(g.timeRemaining(),clock-.05F,.05F)); CHECK(!g.playerIntersectsScene());
    }},
    {"hit_sources",[]{
      Game g; tick(g,4.1F); g.warpPlayer({17,1,13}); tick(g,.02F); CHECK(g.player().health==4);
      // The same plate is held off for half a second...
      g.warpPlayer({17,1,13}); tick(g,.3F); CHECK(g.player().health==4);
      // ...but a different hazard (here the fall reset) hits immediately.
      g.warpPlayer({7,-6,24}); tick(g,.02F); CHECK(g.player().health==3);
      // After the half second the first plate can hit again.
      tick(g,.6F); while(g.trapPhase(0)!=TrapPhase::Active) tick(g,.05F); g.warpPlayer({17,1,13}); tick(g,.02F); CHECK(g.player().health==2);
    }},
    {"slide",[]{
      // A clear lane along the south wall of the courtyard.
      const glm::vec3 lane{5.5F,1,7};
      Input dash; dash.movement={1,0}; dash.sprintHeld=true; Input drop=dash; drop.crouchPressed=true;
      const auto speed=[](const Game& g) { return glm::length(g.player().locomotion); };
      Game g; settle(g,lane); tick(g,.35F,dash); CHECK(g.player().sprinting);
      tick(g,1.F/120,drop); CHECK(g.player().sliding&&g.player().crouching); CHECK(speed(g)>Game::sprintSpeed+2.5F);
      CHECK(g.bodyHeight()==Game::crouchHeight);
      float last=speed(g),slid=1.F/120; const float start=g.player().position.x;
      while(g.player().sliding&&slid<3) { tick(g,1.F/120,dash); CHECK(speed(g)<=last+1e-4F); last=speed(g); slid+=1.F/120; }
      CHECK(slid>.8F&&slid<2.1F); CHECK(g.player().position.x-start>8.F);
      // Holding sprint stands straight back up into a run.
      tick(g,.12F,dash); CHECK(!g.player().crouching&&g.player().sprinting);
      // Inside the cool-down a second slide gets no boost.
      tick(g,1.F/120,drop); CHECK(g.player().sliding); CHECK(speed(g)<Game::sprintSpeed+.05F);
      // Walking speed only crouches.
      Game walk; settle(walk,lane); Input stroll; stroll.movement={1,0}; tick(walk,.5F,stroll); stroll.crouchPressed=true; tick(walk,.1F,stroll);
      CHECK(walk.player().crouching&&!walk.player().sliding);
      // Crouch again mid-slide stands up at speed; a slide jump carries its speed through the air.
      Game cancel; settle(cancel,lane); tick(cancel,.5F,dash); tick(cancel,1.F/120,drop); tick(cancel,.15F,dash);
      tick(cancel,1.F/120,drop); CHECK(!cancel.player().sliding&&!cancel.player().crouching); CHECK(speed(cancel)>Game::sprintSpeed);
      Game hop; settle(hop,lane); tick(hop,.5F,dash); tick(hop,1.F/120,drop); tick(hop,.1F,dash);
      Input jump=dash; jump.jumpPressed=true; jump.jumpHeld=true; tick(hop,1.F/120,jump); jump.jumpPressed=false; tick(hop,.25F,jump);
      CHECK(!hop.player().grounded&&!hop.player().sliding); CHECK(speed(hop)>Game::sprintSpeed+1.F);
      // A slide carries through a one-voxel gap, and the courier stands at the far side.
      Game gap; for(int x=12;x<14;++x) for(int z=6;z<8;++z) gap.setVoxelForDebug(x,2,z,2);
      settle(gap,lane); tick(gap,.4F,dash); CHECK(gap.player().position.x<12-Game::playerRadius);
      tick(gap,1.F/120,drop); tick(gap,1.5F,dash); CHECK(gap.player().position.x>14+Game::playerRadius); CHECK(!gap.playerIntersectsScene());
      tick(gap,.2F,dash); CHECK(!gap.player().crouching);
      // The skid pose stays inside the crouched collider and the body radius.
      SceneModels m;
      for(int f=0;f<144;++f) {
        Player p=SceneModels::previewPlayer(float(f)/60.F,14); m.poseRobot(p); const auto tr=m.robot().transforms(glm::mat4(1));
        for(std::size_t n=0;n<tr.size();++n) if(m.robot().nodes[n].visible)
          for(float x:{-.5F,.5F}) for(float y:{-.5F,.5F}) for(float z:{-.5F,.5F}) {
            const auto v=glm::vec3(tr[n]*glm::vec4(glm::vec3(x,y,z)*m.robot().nodes[n].dimensions,1));
            const float limit=p.crouchBlend>.99F?Game::crouchHeight+.02F:Game::playerHeight+.01F;
            if(v.y>limit||v.y<-.015F||glm::length(glm::vec2(v.x,v.z))>Game::playerRadius+.01F)
              throw std::runtime_error("slide pose: "+m.robot().nodes[n].name+" frame="+std::to_string(f)+" xyz="+std::to_string(v.x)+","+std::to_string(v.y)+","+std::to_string(v.z));
          }
      }
    }},
    {"lives",[]{
      Game g; g.setTimeForDebug(900); CHECK(g.lives()==Game::livesPerLevel);
      const auto destroy=[&]{
        for(int hit=0;hit<5;++hit) { g.warpPlayer({7,1,24}); tick(g,1.2F); while(g.trapPhase(0)!=TrapPhase::Active) tick(g,.05F); g.warpPlayer({17,1,13}); tick(g,.01F); }
        CHECK(g.state()==RunState::Lost);
      };
      for(int left=Game::livesPerLevel-1;left>=1;--left) {
        destroy(); CHECK(g.lives()==left); CHECK(g.respawning()&&!g.outOfLives());
        tick(g,Game::respawnDelay+.1F); CHECK(g.state()==RunState::Playing&&g.player().health==5);
      }
      // The last life does not reboot: the level waits for a restart, which restores all three.
      destroy(); CHECK(g.lives()==0); CHECK(g.outOfLives()&&!g.respawning());
      tick(g,4.F); CHECK(g.state()==RunState::Lost);
      g.reset(0); CHECK(g.lives()==Game::livesPerLevel&&g.state()==RunState::Playing&&!g.outOfLives());
    }},
    {"slide_cancel",[]{
      Game g; settle(g,{5.5F,1,7}); Input dash; dash.movement={1,0}; dash.sprintHeld=true; Input drop=dash; drop.crouchPressed=true;
      tick(g,.5F,dash); tick(g,1.F/120,drop); tick(g,.6F,dash); CHECK(g.player().sliding);
      // Holding sprint does nothing; pressing it springs straight into a run at sprint speed.
      Input cancel=dash; cancel.sprintPressed=true; tick(g,1.F/120,cancel);
      CHECK(!g.player().sliding&&!g.player().crouching&&g.player().sprinting);
      CHECK(glm::length(g.player().locomotion)>=Game::sprintSpeed-.05F); CHECK(g.player().motion.popAge<.05F);
      tick(g,.3F,dash); CHECK(g.player().sprinting&&!g.playerIntersectsScene());
      // Under a low ceiling the cancel waits for headroom.
      Game low; for(int x=10;x<20;++x) for(int z=6;z<8;++z) low.setVoxelForDebug(x,2,z,2);
      settle(low,{5.5F,1,7}); tick(low,.4F,dash); tick(low,1.F/120,drop); tick(low,.25F,dash);
      CHECK(low.player().sliding&&low.player().position.x>10.5F); tick(low,1.F/120,cancel); CHECK(low.player().crouching);
    }},
    {"speed_pad",[]{
      Game g; CHECK(g.speedPads().size()==2); const auto pad=g.speedPads()[0];
      settle(g,{6,1,26.9F}); Input walk; walk.movement={1,0}; float peak=0;
      for(int i=0;i<120;++i) { g.update(1.F/120,walk); peak=std::max(peak,glm::length(g.player().locomotion)); CHECK(!g.playerIntersectsScene()); }
      CHECK(peak>=Game::strideSpeed-.1F); CHECK(g.player().position.x>pad.area.max.x+2);
      // Still carrying, bleeding off toward walking speed.
      const float v=glm::length(g.player().locomotion); CHECK(v>Game::speed+1.F&&v<Game::padSpeed);
      CHECK(g.player().motion.boostAge<1.F);
      // Pulling back against a launch brakes as usual.
      Game b; settle(b,{6,1,26.9F}); tick(b,.6F,walk); Input back; back.movement={-1,0}; tick(b,.3F,back); CHECK(b.player().locomotion.x<0);
      // Walking back across the strip against its arrow does not launch.
      tick(b,.8F,back); CHECK(b.player().locomotion.x<0&&b.player().position.x<pad.area.min.x);
    }},
    {"pressure_door",[]{
      Game g; g.reset(1); CHECK(g.doors().size()==1&&g.plates().size()==1);
      const auto door=g.doors()[0]; const auto plate=g.plates()[0];
      Input east; east.movement={1,0};
      // Sealed: the courier stops at the panels.
      settle(g,{9.3F,1,24}); tick(g,1.F,east); CHECK(g.player().position.x<door.centre.x-door.half.x-Game::playerRadius+.05F);
      // The plate opens it; it stays open for the hold after stepping off, then seals again.
      settle(g,plate.position); tick(g,.1F); CHECK(g.plates()[0].down); tick(g,.5F); CHECK(g.doors()[0].open>.99F);
      settle(g,{9.3F,1,24}); tick(g,1.F,east); CHECK(g.player().position.x>door.centre.x+door.half.x+Game::playerRadius);
      CHECK(!g.playerIntersectsScene());
      tick(g,Game::doorHold+1.F); CHECK(g.doors()[0].open<.01F);
      // It never closes on the courier standing in the doorway.
      settle(g,plate.position); tick(g,.6F); settle(g,{door.centre.x,1,door.centre.z}); tick(g,Game::doorHold+1.F);
      CHECK(g.doors()[0].open>.99F); CHECK(!g.playerIntersectsScene());
    }},
    {"wall_bounce",[]{
      // Sprinting into the courtyard wall rebounds once and turns the courier with it.
      Game g; settle(g,{14,1,22}); Input dash; dash.movement={1,0}; dash.sprintHeld=true;
      float minimum=0,maxTurn=0; int bounces=0; float last=0;
      for(int i=0;i<240;++i) {
        g.update(1.F/120,dash); const float vx=g.player().locomotion.x;
        if(last>0&&vx<0) ++bounces; last=vx; minimum=std::min(minimum,vx);
        if(bounces>0) maxTurn=std::max(maxTurn,std::abs(std::remainder(g.player().yaw+1.5708F,6.2832F)));
        CHECK(!g.playerIntersectsScene());
      }
      CHECK(minimum<-2.5F); CHECK(maxTurn>.4F); CHECK(bounces<=2);
      CHECK(g.player().position.x<19-Game::playerRadius+.02F);
      // Creeping into it just stops.
      Game c; settle(c,{17,1,22}); Input creep; creep.movement={1,0}; creep.crouchHeld=true; float lowest=0;
      for(int i=0;i<240;++i) { c.update(1.F/120,creep); lowest=std::min(lowest,c.player().locomotion.x); }
      CHECK(lowest>-.01F);
    }},
    {"wall_jump",[]{
      // Boosted, a jump against the courtyard wall kicks off it with the wall-kick trick; without
      // a boost it is an ordinary jump.
      const auto run=[](bool boost) {
        Game g; settle(g,{14,1,22}); if(boost) g.setBoostForDebug(2.4F);
        Input dash; dash.movement={1,0}; dash.sprintHeld=true;
        for(int i=0;i<240&&!g.wallTouching();++i) g.update(1.F/120,dash);
        CHECK(g.wallTouching()); Input hop=dash; hop.jumpPressed=hop.jumpHeld=true; g.update(1.F/120,hop);
        return g;
      };
      Game kicked=run(true);
      CHECK(kicked.player().motion.trickKind==8); CHECK(kicked.player().locomotion.x<-Game::wallKickSpeed+.5F); CHECK(kicked.player().velocity.y>5.F);
      Game plain=run(false);
      CHECK(plain.player().motion.trickKind!=8); CHECK(plain.player().locomotion.x>-Game::wallKickSpeed+2.F);
      // It flies back out and lands without clipping, even with the stick still into the wall.
      for(int i=0;i<240;++i) { kicked.update(1.F/120,{{1,0},false,false,true}); CHECK(!kicked.playerIntersectsScene()); }
      CHECK(kicked.player().grounded); CHECK(kicked.player().position.y<1.1F);
    }},
    {"slide_tricks",[]{
      // Out of a slide: a sprint cancel plays the kip-up and a jump the scissor twist; neither
      // changes the move itself.
      Input dash; dash.movement={1,0}; dash.sprintHeld=true; Input dip=dash; dip.crouchPressed=true;
      Game g; settle(g,{5.5F,1,7}); tick(g,.5F,dash); tick(g,1.F/120,dip); CHECK(g.player().sliding);
      tick(g,.15F,dash); Input up=dash; up.sprintPressed=true; tick(g,1.F/120,up);
      CHECK(!g.player().sliding); CHECK(g.player().grounded); CHECK(g.player().motion.trickKind==4);
      Game j; settle(j,{5.5F,1,7}); tick(j,.5F,dash); tick(j,1.F/120,dip); tick(j,.15F,dash);
      Input hop=dash; hop.jumpPressed=hop.jumpHeld=true; tick(j,1.F/120,hop);
      CHECK(!j.player().grounded); CHECK(j.player().motion.trickKind==6);
    }},
    {"training",[]{
      Game g; g.reset(Game::tutorialLevel); CHECK(g.tutorial()&&!g.timed()); CHECK(!g.playerIntersectsScene());
      CHECK(g.tutorialStep()==0); const float clock=g.timeRemaining(); tick(g,1.F); CHECK(g.timeRemaining()==clock);
      // Hall 1 by hand: sprint, jump the barrier, slide through the crawl hole, slide into the
      // tunnel and crawl out of it.
      Input run; run.movement={1,0}; run.sprintHeld=true;
      const auto until=[&](float x,Input in) { for(int i=0;i<2400&&g.player().position.x<x;++i) { g.update(1.F/120,in); CHECK(!g.playerIntersectsScene()); } };
      until(23.6F,run); CHECK(g.tutorialStep()>=2);
      Input hop=run; hop.jumpPressed=true; hop.jumpHeld=true; tick(g,1.F/120,hop); hop.jumpPressed=false; tick(g,.7F,hop);
      CHECK(g.player().position.x>27.7F); CHECK(g.tutorialStep()>=3);
      until(31.F,run); Input drop=run; drop.crouchPressed=true; tick(g,1.F/120,drop); CHECK(g.player().sliding);
      until(37.F,run); CHECK(g.tutorialStep()>=4); CHECK(g.player().sliding);
      tick(g,1.F/120,drop); CHECK(!g.player().crouching);  // crouch again stands out of the slide
      until(41.F,run); tick(g,1.F/120,drop); CHECK(g.player().crouching);
      until(54.F,run); CHECK(g.tutorialStep()>=5); tick(g,.3F,run);
      // The rest by route: cores, the checkpoint, the plate and door, the exit.
      routeTo(g,g.cores()[0].position); tick(g,.5F,{{0,0},false,true}); CHECK(g.cores()[0].collected);
      routeTo(g,g.cores()[1].position); tick(g,.5F,{{0,0},false,true}); CHECK(g.cores()[1].collected);
      routeTo(g,g.plates()[0].position); tick(g,.05F); CHECK(g.doors()[0].hold>0); CHECK(g.tutorialStep()>=10);
      // Run straight for the door while it holds (across the speed pad).
      const glm::vec2 door(g.doors()[0].centre.x,g.doors()[0].centre.z);
      for(int i=0;i<1200&&g.player().position.z>13.F;++i) {
        const glm::vec2 p(g.player().position.x,g.player().position.z);
        const glm::vec2 aim=p.y>19.F?glm::vec2(door.x,18.5F):glm::vec2(door.x,12.F);
        g.update(1.F/120,{glm::normalize(aim-p),false,false,false}); CHECK(!g.playerIntersectsScene());
      }
      CHECK(g.player().position.z<13.F); CHECK(g.tutorialStep()>=11);
      routeTo(g,g.cores()[2].position); tick(g,.5F,{{0,0},false,true}); CHECK(g.cores()[2].collected);
      CHECK(g.tutorialStep()==Game::tutorialSteps-1); CHECK(g.gateUnlocked());
      tick(g,.8F); routeTo(g,g.gatePosition()+glm::vec3(0,0,-2)); CHECK(g.state()==RunState::Won);
      std::cout<<"Training completed in "<<g.elapsed()<<"s\n";
    }},
    {"training_reboots",[]{
      // Training never runs out of lives: each shutdown reboots, without a time penalty.
      Game g; g.reset(Game::tutorialLevel);
      for(int life=0;life<4;++life) {
        for(int hit=0;hit<5;++hit) { g.warpPlayer({42,1,24}); tick(g,1.2F); while(g.trapPhase(0)!=TrapPhase::Active) tick(g,.05F); g.warpPlayer({42,1,24}); tick(g,.01F); }
        CHECK(g.state()==RunState::Lost&&g.respawning()&&!g.outOfLives());
        tick(g,Game::respawnDelay+.1F); CHECK(g.state()==RunState::Playing&&g.player().health==5);
      }
      CHECK(g.lives()==Game::livesPerLevel);
    }},
    {"pad_modes",[]{
      const glm::vec3 lane{6,1,26.9F};
      const auto speed=[](const Game& g) { return glm::length(g.player().locomotion); };
      const auto onto=[&](Game& g,Input in) { for(int i=0;i<240&&g.player().motion.boostAge>1.F;++i) g.update(1.F/120,in); CHECK(g.player().motion.boostAge<1.F); };
      Input walk; walk.movement={1,0}; Input dash=walk; dash.sprintHeld=true;
      // Walking onto the pad breaks into a stride; sprint takes off; crouch dives into a slide
      // that keeps the flight's speed.
      Game g; settle(g,lane); onto(g,walk);
      CHECK(g.player().boost==Boost::Stride&&g.player().motion.boostKind==0); CHECK(speed(g)>=Game::strideSpeed-.1F&&speed(g)<Game::padSpeed);
      tick(g,.15F,walk); Input fly=dash; fly.sprintPressed=true; tick(g,1.F/120,fly);
      CHECK(g.player().boost==Boost::Flight); CHECK(speed(g)>=Game::flightTakeoff-.1F); CHECK(g.player().motion.takeoffAge<.05F);
      tick(g,.3F,dash); const float flying=speed(g); Input dive=dash; dive.crouchPressed=true; tick(g,1.F/120,dive);
      CHECK(g.player().sliding&&g.player().boost==Boost::None); CHECK(speed(g)<=flying+.05F&&speed(g)>flying-.3F);
      CHECK(g.player().motion.touchAge<.05F); CHECK(!g.playerIntersectsScene());
      // Sprinting onto it flies at pad speed, over a live trap without a shock, and touches down
      // into a run.
      Game f; settle(f,lane); onto(f,dash);
      CHECK(f.player().boost==Boost::Flight&&f.player().motion.boostKind==1); CHECK(speed(f)>=Game::padSpeed-.1F);
      for(int i=0;i<600&&f.player().boost==Boost::Flight;++i) { f.update(1.F/120,dash); CHECK(!f.playerIntersectsScene()); }
      CHECK(f.player().boost==Boost::None&&f.player().grounded);
      // A flight hovers over live floor plates: training hall 3 has a pad right before its trap.
      int covered=0;
      for(float delay=0;delay<6.F&&covered==0;delay+=.5F) {
        Game h; h.reset(Game::tutorialLevel); settle(h,{16.5F,1,9.75F}); tick(h,delay);
        for(int i=0;i<240;++i) {
          const int before=h.player().health; h.update(1.F/120,dash);
          if(h.player().boost==Boost::Flight&&h.trapPhase(1)==TrapPhase::Active&&h.traps()[1].touchesFeet(h.player().position,Game::footRadius)) { ++covered; CHECK(h.player().health==before); }
        }
      }
      CHECK(covered>0);
      // A flight runs on its clock: turning hard, even reversing, keeps it flying at speed, and it
      // lands when the time is up.
      {
        Game r; r.reset(Game::tutorialLevel); settle(r,{16.5F,1,9.75F}); onto(r,dash); CHECK(r.player().boost==Boost::Flight);
        // Hold the stick 100 degrees off the heading the whole way: a tight, air-braked circle.
        float flown=0; bool reversed=false; Input turn=dash;
        while(r.player().boost==Boost::Flight&&flown<5) {
          const glm::vec2 v=glm::normalize(r.player().locomotion); const float a=1.745F;
          turn.movement={v.x*std::cos(a)-v.y*std::sin(a),v.x*std::sin(a)+v.y*std::cos(a)};
          r.update(1.F/120,turn); flown+=1.F/120; reversed|=r.player().locomotion.x<0;
          CHECK(!r.playerIntersectsScene()); CHECK(r.player().motion.bounceAge>1.F);
        }
        CHECK(reversed);
        CHECK(r.player().boost==Boost::None); CHECK(flown>Game::boostDuration-.1F&&flown<Game::boostDuration+.1F);
      }
      // Jumping keeps a boost: a flight stays a flight through the air and after landing, and so
      // does a stride. The flight clock outlives an interrupted flight: after a dive into a slide,
      // sprint takes off again without restarting the clock, and once the time is up sprint just runs.
      {
        Game j; settle(j,lane); onto(j,dash); tick(j,.3F,dash);
        Input hop=dash; hop.jumpPressed=true; hop.jumpHeld=true; tick(j,1.F/120,hop);
        CHECK(j.player().boost==Boost::Flight&&!j.player().grounded);
        for(int i=0;i<240&&!j.player().grounded;++i) { j.update(1.F/120,dash); CHECK(j.player().boost==Boost::Flight); }
        CHECK(j.player().grounded&&j.player().boost==Boost::Flight); CHECK(speed(j)>=Game::padSpeed-1.F);
        Input dive=dash; dive.crouchPressed=true; tick(j,1.F/120,dive); CHECK(j.player().sliding&&j.player().boost==Boost::None);
        const float left=j.boostTimeLeft(); CHECK(left>0);
        Input again=dash; again.sprintPressed=true; tick(j,1.F/120,again);
        CHECK(j.player().boost==Boost::Flight&&j.player().motion.takeoffAge<.05F); CHECK(j.boostTimeLeft()<left);
        for(int i=0;i<600&&j.boostTimeLeft()>0;++i) j.update(1.F/120,{{0,1},false,false,true});
        CHECK(j.player().boost==Boost::None); tick(j,1.F/120,again); CHECK(j.player().boost==Boost::None);
        Game w2; settle(w2,lane); onto(w2,walk); Input hopWalk=walk; hopWalk.jumpPressed=true; hopWalk.jumpHeld=true; tick(w2,1.F/120,hopWalk);
        CHECK(w2.player().boost==Boost::Stride&&!w2.player().grounded);
        for(int i=0;i<240&&!w2.player().grounded;++i) w2.update(1.F/120,walk);
        CHECK(w2.player().grounded&&w2.player().boost==Boost::Stride);
      }
      // An interaction (here the corridor relay or core) pauses a flight, which lifts off again when
      // it ends if time is left.
      {
        Game k; k.reset(1); settle(k,{5.4F,1,9.8F}); Input north; north.movement={0,1}; north.sprintHeld=true;
        bool flew=false,used=false;
        for(int i=0;i<480&&!used;++i) {
          Input in=north; if(k.player().boost==Boost::Flight) { flew=true; in.movement=glm::normalize(glm::vec2(.35F,1)); }
          if(flew&&k.availableInteraction()!=Interaction::None) { in.interactPressed=true; used=true; }
          k.update(1.F/120,in);
        }
        CHECK(flew&&used); CHECK(k.player().interactionTime>0); CHECK(k.player().boost==Boost::None); CHECK(k.boostTimeLeft()>0);
        for(int i=0;i<120&&k.player().interactionTime>0;++i) k.update(1.F/120,{});
        tick(k,2.F/120);
        CHECK(k.boostTimeLeft()>0&&k.player().boost==Boost::Flight);
      }
      // A slide across the pad gets a burst on top of the pad speed and keeps sliding.
      Game s; settle(s,{4.8F,1,26.9F}); tick(s,.35F,dash); Input drop=dash; drop.crouchPressed=true; tick(s,1.F/120,drop); CHECK(s.player().sliding);
      onto(s,dash); CHECK(s.player().sliding&&s.player().motion.boostKind==2); CHECK(speed(s)>Game::padSpeed);
      // A jump out of the turbo slide keeps its pad speed after landing, and the meter stays up.
      { Input hop=dash; hop.jumpPressed=true; hop.jumpHeld=true; tick(s,1.F/120,hop); CHECK(!s.player().grounded);
        for(int i=0;i<240&&!s.player().grounded;++i) s.update(1.F/120,dash);
        tick(s,2.F/120,dash); CHECK(s.player().grounded&&!s.player().sliding); CHECK(speed(s)>Game::sprintSpeed+1.F); CHECK(s.boostMeter()>0); }
      // A stride with crouch slides as usual.
      Game w; settle(w,lane); onto(w,walk); Input crouch=walk; crouch.crouchPressed=true; tick(w,1.F/120,crouch);
      CHECK(w.player().sliding&&w.player().boost==Boost::None);
    }},
    {"tunnel_camera",[]{
      // Crawling and sliding through a one-voxel hole: the view lowers behind the courier without
      // jumps (sudden changes in its motion) or clipping, and is quickly back to the normal view once out. (The
      // lane runs just south of the courtyard checkpoint past the hole.)
      const auto build=[](Game& g) { for(int x=12;x<14;++x) for(int z=20;z<29;++z) for(int y=1;y<4;++y) if(!(y==1&&z>=23&&z<=25)) g.setVoxelForDebug(x,y,z,2); };
      for(int slide=0;slide<2;++slide) {
        Game g; build(g); settle(g,{6,1,25.1F}); FollowCamera c; c.reset(g); c.orbit(0,-200);
        Input in; in.movement={1,0};
        if(slide) in.sprintHeld=true; else in.crouchHeld=true;
        float worstMove=0,worstStep=0,worstHeight=0,lowest=10,outAt=-1,backAfter=-1,lastStep=0; glm::vec3 last=c.eye();
        for(int f=0;f<480;++f) {
          Input step=in; if(slide&&f==30) step.crouchPressed=true;
          g.update(1.F/60,step); c.update(g,1.F/60);
          const glm::vec3 now=c.eye(),rel=now-c.target(); const float x=g.player().position.x,t=float(f)/60;
          // A jump is a sudden change in how fast the view moves (a steady fast pull-in is fine).
          // (Leaving the hole the view is meant to swing straight back up, so only the way in counts.)
          if(f>0&&x<14.F) {
            const float stepNow=glm::length(rel)-glm::length(last-c.target());
            worstMove=std::max(worstMove,std::abs(stepNow)); worstStep=std::max(worstStep,std::abs(stepNow-lastStep)); lastStep=stepNow;
            worstHeight=std::max(worstHeight,std::abs(now.y-last.y));
          }
          lowest=std::min(lowest,rel.y); last=now;
          if(outAt<0&&x>14+Game::playerRadius+3.2F) outAt=t;
          // Back to normal: the view angle has returned to the player's own pitch.
          if(outAt>=0&&backAfter<0&&std::asin(std::clamp(rel.y/glm::length(rel),-1.F,1.F))>c.pitch()-.03F) backAfter=t-outAt;
          CHECK(!eyeInGeometry(g,c.eye()));
        }
        CHECK(lowest<.8F); CHECK(outAt>=0);
        if(worstMove>.6F||worstStep>.45F||worstHeight>.45F) throw std::runtime_error("camera jump: distance "+std::to_string(worstMove)+" change "+std::to_string(worstStep)+" height "+std::to_string(worstHeight)+" slide="+std::to_string(slide));
        if(backAfter<0||backAfter>.5F) throw std::runtime_error("slow return: "+std::to_string(backAfter)+" slide="+std::to_string(slide));
      }
      // Inside the shot the mouse still turns the view, and it is not dragged straight back.
      Game g; build(g); settle(g,{6,1,25.1F}); FollowCamera c; c.reset(g); c.orbit(0,-200);
      Input creep; creep.movement={1,0}; creep.crouchHeld=true;
      for(int f=0;f<600&&g.player().position.x<12.6F;++f) { g.update(1.F/60,creep); c.update(g,1.F/60); }
      for(int f=0;f<30;++f) { g.update(1.F/60,{{0,0},false,false,false,false,true}); c.update(g,1.F/60); }
      const float before=c.yaw(); c.orbit(250,0);
      for(int f=0;f<30;++f) { g.update(1.F/60,{{0,0},false,false,false,false,true}); c.update(g,1.F/60); }
      CHECK(std::abs(std::remainder(c.yaw()-before,6.2832F))>.3F);
      CHECK(!g.collidesWithScene({c.eye()-glm::vec3(.05F),c.eye()+glm::vec3(.05F)}));
    }},
    {"powerups",[]{
      Game g; CHECK(g.pickups().size()==3);
      const auto shield=g.pickups()[0],spring=g.pickups()[1];
      CHECK(shield.kind==PowerUp::Shield&&spring.kind==PowerUp::Spring);
      const auto apex=[](Game& game) { Input jump; jump.jumpPressed=true; jump.jumpHeld=true; float top=game.player().position.y;
        tick(game,1.F/120,jump); jump.jumpPressed=false; for(int i=0;i<180;++i) { game.update(1.F/120,jump); top=std::max(top,game.player().position.y); } return top-1.F; };
      // Spring boots: a jump goes about 2.5 high instead of 1.7 (clearing a two-block wall); they wear
      // off after springTime, and the item comes back after pickupRespawn.
      Game plain; settle(plain,spring.position+glm::vec3(0,0,2.5F)); tick(plain,.1F); CHECK(plain.player().spring==0);
      const float normal=apex(plain); CHECK(normal<1.9F);
      settle(g,spring.position); tick(g,.05F); CHECK(g.player().spring>Game::springTime-.2F); CHECK(g.pickups()[1].respawn>0);
      const float boosted=apex(g); CHECK(boosted>2.3F&&boosted<2.9F);
      tick(g,Game::springTime); CHECK(g.player().spring==0);
      settle(g,spring.position+glm::vec3(0,0,3)); tick(g,Game::pickupRespawn); CHECK(g.pickups()[1].respawn==0);
      // Shield: absorbs one hit (armour and clock untouched), then the next hit lands as usual.
      Game h; tick(h,3.F); settle(h,shield.position); tick(h,.05F); CHECK(h.player().shielded);
      while(h.trapPhase(0)!=TrapPhase::Active) tick(h,.02F);
      const float clock=h.timeRemaining();
      h.warpPlayer({17,1,13}); tick(h,.02F); CHECK(h.player().health==5&&!h.player().shielded); CHECK(h.timeRemaining()>clock-1.F);
      CHECK(h.player().motion.shieldBreakAge<.1F);
      tick(h,.6F); while(h.trapPhase(0)!=TrapPhase::Active) tick(h,.02F);
      h.warpPlayer({17,1,13}); tick(h,.02F); CHECK(h.player().health==4);
    }},
    {"time_pickup",[]{
      // The clock adds 30 s to a mission's countdown once: it stays spent (no respawn) until the
      // level restarts. Both missions have one; training (no clock) has none.
      for(int level:{0,1}) {
        Game g; g.reset(level); tick(g,1.F);
        const auto it=std::find_if(g.pickups().begin(),g.pickups().end(),[](const Pickup& p) { return p.kind==PowerUp::Time; });
        CHECK(it!=g.pickups().end()); const std::size_t index=std::size_t(it-g.pickups().begin());
        settle(g,it->position+glm::vec3(0,0,-2)); CHECK(!g.playerIntersectsScene());
        const float before=g.timeRemaining();
        tick(g,.05F); settle(g,g.pickups()[index].position); tick(g,.05F);
        CHECK(g.pickups()[index].taken); CHECK(g.timeRemaining()>before+Game::timeBonus-.5F); CHECK(g.timeBonusAge()<.2F);
        // Spent: standing on it again past the respawn time adds nothing.
        settle(g,g.pickups()[index].position+glm::vec3(0,0,-2)); tick(g,Game::pickupRespawn+1);
        const float later=g.timeRemaining(); settle(g,g.pickups()[index].position); tick(g,.05F);
        CHECK(g.pickups()[index].taken); CHECK(g.timeRemaining()<later);
        g.reset(level); CHECK(!g.pickups()[index].taken);
      }
      Game t; t.reset(Game::tutorialLevel);
      CHECK(std::none_of(t.pickups().begin(),t.pickups().end(),[](const Pickup& p) { return p.kind==PowerUp::Time; }));
    }},
    {"camera_edge",[]{
      // Standing on the map edge with the view looking in from outside: the view keeps its full
      // distance (the boundary is ignored) instead of hugging the courier.
      for(const glm::vec3 spot:{glm::vec3(4.4F,1,16),glm::vec3(27.6F,1,16),glm::vec3(16,1,4.4F),glm::vec3(16,1,27.6F)}) {
        Game g; settle(g,spot); FollowCamera c; c.reset(g);
        const glm::vec2 out=glm::normalize(glm::vec2(spot.x-16,spot.z-16));
        const float yaw=std::atan2(out.x,out.y);
        c.orbit(std::remainder(c.yaw()-yaw,6.2831853F)/.0016F,(.35F-c.pitch())/.0016F);
        for(int i=0;i<120;++i) { g.update(1.F/60,{}); c.update(g,1.F/60); }
        CHECK(glm::distance(c.eye(),c.target())>c.distance()-.3F);
        const glm::vec3 e=c.eye(); CHECK(e.x<g.room().min.x||e.x>g.room().max.x||e.z<g.room().min.z||e.z>g.room().max.z);
      }
    }},
    {"level_files",[]{
      // Every built-in level survives a save and load through the level file format unchanged:
      // the same voxels, items, spawn, gate and clock.
      for(int level:{0,1,Game::tutorialLevel}) {
        Game g; g.reset(level); const LevelData before=g.captureLevel();
        const auto path=std::filesystem::temp_directory_path()/("relay_level_test_"+std::to_string(level)+".txt");
        std::string error; CHECK(before.save(path,&error));
        LevelData after; CHECK(LevelData::load(path,after,&error)); std::filesystem::remove(path);
        Game h; h.reset(level); h.applyLevel(after);
        for(int y=0;y<6;++y) for(int z=0;z<VoxelWorld::size;++z) for(int x=0;x<VoxelWorld::size;++x) CHECK(g.world().cell(x,y,z)==h.world().cell(x,y,z));
        CHECK(h.cores().size()==g.cores().size()&&h.traps().size()==g.traps().size()&&h.switches().size()==g.switches().size());
        CHECK(h.checkpoints().size()==g.checkpoints().size()&&h.movers().size()==g.movers().size()&&h.pickups().size()==g.pickups().size());
        CHECK(h.doors().size()==g.doors().size()&&h.plates().size()==g.plates().size()&&h.speedPads().size()==g.speedPads().size());
        for(std::size_t i=0;i<g.cores().size();++i) CHECK(glm::distance(g.cores()[i].position,h.cores()[i].position)<1e-4F);
        for(std::size_t i=0;i<g.doors().size();++i) { CHECK(glm::distance(g.doors()[i].centre,h.doors()[i].centre)<1e-4F); CHECK(glm::distance(g.doors()[i].half,h.doors()[i].half)<1e-4F); }
        for(std::size_t i=0;i<g.movers().size();++i) { CHECK(glm::distance(g.movers()[i].start,h.movers()[i].start)<1e-4F); CHECK(glm::distance(g.movers()[i].end,h.movers()[i].end)<1e-4F); CHECK(near(g.movers()[i].period,h.movers()[i].period)); }
        CHECK(glm::distance(g.spawnPoint(),h.spawnPoint())<1e-4F); CHECK(glm::distance(g.gatePosition(),h.gatePosition())<1e-4F);
        CHECK(near(g.timeRemaining(),h.timeRemaining())); CHECK(near(g.player().yaw,h.player().yaw));
        CHECK(after.message==before.message&&!after.message.empty());
      }
      // An edited copy really changes the level: a new wall, a moved core, a missing link dropped.
      Game g; LevelData d=g.captureLevel(); d.setCell(10,1,10,LevelData::Wall); d.cores[0]={20,20}; d.switches.push_back({{8,8},9});
      const auto path=std::filesystem::temp_directory_path()/"relay_level_edit.txt"; CHECK(d.save(path));
      LevelData e; CHECK(LevelData::load(path,e)); std::filesystem::remove(path);
      CHECK(e.switches.size()==d.switches.size()-1);
      g.applyLevel(e); CHECK(g.world().cell(10,1,10)==LevelData::Wall); CHECK(near(g.cores()[0].position.x,20));
      // Taller maps: six layers with a bridge on layer 5 survive the file and reach the game.
      LevelData tall=Game().captureLevel(); CHECK(tall.height==LevelData::minHeight);
      tall.setHeight(6); for(int x=8;x<16;++x) tall.setCell(x,5,12,LevelData::Wall);
      const auto tallPath=std::filesystem::temp_directory_path()/"relay_level_tall.txt"; CHECK(tall.save(tallPath));
      LevelData back; CHECK(LevelData::load(tallPath,back)); std::filesystem::remove(tallPath);
      CHECK(back.height==6&&back.cell(12,5,12)==LevelData::Wall&&back.cell(12,4,12)==LevelData::Empty);
      Game t; t.applyLevel(back); CHECK(t.world().cell(12,5,12)==LevelData::Wall); CHECK(t.captureLevel().height==5);
    }},
    {"camera_steady",[]{
      // A still courier, seen from any angle, must not make the view tremble: frame to frame the
      // eye-to-aim distance may only drift smoothly (no flicker back and forth).
      float worst=0; int worstFlips=0; std::string where;
      for(int level:{0,1,Game::tutorialLevel}) {
        Game g; g.reset(level); const Bounds room=g.room();
        for(float x=room.min.x+1.5F;x<room.max.x-1.F;x+=3.5F) for(float z=room.min.z+1.5F;z<room.max.z-1.F;z+=3.5F) {
          g.warpPlayer({x,1,z}); tick(g,.05F);
          if(g.playerIntersectsScene()||!g.player().grounded||g.player().position.y>1.2F) continue;
          // Hazards shake the view on purpose: only quiet spots count.
          const auto quiet=[&] { return g.player().hurtCooldown<=0&&g.player().motion.shockAge>1.F&&g.player().motion.hitAge>1.F&&g.player().deathTime<=0; };
          if(!quiet()) continue;
          for(float yaw=0;yaw<6.28F;yaw+=.7F) for(float pitch:{.25F,.6F,1.F}) {
            FollowCamera c; c.reset(g); c.orbit((c.yaw()-yaw)/.0016F,(pitch-c.pitch())/.0016F);
            for(int i=0;i<60;++i) { g.update(1.F/60,{}); c.update(g,1.F/60); }
            if(!quiet()) continue;
            float last=glm::distance(c.eye(),c.target()),lastDelta=0,biggest=0; int flips=0; bool calm=true;
            for(int i=0;i<60;++i) {
              g.update(1.F/60,{}); c.update(g,1.F/60);
              const float d=glm::distance(c.eye(),c.target()),delta=d-last; last=d;
              biggest=std::max(biggest,std::abs(delta));
              if(std::abs(delta)>.004F&&std::abs(lastDelta)>.004F&&delta*lastDelta<0) ++flips;
              if(std::abs(delta)>.004F) lastDelta=delta;
              calm=calm&&quiet();
            }
            if(!calm) continue;
            if(flips>worstFlips||(flips==worstFlips&&biggest>worst)) { worst=biggest; worstFlips=flips; where="level "+std::to_string(level)+" at "+std::to_string(x)+","+std::to_string(z)+" yaw "+std::to_string(yaw)+" pitch "+std::to_string(pitch); }
          }
        }
      }
      std::cout<<"camera steadiness: worst step "<<worst<<", reversals "<<worstFlips<<" ("<<where<<")\n";
      CHECK(worstFlips<=1); CHECK(worst<.2F);  // one pull-in when a moving hazard crosses the line is fine
    }},
    {"wall_camera",[]{
      // Backing the view into walls: walking toward the camera until it meets the courtyard wall,
      // then slowly orbiting in the corner. The view eases in (never snaps) and never clips.
      Game g; settle(g,{14,1,14}); FollowCamera c; c.reset(g);
      for(int i=0;i<120;++i) c.update(g,1.F/120);
      float lastLen=glm::length(c.eye()-c.target()),lastStep=0,worstStep=0,worstMove=0;
      const auto watch=[&]{
        const float len=glm::length(c.eye()-c.target()),step=len-lastLen;
        worstMove=std::max(worstMove,std::abs(step)); worstStep=std::max(worstStep,std::abs(step-lastStep));
        lastLen=len; lastStep=step;
        CHECK(!g.collidesWithScene({c.eye()-glm::vec3(.05F),c.eye()+glm::vec3(.05F)}));
      };
      const glm::vec2 toCamera=glm::normalize(glm::vec2(c.eye().x-c.target().x,c.eye().z-c.target().z));
      for(int f=0;f<240;++f) { g.update(1.F/60,{toCamera}); c.update(g,1.F/60); watch(); }
      for(int f=0;f<180;++f) { c.orbit(-1.F/60/.0016F,0); g.update(1.F/60,{}); c.update(g,1.F/60); watch(); }
      if(worstMove>.8F||worstStep>.6F) throw std::runtime_error("wall camera jump: move "+std::to_string(worstMove)+" change "+std::to_string(worstStep));
    }},
    {"key_bindings",[]{
      KeyBindings kb; CHECK(kb[KeyAction::Jump]==32);  // GLFW_KEY_SPACE
      // Rebinding to a key already in use swaps the two actions.
      const int forward=kb[KeyAction::Forward],interact=kb[KeyAction::Interact];
      CHECK(kb.assign(KeyAction::Interact,forward)); CHECK(kb[KeyAction::Interact]==forward&&kb[KeyAction::Forward]==interact);
      CHECK(!kb.assign(KeyAction::Jump,256)); CHECK(!kb.assign(KeyAction::Jump,257)); CHECK(kb[KeyAction::Jump]==32);  // Esc, Enter
      CHECK(kb.assign(KeyAction::Sprint,341));  // GLFW_KEY_LEFT_CONTROL
      const auto path=std::filesystem::temp_directory_path()/"relay_keys_test.ini";
      CHECK(kb.save(path)); KeyBindings loaded; CHECK(loaded.load(path)); CHECK(loaded.keys==kb.keys); std::filesystem::remove(path);
      auto sorted=kb.keys; std::sort(sorted.begin(),sorted.end()); CHECK(std::adjacent_find(sorted.begin(),sorted.end())==sorted.end());
      kb.reset(); CHECK(kb.keys==KeyBindings::defaults()); CHECK(KeyBindings::keyName(kb[KeyAction::Sprint])=="L-SHIFT");
    }},
    {"timeout_no_respawn",[]{ Game g; g.setTimeForDebug(.2F); tick(g,.5F); CHECK(g.state()==RunState::Lost); CHECK(!g.respawning()); tick(g,4.F); CHECK(g.state()==RunState::Lost); }},
    {"crouch_toggle",[]{
      Game g; Input press; press.crouchPressed=true;
      tick(g,.1F,press); tick(g,.3F); CHECK(g.player().crouching);
      tick(g,.1F,press); tick(g,.3F); CHECK(!g.player().crouching);
      tick(g,.1F,press); Input dash; dash.movement={1,0}; dash.sprintHeld=true; tick(g,.2F,dash); CHECK(!g.player().crouching&&g.player().sprinting);
      tick(g,.5F); tick(g,.1F,press); tick(g,.2F); CHECK(g.player().crouching);
      Input jump; jump.jumpPressed=true; tick(g,.1F,jump); CHECK(!g.player().crouching); tick(g,1.F); CHECK(g.player().grounded&&!g.player().crouching);
      // Toggling off under a one-voxel ceiling stays crouched, then stands at the exit.
      for(int x=12;x<14;++x) for(int z=21;z<28;++z) g.setVoxelForDebug(x,2,z,2);
      g.warpPlayer({10,1,24}); tick(g,.1F); tick(g,.1F,press); Input creep; creep.movement={1,0};
      for(int i=0;i<800&&g.player().position.x<13;++i) g.update(1.F/120,creep);
      tick(g,.1F,press); tick(g,.2F); CHECK(g.player().crouching);
      tick(g,1.5F,creep); CHECK(g.player().position.x>14+Game::playerRadius); tick(g,.2F); CHECK(!g.player().crouching);
    }},
    {"crouch_tunnel",[]{
      // One-voxel gap: ceiling cells at y=2 over the floor surface at y=1.
      const auto build=[](Game& g) { for(int x=12;x<14;++x) for(int z=21;z<28;++z) g.setVoxelForDebug(x,2,z,2); };
      Game standing,crouched; build(standing); build(crouched);
      Input walk; walk.movement={1,0}; Input creep=walk; creep.crouchHeld=true; Input hold; hold.crouchHeld=true;
      tick(standing,2.5F,walk); CHECK(standing.player().position.x<12-Game::playerRadius+.02F);
      tick(crouched,.2F,hold);
      for(int i=0;i<800&&crouched.player().position.x<13;++i) crouched.update(1.F/120,creep);
      CHECK(crouched.player().position.x>=13);
      tick(crouched,.3F); CHECK(crouched.player().crouching); CHECK(!crouched.playerIntersectsScene());
      Input jump; jump.jumpPressed=true; tick(crouched,.1F,jump); CHECK(crouched.player().crouching&&crouched.player().grounded);
      tick(crouched,1.5F,creep); CHECK(crouched.player().position.x>14+Game::playerRadius);
      tick(crouched,.3F); CHECK(!crouched.player().crouching); CHECK(!crouched.playerIntersectsScene());
      // The visible crouched model stays under the crouched collider, idle or creeping.
      SceneModels m; float top=0; std::string worst;
      for(int f=0;f<160;++f) for(int idle=0;idle<2;++idle) {
        Player p=SceneModels::previewPlayer(float(f)/60.F,12); if(idle) { p.moveBlend=0; p.motion.idleFor=float(f)/10.F; }
        m.poseRobot(p); const auto tr=m.robot().transforms(glm::mat4(1));
        for(std::size_t n=0;n<tr.size();++n) if(m.robot().nodes[n].visible)
          for(float x:{-.5F,.5F}) for(float y:{-.5F,.5F}) for(float z:{-.5F,.5F}) {
            const float v=glm::vec3(tr[n]*glm::vec4(glm::vec3(x,y,z)*m.robot().nodes[n].dimensions,1)).y;
            if(v>top) { top=v; worst=m.robot().nodes[n].name+(idle?" idle":" creep")+" frame="+std::to_string(f); }
          }
      }
      if(top>Game::crouchHeight+.02F) throw std::runtime_error("crouched model top="+std::to_string(top)+" "+worst);
    }},
    {"level_transition",[]{
      LevelTransition t; CHECK(!t.active()&&t.black()==0);
      t.start(1); t.start(0); CHECK(t.target==1);
      int loads=0;
      for(int i=0;i<400&&t.active();++i) {
        if(t.update(1.F/60)) { ++loads; CHECK(near(t.black(),1,.05F)); CHECK(t.blocksGameplay()); }
        if(t.active()&&t.elapsed<LevelTransition::fadeOut) CHECK(t.blocksGameplay());
      }
      CHECK(loads==1&&!t.active()&&t.black()==0);
      t.start(0); CHECK(t.update(5.F)); CHECK(!t.active());
    }},
    {"sprint",[]{ Game walk,run,diag; tick(walk,.4F,{{1,0}}); tick(run,.4F,{{1,0},false,false,true}); tick(diag,.4F,{{1,1},false,false,true}); CHECK(run.player().position.x-7>2.75F&&run.player().position.x-7<3.F); CHECK(run.player().position.x>walk.player().position.x); CHECK(run.player().sprinting); CHECK(near(glm::distance(diag.player().position,glm::vec3(7,1,24)),run.player().position.x-7)); tick(run,.1F,{{1,0}}); CHECK(!run.player().sprinting); CHECK(near(run.player().velocity.x,Game::speed)); settle(run,{18,1,22}); tick(run,.5F,{{1,0},false,false,true}); CHECK(!run.playerIntersectsScene()); CHECK(run.player().position.x<18.31F); }},
    {"death",[]{ Game g; for(int i=0;i<5;++i) { g.warpPlayer({7,1,24}); tick(g,1.2F); while(g.trapPhase(0)!=TrapPhase::Active) tick(g,.05F); g.warpPlayer({17,1,13}); tick(g,.01F); CHECK(g.player().health==4-i); CHECK(g.state()==(i==4?RunState::Lost:RunState::Playing)); } CHECK(g.liveParticleCount()>0); tick(g,1.3F,{{1,1},true,true,true}); CHECK(g.player().deathTime>1.2F); CHECK(g.player().health==0); CHECK(g.player().interaction==Interaction::None); CHECK(!g.player().sprinting); CHECK(!g.playerIntersectsScene()); g.reset(0); CHECK(g.player().health==5&&g.player().deathTime==0); CHECK(g.state()==RunState::Playing); }},
    {"knockback",[]{ Game g; g.reset(1); tick(g,.5F); const auto hazard=g.movers()[0]; g.warpPlayer({hazard.position.x,1,hazard.position.z}); tick(g,.01F); CHECK(g.player().health==4); CHECK(glm::length(g.player().knockback)>6.F); CHECK(g.player().velocity.y>0); CHECK(!g.movers()[0].intersectsPlayer(g.player().position,Game::playerRadius,Game::playerHeight)); for(int i=0;i<120;++i) { g.update(1.F/120,{}); CHECK(!g.playerIntersectsScene()); } }},
    {"sensitivity",[]{ Game g; FollowCamera c; c.reset(g); const float before=c.yaw(); c.orbit(100,0); CHECK(near(std::abs(c.yaw()-before),.16F,.001F)); }},
    {"model_bounds",[]{ SceneModels models; CHECK(models.robot().nodes.size()>=80); for(int mode=0;mode<8;++mode) for(int step=0;step<30;++step) { Player p; p.velocity={8,0,0}; p.walkPhase=step*.21F; p.sprinting=mode==1; p.moveBlend=1; p.sprintBlend=mode==1?1.F:0.F; p.airBlend=mode==2?1.F:0.F; p.grounded=mode!=2; if(mode==3) { p.interaction=Interaction::Core; p.interactionTime=.46F-float(step)/30*.45F; } if(mode==4) { p.health=0; p.deathTime=step/29.F; } if(mode==5) { p.landingBlend=step/29.F; } if(mode==6) { p.turnLean=.08F; p.sprintBlend=1; } if(mode==7) { p.hitBlend=1; p.airBlend=.7F; } models.poseRobot(p); for(float yaw:{0.F,.7854F,1.5708F}) { const auto transform=models.robot().transforms(glm::rotate(glm::mat4(1),yaw,{0,1,0})); for(std::size_t n=0;n<transform.size();++n) if(models.robot().nodes[n].visible) for(float x:{-.5F,.5F}) for(float y:{-.5F,.5F}) for(float z:{-.5F,.5F}) { auto v=glm::vec3(transform[n]*glm::vec4(glm::vec3(x,y,z)*models.robot().nodes[n].dimensions,1)); if(glm::length(glm::vec2(v.x,v.z))>Game::playerRadius+.01F||v.y<-.015F||v.y>Game::playerHeight+.01F) throw std::runtime_error("model envelope: "+models.robot().nodes[n].name+" mode="+std::to_string(mode)+" xyz="+std::to_string(v.x)+","+std::to_string(v.y)+","+std::to_string(v.z)); } } } }},

    {"camera",[]{ Game g; FollowCamera c; c.reset(g); auto direction=c.movement(0,1); auto eye=c.eye()-c.target(); CHECK(glm::dot(direction,glm::normalize(glm::vec2(eye.x,eye.z)))<-.999F); CHECK(std::abs(glm::dot(c.movement(1,0),direction))<.001F); c.orbit(10000,10000); CHECK(c.pitch()<=1.22F); c.zoom(1000); CHECK(near(c.distance(),3.5F)); c.zoom(-1000); CHECK(near(c.distance(),13)); c.orbit(0,-10000); CHECK(c.pitch()>=.12F); settle(g,{27.3F,1,27.3F}); c.reset(g); c.orbit(0,-1000); c.update(g,1); CHECK(!g.collidesWithScene({c.eye()-glm::vec3(.10F),c.eye()+glm::vec3(.10F)})); CHECK(glm::distance(c.eye(),c.target())<8); settle(g,{14,1,14}); for(int i=0;i<120;++i)c.update(g,1.F/120); CHECK(glm::distance(c.target(),g.player().position+glm::vec3(0,1.1F,0))<.02F); CHECK(glm::distance(c.eye(),c.target())>7); }},
    {"movement",[]{ Game a,b; const auto origin=a.player().position; tick(a,.5F,{{1,0}}); tick(b,.5F,{{1,1}}); CHECK(a.player().position.x-7>2.38F&&a.player().position.x-7<2.45F); CHECK(near(glm::distance(a.player().position,origin),glm::distance(b.player().position,origin))); Game c; c.update(.5F,{{1,0}}); CHECK(glm::distance(a.player().position,c.player().position)<.001F); }},
    {"jump",[]{
      Game held,tap; float high=1,low=1;
      for(int i=0;i<140;++i) {
        held.update(1.F/120,{{0,0},i==0,false,false,true});
        tap.update(1.F/120,{{0,0},i==0,false,false,false});
        high=std::max(high,held.player().position.y); low=std::max(low,tap.player().position.y);
      }
      CHECK(high>2.70F&&high<2.78F); CHECK(low>2.03F&&low<2.20F); CHECK(high-low>.5F);
      CHECK(held.player().grounded&&tap.player().grounded); CHECK(near(held.player().position.y,1));
      // A second press away from the ground must not create a double jump.
      Game a,b; tick(a,.2F,{{0,0},true,false,false,true}); tick(b,.2F,{{0,0},true,false,false,true});
      a.update(.008F,{{0,0},false,false,false,true}); b.update(.008F,{{0,0},true,false,false,true});
      CHECK(near(a.player().velocity.y,b.player().velocity.y,.001F));
    }},
    {"low_barrier",[]{ Game walk,jump; settle(walk,{11.6F,1,21.1F}); settle(jump,{11.6F,1,21.1F}); tick(walk,1,{{0,-1}}); CHECK(walk.player().position.z>20.27F); tick(jump,1.25F,{{0,-1},true,false,false,true}); CHECK(jump.player().position.z<19); CHECK(jump.player().grounded); CHECK(!jump.playerIntersectsScene()); }},
    {"wall",[]{ Game g; settle(g,{18,1,22}); tick(g,1,{{1,0},true}); CHECK(g.player().position.x<18.73F); CHECK(!g.playerIntersectsScene()); }},
    {"trap_air",[]{ Game ground,air; tick(ground,4.2F); tick(air,4.2F); ground.warpPlayer({17,1,13}); air.warpPlayer({17,1.8F,13}); const float before=air.timeRemaining(); tick(ground,.05F); tick(air,.05F); CHECK(ground.player().hurtCooldown>.9F); CHECK(air.player().hurtCooldown==0); CHECK(near(before-air.timeRemaining(),.05F)); }},
    {"damage_cooldown",[]{ Game g; tick(g,4.1F); g.warpPlayer({17,1,13}); tick(g,.02F); CHECK(g.player().health==4); CHECK(g.player().velocity.y>0); g.warpPlayer({17,1,13}); tick(g,.2F); CHECK(g.player().health==4); g.warpPlayer({7,1,24}); tick(g,1.F); g.warpPlayer({17,1,13}); tick(g,.02F); CHECK(g.player().health==3); }},
    {"switch",[]{ Game g; tick(g,.01F,{{0,0},false,true}); CHECK(g.traps()[0].disabledFor==0); settle(g,g.switches()[0].position+glm::vec3(0,0,1.2F)); CHECK(g.nearestSwitch()==0); tick(g,.01F,{{0,0},false,true}); CHECK(g.player().interaction==Interaction::Switch); tick(g,.5F); CHECK(g.trapPhase(0)==TrapPhase::Disabled); tick(g,4.5F); CHECK(g.trapPhase(0)==TrapPhase::Disabled); tick(g,.4F); CHECK(g.trapPhase(0)!=TrapPhase::Disabled); }},
    {"pickup",[]{ Game g; settle(g,{9,1,22}); CHECK(g.collectedCount()==0); tick(g,1); CHECK(g.collectedCount()==0); tick(g,.01F,{{0,0},false,true}); CHECK(g.player().interaction==Interaction::Core); CHECK(g.collectedCount()==0); tick(g,.25F); CHECK(g.collectedCount()==1); CHECK(g.liveParticleCount()>0); tick(g,1); tick(g,.6F,{{0,0},false,true}); CHECK(g.collectedCount()==1); CHECK(!g.gateUnlocked()); }},
    {"gate",[]{ Game g; settle(g,{25,1,8.3F}); tick(g,.5F,{{0,-1}}); CHECK(g.player().position.z>7.4F); CHECK(g.state()==RunState::Playing); collectAll(g); CHECK(g.gateUnlocked()); settle(g,{25,1,9}); tick(g,.8F); CHECK(near(g.gateOpen(),1)); tick(g,1,{{0,-1}}); CHECK(g.state()==RunState::Won); }},
    {"timeout",[]{ Game g; g.setTimeForDebug(.02F); tick(g,.1F); CHECK(g.state()==RunState::Lost); CHECK(g.timeRemaining()==0); auto p=g.player().position; tick(g,1,{{1,0},true,true}); CHECK(glm::distance(g.player().position,p)<.01F); }},
    {"reset",[]{ Game g; collectAll(g); tick(g,1); g.reset(1); CHECK(g.cores().size()==5); CHECK(g.traps().size()==2); CHECK(g.movers().size()==2); CHECK(g.collectedCount()==0); CHECK(g.gateOpen()==0); CHECK(g.liveParticleCount()==0); CHECK(g.elapsed()==0); CHECK(g.timeRemaining()==180); CHECK(g.player().velocity==glm::vec3(0)); CHECK(g.player().hurtCooldown==0); CHECK(g.player().health==5); CHECK(g.player().deathTime==0); CHECK(g.player().knockback==glm::vec2(0)); CHECK(!g.player().sprinting); CHECK(g.player().interaction==Interaction::None); CHECK(g.traps()[0].disabledFor==0); g.reset(0); CHECK(g.cores().size()==3&&g.movers().empty()); CHECK(g.timeRemaining()==150); }},
    {"particles",[]{ Game g; settle(g,g.switches()[0].position+glm::vec3(0,0,1.2F)); for(int i=0;i<1000;++i) g.update(.01F,{{0,0},false,true}); CHECK(g.liveParticleCount()<=int(Game::particleCapacity)); CHECK(g.liveParticleCount()>0); tick(g,1.1F); CHECK(g.liveParticleCount()==0); }},
    {"hierarchy",[]{ SceneModels m; CHECK(m.robot().nodes.size()>=20); CHECK(m.gate().nodes.size()>=8); auto a=m.robot().transforms(glm::mat4(1)); auto b=m.robot().transforms(glm::translate(glm::mat4(1),{2,3,4})); for(std::size_t i=0;i<a.size();++i) CHECK(glm::length(glm::vec3(b[i][3]-a[i][3])-glm::vec3(2,3,4))<.001F); Player p; p.velocity={5,0,0}; p.moveBlend=1; p.walkPhase=1; m.poseRobot(p); auto animated=m.robot().transforms(glm::mat4(1)); CHECK(glm::length(glm::vec3(animated.back()[3]-a.back()[3]))>.03F); bool threw=false; try { ArticulatedModel invalid; invalid.add({"bad",0}); } catch(const std::invalid_argument&) { threw=true; } CHECK(threw); }},
    {"movers",[]{ Game g; g.reset(1); auto old=g.movers()[0].position; tick(g,.5F); CHECK(glm::distance(old,g.movers()[0].position)>.2F); g.warpPlayer({g.movers()[0].position.x,1,g.movers()[0].position.z}); tick(g,.01F); CHECK(g.player().hurtCooldown>0); }},
    {"routes",[]{ for(int level=0;level<2;++level) { Game g; g.reset(level); const auto cores=g.cores(); for(auto c:cores) { routeTo(g,c.position); tick(g,.5F,{{0,0},false,true}); } CHECK(g.collectedCount()==int(cores.size())); tick(g,.8F); routeTo(g,{25,1,5}); CHECK(g.state()==RunState::Won); std::cout<<"Level "<<level+1<<" route completed with "<<g.timeRemaining()<<"s remaining\n"; } }}
  };
  try { if(argc!=2||!tests.count(argv[1])) throw std::runtime_error("Pass a registered test name"); tests.at(argv[1])(); std::cout<<"PASS "<<argv[1]<<'\n'; }
  catch(const std::exception& e) { std::cerr<<"FAIL "<<(argc>1?argv[1]:"")<<": "<<e.what()<<'\n'; return 1; }
}
