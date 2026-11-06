#include "core/RelayGame.h"
#include <cmath>
#include "graphics/RelayModels.h"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void advance(relay::Game& g,float seconds,relay::Input in={}){for(int i=0;i<int(seconds*120);++i){g.update(1.F/120,in);in.jumpPressed=in.crouchPressed=in.interactPressed=false;}}
int main(){try{
 relay::Game g;
 for(int level=0;level<2;++level){
  g.reset(level); check(g.portals().empty()&&g.checkpoints().empty()&&g.pickups().empty()&&g.speedPads().empty()&&g.doors().empty(),"Removed objects still present");
  check(!g.traps().empty()&&!g.movers().empty(),"Missing retained hazards");check(g.player().motion.spawnAge>=10,"Arrival effect active");
 }
 g.reset(0);advance(g,1.25F);check(std::abs(g.elapsed()-1.25F)<.001F,"Run clock advances");
 const float timeBefore=g.elapsed();g.update(0,{});g.update(-1,{});check(g.elapsed()==timeBefore,"Invalid dt must not move clock");
 g.reset(1);check(g.elapsed()==0,"Level change resets clock");
 g.reset(0);advance(g,4.1F);g.warpPlayer({14,1,13});g.update(.01F,{});
 check(g.liveParticleCount()==18&&g.player().hitBlend>.7F,"Impact produces immediate recoil and a small burst");
 const auto firstParticle=g.particles()[0].position;advance(g,.05F);
 check(glm::distance(firstParticle,g.particles()[0].position)>.01F,"Impact particles move");
 check(g.liveParticleCount()==18,"Damage cooldown prevents repeated bursts");
 relay::SceneModels models;std::vector<relay::Instance> instances;models.append(g,instances);
 bool hasSpark=false;for(const auto& instance:instances)if(instance.blend==relay::Blend::Additive)hasSpark=true;
 check(hasSpark,"Impact sparks reach the renderer");
 g.warpPlayer({8,1,25});advance(g,1);check(g.liveParticleCount()==0&&g.player().hitBlend<.01F,"Impact cleans up and pose recovers");
 // Repeated valid trap hits reach failure; the last burst must still expire afterwards.
 g.reset(0);
 for(int attempt=0;attempt<8&&g.state()==relay::RunState::Playing;++attempt){
  g.warpPlayer({8,1,25});advance(g,1.1F);
  while(g.state()==relay::RunState::Playing&&g.trapPhase(0)!=relay::TrapPhase::Active)advance(g,.05F);
  g.warpPlayer({14,1,13});g.update(.01F,{});
 }
 check(g.state()==relay::RunState::Lost&&g.liveParticleCount()>0,"Fatal damage burst");
 const float lostTime=g.elapsed();advance(g,1);check(g.elapsed()==lostTime&&g.liveParticleCount()==0,"Failure freezes clock but settles particles");
 g.reset(0);check(g.elapsed()==0&&g.liveParticleCount()==0,"Restart resets timer and particles");
 g.reset(0);advance(g,125);check(!g.timedLevel()&&g.state()==relay::RunState::Playing,"Tutorial has unlimited time");
 g.reset(1);check(g.timeRemaining()==relay::Game::challengeTimeLimit,"Challenge countdown initial value");
 advance(g,1.25F);check(std::abs(g.timeRemaining()-118.75F)<.01F,"Challenge counts down");
 const float remaining=g.timeRemaining();g.update(0,{});check(g.timeRemaining()==remaining,"Stopped simulation keeps countdown");
 advance(g,120);check(g.timedOut()&&g.timeRemaining()==0&&g.player().health==5,"Timeout fails without damaging robot");
 const float timeoutElapsed=g.elapsed();advance(g,1);check(g.timeRemaining()==0&&g.elapsed()==timeoutElapsed,"Timeout stays frozen at zero");
 g.reset(1);check(g.timeRemaining()==120&&!g.timedOut(),"Restart restores countdown");
 g.reset(0); auto start=g.player().position;relay::Input move;move.movement={0,1};advance(g,.5F,move);check(glm::distance(start,g.player().position)>1,"WASD movement");
 g.reset(0); relay::Input jump;jump.jumpPressed=jump.jumpHeld=true;advance(g,.25F,jump);check(g.player().position.y>2,"Jump");advance(g,1);check(g.player().grounded,"Landing");
 g.reset(0);relay::Input crouch;crouch.crouchPressed=true;g.update(.01F,crouch);advance(g,.1F);check(g.player().crouching,"Toggle crouch must persist");
 g.warpPlayer({17.5F,1,23});move.movement={1,0};advance(g,2.8F,move);check(g.player().position.x>23.7F,"Crouch tunnel traversal");
 g.warpPlayer({21,1,23});g.update(.01F,crouch);check(g.player().crouching&&!g.playerIntersectsScene(),"Blocked standing under tunnel");
 g.warpPlayer({24,1,23});advance(g,.1F);check(!g.player().crouching,"Stand when headroom returns");
 g.reset(0);g.warpPlayer({10,1,17.5F});relay::Input use;use.interactPressed=true;advance(g,.45F,use);check(g.trapPhase(0)==relay::TrapPhase::Disabled,"E interaction");advance(g,5.2F);check(g.trapPhase(0)!=relay::TrapPhase::Disabled,"Timed retraction expires");
 g.reset(0);advance(g,4.1F);g.warpPlayer({14,1,13});g.update(.01F,{});check(g.player().health==4,"Active spike damage");advance(g,.1F);check(g.player().health==4,"Hit cooldown");
 g.reset(0);advance(g,4.1F);g.warpPlayer({14,2,13});g.update(.01F,{});check(g.player().health==5,"Airborne above spikes safe");
 g.reset(0);auto h=g.movers()[0].position;g.warpPlayer({25,1,17});advance(g,2);check(glm::distance(h,g.movers()[0].position)>1,"Moving block motion");check(g.player().health<5,"Moving block damage");check(!g.playerIntersectsScene(),"Moving block separation");
 g.reset(1);check(g.player().health==5&&g.state()==relay::RunState::Playing,"Restart resets damage");
 for(int level=0;level<2;++level){
  g.reset(level);check(g.cores().size()==3&&!g.gateUnlocked(),"Initial cores/gate");
  auto gate=g.gatePosition();g.warpPlayer(gate);check(g.playerIntersectsScene(),"Closed exit collider");
  g.warpPlayer(gate+glm::vec3(0,0,-1));advance(g,.1F);check(g.state()==relay::RunState::Playing,"Locked exit cannot win");
  const auto cores=g.cores();
  for(int i=0;i<3;++i){
   g.warpPlayer(cores[i].position-glm::vec3(0,.8F,0));advance(g,.05F);
   check(g.availableInteraction()==relay::Interaction::Core,"Core interaction offered");
   advance(g,.45F,use);check(g.collectedCount()==i+1,"Core collected once");
   advance(g,.45F,use);check(g.collectedCount()==i+1,"Duplicate pickup ignored");
  }
  check(g.gateUnlocked(),"All cores unlock gate");advance(g,1);check(g.gateOpen()>.98F,"Door opens fully");
  g.warpPlayer(gate+glm::vec3(0,0,2));advance(g,.05F);relay::Input exit;exit.movement={0,-1};advance(g,1,exit);
  check(g.state()==relay::RunState::Won,"Walk through open door to complete");
  const float finishTime=g.elapsed(),finishRemaining=g.timeRemaining();advance(g,1);check(g.elapsed()==finishTime&&g.timeRemaining()==finishRemaining,"Completion freezes clock");
  g.reset(level);check(g.collectedCount()==0&&!g.gateUnlocked()&&g.gateOpen()==0&&g.state()==relay::RunState::Playing,"Restart clears progress");
 }
 g.reset(0);g.warpPlayer({14.8F,1,23});check(g.nearestCore()<0,"Core cannot be collected through low block");
 std::cout<<"Lite: movement, jump/land, toggle/headroom/tunnel, interaction, spike timing/height/cooldown, moving collision and reset passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
