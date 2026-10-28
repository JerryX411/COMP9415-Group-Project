#include "core/RelayGame.h"
#include "core/ButtonState.h"
#include "core/Collision.h"
#include "graphics/RelayModels.h"
#include "core/FollowCamera.h"
#include "core/ArrivalSequence.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
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
  for(int i=0;i<n;++i) { g.update(duration/float(n),input); input.jumpPressed=input.interactPressed=false; }
}
void settle(Game& g,glm::vec3 p) { g.warpPlayer(p); tick(g,.025F); }
void collectAll(Game& g) { const auto cores=g.cores(); for(auto c:cores) { settle(g,{c.position.x,1,c.position.z}); tick(g,.5F,{{0,0},false,true}); } }
void routeTo(Game& g,glm::vec3 target) {
  const int size=64;
  auto index=[](glm::vec3 p){ return int(std::floor(p.z*2))*64+int(std::floor(p.x*2)); };
  const int start=index(g.player().position), goal=index(target);
  std::vector<int> parent(size*size,-1); std::queue<int> q; q.push(start); parent[std::size_t(start)]=start;
  while(!q.empty()&&parent[std::size_t(goal)]<0) {
    int current=q.front(); q.pop(); int x=current%size,z=current/size;
    for(auto d:std::vector<glm::ivec2>{{1,0},{-1,0},{0,1},{0,-1}}) {
      int nx=x+d.x,nz=z+d.y;
      if(nx<8||nx>55||nz<8||nz>55) continue;
      int next=nz*size+nx; if(parent[std::size_t(next)]>=0) continue;
      glm::vec3 feet((nx+.5F)*.5F,1,(nz+.5F)*.5F);
      if(g.world().collides({feet+glm::vec3(-Game::playerRadius,0,-Game::playerRadius),feet+glm::vec3(Game::playerRadius,Game::playerHeight,Game::playerRadius)})) continue;
      // Walk around relay pedestals, leaving a conservative clearance margin.
      bool nearRelay=false;
      for(const auto& sw:g.switches()) if(glm::distance(glm::vec2(feet.x,feet.z),glm::vec2(sw.position.x,sw.position.z))<1.3F) nearRelay=true;
      if(nearRelay) continue;
      parent[std::size_t(next)]=current; q.push(next);
    }
  }
  CHECK(parent[std::size_t(goal)]>=0);
  std::vector<int> path; int cell=goal;
  while(cell!=start) { path.push_back(cell); cell=parent[std::size_t(cell)]; }
  path.push_back(start); std::reverse(path.begin(),path.end());
  for(int next:path) {
    const glm::vec2 destination((next%size+.5F)*.5F,(next/size+.5F)*.5F);
    int attempts=0;
    while(g.state()==RunState::Playing) {
      auto p=g.player().position; auto delta=destination-glm::vec2(p.x,p.z);
      if(glm::length(delta)<.055F) break;
      g.update(1.F/120.F,{glm::normalize(delta),false,false});
      CHECK(!g.playerIntersectsScene());
      if(++attempts>=360) throw std::runtime_error("route stuck: target="+std::to_string(destination.x)+","+std::to_string(destination.y)+" player="+std::to_string(p.x)+","+std::to_string(p.z)+" health="+std::to_string(g.player().health));
    }
    CHECK(g.state()!=RunState::Lost);
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
      for(int clip=0;clip<11;++clip) for(int f=0;f<270;++f) {
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
      if(maxDelta>=.22F) throw std::runtime_error("pose jump="+std::to_string(maxDelta)+" "+worst); CHECK(g.player().moveBlend<.01F);
    }},
    {"sprint",[]{ Game walk,run,diag; tick(walk,.4F,{{1,0}}); tick(run,.4F,{{1,0},false,false,true}); tick(diag,.4F,{{1,1},false,false,true}); CHECK(run.player().position.x-7>2.75F&&run.player().position.x-7<3.F); CHECK(run.player().position.x>walk.player().position.x); CHECK(run.player().sprinting); CHECK(near(glm::distance(diag.player().position,glm::vec3(7,1,24)),run.player().position.x-7)); tick(run,.1F,{{1,0}}); CHECK(!run.player().sprinting); CHECK(near(run.player().velocity.x,Game::speed)); settle(run,{18,1,22}); tick(run,.5F,{{1,0},false,false,true}); CHECK(!run.playerIntersectsScene()); CHECK(run.player().position.x<18.31F); }},
    {"death",[]{ Game g; for(int i=0;i<5;++i) { g.warpPlayer({7,1,24}); tick(g,1.2F); while(g.trapPhase(0)!=TrapPhase::Active) tick(g,.05F); g.warpPlayer({17,1,13}); tick(g,.01F); CHECK(g.player().health==4-i); CHECK(g.state()==(i==4?RunState::Lost:RunState::Playing)); } CHECK(g.liveParticleCount()>0); tick(g,1.3F,{{1,1},true,true,true}); CHECK(g.player().deathTime>1.2F); CHECK(g.player().health==0); CHECK(g.player().interaction==Interaction::None); CHECK(!g.player().sprinting); CHECK(!g.playerIntersectsScene()); g.reset(0); CHECK(g.player().health==5&&g.player().deathTime==0); CHECK(g.state()==RunState::Playing); }},
    {"knockback",[]{ Game g; g.reset(1); tick(g,.5F); const auto hazard=g.movers()[0]; g.warpPlayer({hazard.position.x,1,hazard.position.z}); tick(g,.01F); CHECK(g.player().health==4); CHECK(glm::length(g.player().knockback)>6.F); CHECK(g.player().velocity.y>0); CHECK(!g.movers()[0].intersectsPlayer(g.player().position,Game::playerRadius,Game::playerHeight)); for(int i=0;i<120;++i) { g.update(1.F/120,{}); CHECK(!g.playerIntersectsScene()); } }},
    {"sensitivity",[]{ Game g; FollowCamera c; c.reset(g); const float before=c.yaw(); c.orbit(100,0); CHECK(near(std::abs(c.yaw()-before),.16F,.001F)); }},
    {"model_bounds",[]{ SceneModels models; CHECK(models.robot().nodes.size()>=80); for(int mode=0;mode<8;++mode) for(int step=0;step<30;++step) { Player p; p.velocity={8,0,0}; p.walkPhase=step*.21F; p.sprinting=mode==1; p.moveBlend=1; p.sprintBlend=mode==1?1.F:0.F; p.airBlend=mode==2?1.F:0.F; p.grounded=mode!=2; if(mode==3) { p.interaction=Interaction::Core; p.interactionTime=.46F-float(step)/30*.45F; } if(mode==4) { p.health=0; p.deathTime=step/29.F; } if(mode==5) { p.landingBlend=step/29.F; } if(mode==6) { p.turnLean=.08F; p.sprintBlend=1; } if(mode==7) { p.hitBlend=1; p.airBlend=.7F; } models.poseRobot(p); for(float yaw:{0.F,.7854F,1.5708F}) { const auto transform=models.robot().transforms(glm::rotate(glm::mat4(1),yaw,{0,1,0})); for(std::size_t n=0;n<transform.size();++n) if(models.robot().nodes[n].visible) for(float x:{-.5F,.5F}) for(float y:{-.5F,.5F}) for(float z:{-.5F,.5F}) { auto v=glm::vec3(transform[n]*glm::vec4(glm::vec3(x,y,z)*models.robot().nodes[n].dimensions,1)); if(glm::length(glm::vec2(v.x,v.z))>Game::playerRadius+.01F||v.y<-.015F||v.y>Game::playerHeight+.01F) throw std::runtime_error("model envelope: "+models.robot().nodes[n].name+" mode="+std::to_string(mode)+" xyz="+std::to_string(v.x)+","+std::to_string(v.y)+","+std::to_string(v.z)); } } } }},

    {"camera",[]{ Game g; FollowCamera c; c.reset(g); auto direction=c.movement(0,1); auto eye=c.eye()-c.target(); CHECK(glm::dot(direction,glm::normalize(glm::vec2(eye.x,eye.z)))<-.999F); CHECK(std::abs(glm::dot(c.movement(1,0),direction))<.001F); c.orbit(10000,10000); CHECK(c.pitch()<=1.22F); c.zoom(1000); CHECK(near(c.distance(),3.5F)); c.zoom(-1000); CHECK(near(c.distance(),13)); c.orbit(0,-10000); CHECK(c.pitch()>=.12F); settle(g,{27.3F,1,27.3F}); c.reset(g); c.orbit(0,-1000); c.update(g,1); CHECK(!g.collidesWithScene({c.eye()-glm::vec3(.10F),c.eye()+glm::vec3(.10F)})); CHECK(glm::distance(c.eye(),c.target())<8); settle(g,{16,1,16}); for(int i=0;i<120;++i)c.update(g,1.F/120); CHECK(glm::distance(c.target(),g.player().position+glm::vec3(0,1.1F,0))<.02F); CHECK(glm::distance(c.eye(),c.target())>7); }},
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
