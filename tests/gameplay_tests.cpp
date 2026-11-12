#include "core/RelayGame.h"
#include "core/FollowCamera.h"
#include <cmath>
#include "graphics/RelayModels.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <stdexcept>
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void advance(relay::Game& g,float seconds,relay::Input in={}){for(int i=0;i<int(seconds*120);++i){g.update(1.F/120,in);in.jumpPressed=in.crouchPressed=in.interactPressed=false;}}
int main(){try{
 relay::Game g;
 for(int level=0;level<2;++level){
  g.reset(level); check(g.checkpoints().empty()&&g.pickups().empty()&&g.speedPads().empty(),"Removed objects still present");
  check(!g.traps().empty()&&!g.movers().empty(),"Missing retained hazards");check(g.player().motion.spawnAge>=10,"Arrival effect active");
 }
 g.reset(0);advance(g,1.25F);check(std::abs(g.elapsed()-1.25F)<.001F,"Run clock advances");
 const float timeBefore=g.elapsed();g.update(0,{});g.update(-1,{});check(g.elapsed()==timeBefore,"Invalid dt must not move clock");
 g.reset(1);check(g.elapsed()==0,"Level change resets clock");
 g.reset(0);advance(g,4.1F);g.warpPlayer({25,1,24});g.update(.01F,{});
 check(g.liveParticleCount()==24&&g.player().hitBlend>.7F,"Impact produces immediate recoil and a small burst");
 const auto firstParticle=g.particles()[0].position;advance(g,.05F);
 check(glm::distance(firstParticle,g.particles()[0].position)>.01F,"Impact particles move");
 check(g.liveParticleCount()==24,"Damage cooldown prevents repeated bursts");
 relay::SceneModels models;std::vector<relay::Instance> instances;models.append(g,instances);
 bool hasSpark=false;for(const auto& instance:instances)if(instance.blend==relay::Blend::Additive)hasSpark=true;
 check(hasSpark,"Impact sparks reach the renderer");
 g.warpPlayer({20,1,32});advance(g,1);check(g.liveParticleCount()==0&&g.player().hitBlend<.01F,"Impact cleans up and pose recovers");
 // Repeated valid trap hits reach failure; the last burst must still expire afterwards.
 g.reset(0);
 for(int attempt=0;attempt<8&&g.state()==relay::RunState::Playing;++attempt){
  g.warpPlayer({20,1,32});advance(g,1.1F);
  while(g.state()==relay::RunState::Playing&&g.trapPhase(0)!=relay::TrapPhase::Active)advance(g,.05F);
  g.warpPlayer({25,1,24});g.update(.01F,{});
 }
 check(g.state()==relay::RunState::Lost&&g.liveParticleCount()>0,"Fatal damage burst");
 const float lostTime=g.elapsed();advance(g,1);check(g.elapsed()==lostTime&&g.liveParticleCount()==0,"Failure freezes clock but settles particles");
 g.reset(0);check(g.elapsed()==0&&g.liveParticleCount()==0,"Restart resets timer and particles");
 g.reset(0);advance(g,4.1F);g.warpPlayer({25,1,24});g.update(.01F,{});
 const auto hitPosition=g.player().position;const auto push=glm::normalize(g.player().knockback);
 advance(g,.3F);const auto recoil=glm::vec2(g.player().position.x-hitPosition.x,g.player().position.z-hitPosition.z);
 check(glm::dot(recoil,push)>1.7F&&glm::length(recoil)<2.1F,"Hit recoils approximately two cells away from damage");
 check(!g.playerIntersectsScene(),"Recoil respects scene collision");
 g.reset(0);advance(g,125);check(!g.timedLevel()&&g.state()==relay::RunState::Playing,"Tutorial has unlimited time");
 // Mission countdown (Helios Yard's 10:00): counts down, holds when stopped, times out without damage.
 g.reset(relay::Game::heliosLevel);const float limit=g.timeLimit();check(g.timeRemaining()==limit&&limit>100,"Mission countdown initial value");
 advance(g,1.25F);check(std::abs(g.timeRemaining()-(limit-1.25F))<.01F,"Mission counts down");
 const float remaining=g.timeRemaining();g.update(0,{});check(g.timeRemaining()==remaining,"Stopped simulation keeps countdown");
 g.warpPlayer(g.spawnPoint());advance(g,limit);check(g.timedOut()&&g.timeRemaining()==0,"Timeout fails the attempt");
 const float timeoutElapsed=g.elapsed();advance(g,1);check(g.timeRemaining()==0&&g.elapsed()==timeoutElapsed,"Timeout stays frozen at zero");
 g.reset(relay::Game::heliosLevel);check(g.timeRemaining()==limit&&!g.timedOut(),"Restart restores countdown");
 for(int level=0;level<2;++level){
  g.reset(level);check(g.jumpPads().size()==1,"Each map has a jump pad");
  const auto pad=g.jumpPads()[0].position;g.warpPlayer(pad);g.update(.01F,{});
  check(g.player().velocity.y>14&&!g.player().grounded,"Standing on pad launches automatically");
  check(g.liveParticleCount()==20&&g.particles()[0].kind==relay::ParticleKind::Ray&&g.particles()[0].velocity.y>5,"Pad emits upward launch streaks");
  float peak=0;for(int i=0;i<90;++i){g.update(1.F/120,{});peak=std::max(peak,g.player().position.y-pad.y);}
  check(peak>4&&peak<4.5F,"Pad jumps higher without holding Space");
  check(g.jumpPads()[0].pulse==0,"Pad feedback decays in flight");
  check(g.liveParticleCount()==0,"Launch plume and trail expire without filling the pool");
  bool bounced=false;for(int i=0;i<120;++i){g.update(1.F/120,{});if(g.player().velocity.y>14){bounced=true;break;}}
  check(bounced,"Landing on pad triggers another bounce");
  g.reset(level);g.warpPlayer(g.jumpPads()[0].position+glm::vec3(0,2,0));g.update(.05F,{});
  check(g.player().velocity.y<0&&g.jumpPads()[0].pulse==0,"Passing above pad does not trigger it");
  g.reset(level);check(g.jumpPads()[0].pulse==0&&g.jumpPads()[0].cooldown==0,"Restart resets pad");
 }
 g.reset(0);advance(g,4.1F);
 g.warpPlayer(g.jumpPads()[0].position);g.update(.01F,{});advance(g,.12F);
 g.warpPlayer({25,1,24});g.update(.01F,{});
 check(g.liveParticleCount()>=50&&g.liveParticleCount()<=int(g.particles().size()),"Overlapping pad and hit bursts keep their particles");
 g.warpPlayer({20,1,32});advance(g,1.2F);check(g.liveParticleCount()==0,"Overlapping particles fully recycle");
 g.reset(0); auto start=g.player().position;relay::Input move;move.movement={0,1};advance(g,.5F,move);check(glm::distance(start,g.player().position)>1,"WASD movement");
 {
  g.reset(relay::Game::heliosLevel);
  relay::FollowCamera camera;
  camera.reset(g);
  relay::Input aim;
  auto position=g.player().position;
  const float idleYaw=g.player().yaw;
  const auto initialView=camera.movement(0,1);
  constexpr float quarterOrbit=1.57079633F/.0016F;
  for(int i=1;i<=4;++i) {
   camera.orbit(quarterOrbit,0);
   aim.facingYaw=camera.yaw();
   advance(g,.1F,aim);
   camera.update(g,.1F);
   check(g.player().cameraMode()==relay::CameraMode::Surrounding&&g.player().yaw==idleYaw,
         "Orbiting an idle courier preserves its heading");
   if(i==2)check(glm::dot(camera.movement(0,1),initialView)<-.999F,"Camera can orbit to the front");
  }
  check(glm::dot(camera.movement(0,1),initialView)>.999F&&glm::distance(position,g.player().position)<.001F,
        "Camera completes a full orbit around the idle courier");

  camera.orbit(600.F,0);
  aim.facingYaw=camera.yaw();
  for(const auto keys:{glm::vec2(1,0),glm::vec2(0,-1)}) {
   position=g.player().position;
   aim.movement=camera.movement(keys.x,keys.y);
   advance(g,.3F,aim);
   const auto moved=g.player().position-position;
   check(glm::dot(glm::vec2(moved.x,moved.z),aim.movement)>.5F&&
         g.player().cameraMode()==relay::CameraMode::Follow&&g.player().yaw==camera.yaw(),
         "Moving courier follows camera heading while strafing or backing up");
  }

  aim.movement={0,0};
  advance(g,.4F,aim);
  const float stoppedYaw=g.player().yaw;
  camera.orbit(-440.F,0);
  aim.facingYaw=camera.yaw();
  advance(g,.1F,aim);
  check(g.player().cameraMode()==relay::CameraMode::Surrounding&&g.player().yaw==stoppedYaw,
        "Slowing to a stop returns the camera to surrounding mode");
 }
 g.reset(0); relay::Input jump;jump.jumpPressed=jump.jumpHeld=true;advance(g,.25F,jump);check(g.player().position.y>2,"Jump");advance(g,1);check(g.player().grounded,"Landing");
 g.reset(0);relay::Input crouch;crouch.crouchPressed=true;g.update(.01F,crouch);advance(g,.1F);check(g.player().crouching,"Toggle crouch must persist");
 g.warpPlayer({13.5F,1,26});move.movement={1,0};advance(g,2.8F,move);check(g.player().position.x>18.7F,"Crouch tunnel traversal");
 g.warpPlayer({16.5F,1,26});g.update(.01F,crouch);check(g.player().crouching&&!g.playerIntersectsScene(),"Blocked standing under tunnel");
 g.warpPlayer({19,1,26});advance(g,.1F);check(!g.player().crouching,"Stand when headroom returns");
 g.reset(0);g.warpPlayer({22,1,26.5F});relay::Input use;use.interactPressed=true;advance(g,.45F,use);check(g.trapPhase(0)==relay::TrapPhase::Disabled,"E interaction");advance(g,5.2F);check(g.trapPhase(0)!=relay::TrapPhase::Disabled,"Timed retraction expires");
 g.reset(0);advance(g,4.1F);g.warpPlayer({25,1,24});g.update(.01F,{});check(g.player().health==4,"Active shock plate damage");advance(g,.1F);check(g.player().health==4,"Hit cooldown");
 g.reset(0);advance(g,4.1F);g.warpPlayer({25,2,24});g.update(.01F,{});check(g.player().health==5,"Airborne above shock plates safe");
 g.reset(0);auto h=g.movers()[0].position;g.warpPlayer({28,1,29.5F});advance(g,2);check(glm::distance(h,g.movers()[0].position)>1,"Moving block motion");check(g.player().health<5,"Moving block damage");check(!g.playerIntersectsScene(),"Moving block separation");
 g.reset(1);check(g.player().health==5&&g.state()==relay::RunState::Playing,"Restart resets damage");
 for(int level=0;level<1;++level){
  g.reset(level);check(g.cores().size()==3&&!g.gateUnlocked(),"Initial cores/gate");
  auto gate=g.gatePosition();g.warpPlayer(gate);check(g.playerIntersectsScene(),"Closed exit collider");
  g.warpPlayer(gate+glm::vec3(0,0,-1));advance(g,.1F);check(g.state()==relay::RunState::Playing,"Locked exit cannot win");
  const auto cores=g.cores();
  for(int i=0;i<3;++i){
   g.warpPlayer(cores[i].position-glm::vec3(0,.8F,0));advance(g,.05F);
   check(g.availableInteraction()==relay::Interaction::Core,"Core interaction offered");
   advance(g,.45F,use);check(g.collectedCount()==i+1,"Core collected once");check(g.liveParticleCount()>0,"Core pickup emits sparks");
   advance(g,.45F,use);check(g.collectedCount()==i+1,"Duplicate pickup ignored");advance(g,.4F);check(g.liveParticleCount()==0,"Pickup burst expires without duplicate emission");
  }
  check(g.gateUnlocked(),"All cores unlock gate");advance(g,1);check(g.gateOpen()>.98F,"Door opens fully");
  g.warpPlayer(gate+glm::vec3(0,0,2));advance(g,.05F);relay::Input exit;exit.movement={0,-1};advance(g,1,exit);
  check(g.state()==relay::RunState::Won,"Walk through open door to complete");
  const float finishTime=g.elapsed(),finishRemaining=g.timeRemaining();advance(g,1);check(g.elapsed()==finishTime&&g.timeRemaining()==finishRemaining,"Completion freezes clock");
  g.reset(level);check(g.collectedCount()==0&&!g.gateUnlocked()&&g.gateOpen()==0&&g.state()==relay::RunState::Playing,"Restart clears progress");
 }
 g.reset(0);g.warpPlayer({29.5F,2.5F,10});check(g.nearestCore()<0,"Core cannot be collected through the raised platform");
 // ---- Helios Yard (shortened  layout) ----
 {
  relay::Game y; y.reset(relay::Game::heliosLevel);
  check(y.level()==relay::Game::heliosLevel&&std::string(y.levelName())=="HELIOS YARD","Helios Yard loads as level 2");
  check(y.cores().size()==4&&y.doors().size()==4&&y.plates().size()==4&&y.traps().size()==48&&y.movers().size()==6&&y.portals().size()==1&&y.jumpPads().size()==1,"Helios Yard: four retained districts and their mechanisms");
 {
  // The big shock floor fires at random: each plate on its own schedule, always warning first, with
  // at least a second of safe floor between shocks; the small plates keep the fixed beat.
  relay::Game r; r.reset(relay::Game::heliosLevel);
  std::vector<std::size_t> rnd; for(std::size_t i=0;i<r.traps().size();++i) if(r.traps()[i].random) rnd.push_back(i);
  check(rnd.size()==31&&rnd.size()<r.traps().size(),"Helios big shock floor is random");
  std::vector<relay::TrapPhase> last(r.traps().size()); for(std::size_t i=0;i<last.size();++i) last[i]=r.trapPhase(i); std::vector<float> safeFor(r.traps().size(),100); std::vector<float> starts;
  bool warnFirst=true,restOk=true,outOfStep=false; float shortestRest=99;
  for(int f=0;f<120*90;++f) {
   r.update(1.F/120,{}); r.warpPlayer(r.spawnPoint());
   int active=0;
   for(std::size_t i:rnd) {
    const auto p=r.trapPhase(i);
    if(p==relay::TrapPhase::Active&&last[i]==relay::TrapPhase::Safe) warnFirst=false;
    if(p==relay::TrapPhase::Warning&&last[i]==relay::TrapPhase::Safe&&safeFor[i]<90) { shortestRest=std::min(shortestRest,safeFor[i]); if(i==rnd[0]) starts.push_back(std::fmod(r.elapsed(),6.F)); }
    safeFor[i]=p==relay::TrapPhase::Safe?safeFor[i]+1.F/120:0;
    active+=p==relay::TrapPhase::Active; last[i]=p;
   }
   if(active>0&&active<int(rnd.size())) outOfStep=true;
  }
  restOk=shortestRest>=.95F;
  float spread=0; for(float a:starts) for(float b:starts) spread=std::max(spread,std::abs(a-b));
  check(warnFirst&&restOk&&outOfStep&&starts.size()>10&&spread>1.F,(std::string("Random plates warn first, rest a second, fire out of step and drift (warn ")+std::to_string(warnFirst)+" rest "+std::to_string(shortestRest)+" step "+std::to_string(outOfStep)+" starts "+std::to_string(starts.size())+" spread "+std::to_string(spread)+")").c_str());
 }
  check(y.timedLevel()&&y.timeRemaining()==600&&y.wordless(),"Helios Yard: 10-minute mission without HUD words");
  check(y.gatePosition()==glm::vec3(46,1,6)&&y.room().max.x==88&&y.room().max.z==64,"Helios Yard compact room and retained exit");
  advance(y,.2F);check(y.player().grounded&&!y.playerIntersectsScene()&&y.player().health==5,"Helios spawn is clear");
  // Materials and floor paint arrive in the voxel world.
  int special=0;for(int z=4;z<88;++z)for(int x=4;x<88;++x)for(int h=0;h<12;++h){const auto m=y.world().cell(x,h,z);if(m>=4&&m<=8)++special;}
  check(special>1000,"Guide, beacon, sandstone and slate materials load");
  // Pressure plates unlock their doors for good, including a ground-level plate linked to a raised door.
  const auto plate=y.plates()[0];const auto& door=y.doors()[std::size_t(plate.door)];
  check(!door.unlocked&&door.open==0,"Door starts sealed");
  y.warpPlayer(plate.position);advance(y,.6F);
  check(y.doors()[std::size_t(plate.door)].unlocked&&y.doors()[std::size_t(plate.door)].open>.99F,"Plate unlocks its door");
  y.warpPlayer(y.spawnPoint());advance(y,30);check(y.doors()[std::size_t(plate.door)].open>.99F,"Door stays open for good");
  const auto raised=std::find_if(y.plates().begin(),y.plates().end(),[&](const auto& p){
   return p.position.y==1.F&&y.doors().at(std::size_t(p.door)).centre.y>2.5F;
  });
  check(raised!=y.plates().end(),"A ground plate controls a raised door");
  y.warpPlayer(raised->position);advance(y,.6F);
  check(y.doors().at(std::size_t(raised->door)).unlocked,"Ground plate unlocks the raised door");
  // Portals: wake both ends with E, then step in to cross.
  relay::Input use;use.interactPressed=true;
  const auto link=y.portals()[0];
  y.warpPlayer(link.from+glm::vec3(1.1F,0,0));advance(y,.1F);check(y.availableInteraction()==relay::Interaction::Portal,"Portal end offers activation");
  advance(y,.5F,use);check(y.portals()[0].fromOn&&!y.portals()[0].linked(),"First end wakes");
  y.warpPlayer(link.to+glm::vec3(1.1F,0,0));advance(y,.1F);advance(y,.5F,use);check(y.portals()[0].linked(),"Both ends link");
  advance(y,1.2F);y.warpPlayer(link.from);advance(y,.1F);
  check(y.teleports()==1&&glm::distance(glm::vec2(y.player().position.x,y.player().position.z),glm::vec2(link.to.x,link.to.z))<.1F,"Stepping in crosses to the pair");
  advance(y,.5F);check(y.teleports()==1,"Arrival end waits until stepped off");
  // Restart seals the doors and puts the portals to sleep again.
  y.reset(relay::Game::heliosLevel);check(!y.doors()[0].unlocked&&!y.portals()[0].fromOn&&y.teleports()==0,"Restart resets Helios Yard");
  // Electric floor plates hurt only feet on the plate.
  relay::Game t;t.reset(relay::Game::heliosLevel);
  const auto& trap=t.traps()[0];std::size_t ti=0;
  while(t.trapPhase(ti)!=relay::TrapPhase::Active)advance(t,.05F);
  t.warpPlayer({trap.area.min.x+.5F,1,trap.area.min.z+.5F});t.update(.01F,{});check(t.player().health==4,"Live shock plate hurts");
 }
 // Shift sprint is faster than a run, and crouching cancels it.
 {
  relay::Game a,b;a.reset(0);b.reset(0);relay::Input run;run.movement={0,1};relay::Input dash=run;dash.sprintHeld=true;
  const auto a0=a.player().position,b0=b.player().position;advance(a,.4F,run);advance(b,.4F,dash);
  check(glm::distance(b0,b.player().position)>glm::distance(a0,a.player().position)*1.35F&&b.player().sprinting,"Shift sprint is faster");
  relay::Input crouchDash=dash;crouchDash.crouchHeld=true;advance(b,.1F,crouchDash);check(!b.player().sprinting,"Crouch stops the sprint");
 }
 // Ground dust: footfalls while running, a puff at take-off, a ring on landing; all of it settles.
 {
  const auto dust=[](const relay::Game& g){int n=0;for(const auto& p:g.particles())if(p.remaining>0&&p.kind==relay::ParticleKind::Dust)++n;return n;};
  relay::Game d;d.reset(0);relay::Input run;run.movement={0,1};
  int most=0;for(int i=0;i<60;++i){d.update(1.F/120,run);most=std::max(most,dust(d));}
  check(most>0,"Running kicks up dust at the feet");
  for(const auto& p:d.particles())if(p.remaining>0&&p.kind==relay::ParticleKind::Dust)check(p.position.y>1.02F&&p.position.y<1.6F,"Dust stays low above the floor");
  advance(d,1);check(dust(d)==0,"Step dust settles");
  d.reset(0);advance(d,.2F);relay::Input hop;hop.jumpPressed=hop.jumpHeld=true;d.update(1.F/120,hop);
  check(dust(d)>=5,"Take-off puff");
  advance(d,1);d.warpPlayer({20,4,32});advance(d,.05F);const int before=dust(d);
  bool landed=false;int ring=0;for(int i=0;i<120&&!landed;++i){d.update(1.F/120,{});if(d.player().grounded){landed=true;ring=dust(d);}}
  check(landed&&ring>=before+8,"Landing ring of dust");
  advance(d,1);check(dust(d)==0,"Landing dust settles");
  d.reset(0);advance(d,2);check(dust(d)==0,"Standing still makes no dust");
 }
 // The tutorial yard never runs out of lives: a destroyed robot always reboots.
 {
  relay::Game t;t.reset(0);check(!t.livesLimited(),"Tutorial lives are unlimited");
  relay::Game o;o.reset(1);check(o.livesLimited()&&o.lives()==relay::Game::livesPerLevel,"Missions start with three lives");
 }
 std::cout<<": movement, jump/land, toggle/headroom/tunnel, interaction, shock timing/height/cooldown, moving collision and reset passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
