#include "core/RelayGame.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
constexpr float dt=1.F/120;
void check(bool condition,const char* message) {
 if(!condition)throw std::runtime_error(message);
}
void wait(relay::Game& game,float seconds) {
 for(int i=0;i<int(seconds/dt);++i)game.update(dt,{});
}
// Traverse the actual map with ordinary movement; never warp between demonstration stations.
void walkTo(relay::Game& game,glm::vec2 target,bool jump=false) {
 for(int i=0;i<1200;++i) {
  const auto p=game.player().position;
  const glm::vec2 delta=target-glm::vec2(p.x,p.z);
  if(glm::length(delta)<.08F){wait(game,.2F);return;}
  relay::Input input;input.movement=delta*3.F;
  input.jumpPressed=jump&&i==0;input.jumpHeld=jump;
  if(glm::length(input.movement)>1)input.movement=glm::normalize(input.movement);
  game.update(dt,input);
 }
 const auto p=game.player().position;
 std::cerr<<"Route blocked at "<<p.x<<','<<p.y<<','<<p.z<<" towards "<<target.x<<','<<target.y<<'\n';
 throw std::runtime_error("Showcase station is not walkable");
}
void interact(relay::Game& game,relay::Interaction kind) {
 check(game.availableInteraction()==kind,"Expected demonstration interaction is unavailable");
 relay::Input input;input.interactPressed=true;game.update(dt,input);wait(game,.5F);
}
}

int main(){try {
 relay::Game game;game.reset(0);
 check(!game.timedLevel()&&game.wordless()&&std::string(game.tutorialHint()).empty(),"Showcase must be unlimited and wordless");
 check(!game.traps().empty()&&!game.switches().empty()&&!game.movers().empty()&&!game.jumpPads().empty()&&
       game.doors().size()==2&&game.plates().size()==2&&game.portals().size()==1,"Showcase must include all  mechanism types");
 wait(game,.2F);
 check(game.player().grounded&&!game.playerIntersectsScene(),"Clear showcase spawn");

 walkTo(game,{20,27.2F});interact(game,relay::Interaction::Core);
 check(game.collectedCount()==1,"Collect the central demonstration core");

 walkTo(game,{20,19.5F});walkTo(game,{10,19.5F});walkTo(game,{10,15});
 check(game.doors()[0].unlocked,"Ground plate opens its door");
 walkTo(game,{10,10.2F});interact(game,relay::Interaction::Core);
 check(game.collectedCount()==2,"Walk through the door to collect its core");
 walkTo(game,{10,15});walkTo(game,{10,19.5F});walkTo(game,{7,19.1F});
 check(game.doors()[0].open>.99F,"Door remains open after leaving the plate");

 interact(game,relay::Interaction::Portal);
 walkTo(game,{18,19.5F});walkTo(game,{18,17.1F});interact(game,relay::Interaction::Portal);
 check(game.portals()[0].linked(),"Both nearby portal ends can be activated on foot");
 for(int i=0;i<120&&game.teleports()==0;++i){relay::Input input;input.movement={0,-1};game.update(dt,input);}
 check(game.teleports()==1,"Step into the linked portal to cross");
 wait(game,.6F);check(game.teleports()==1,"Arrival portal does not bounce the courier back");

 walkTo(game,{7,20.5F});walkTo(game,{20,20.5F});walkTo(game,{29.5F,21});
 bool launched=false;
 for(int i=0;i<120&&!launched;++i){relay::Input input;input.movement={0,-1};game.update(dt,input);launched=game.player().velocity.y>14;}
 check(launched&&game.liveParticleCount()>0,"Walk onto the jump pad to launch with particles");
 walkTo(game,{29.5F,14.5F});wait(game,.7F);
 check(game.player().grounded&&std::abs(game.player().position.y-4)<.02F,"Jump pad reaches the upper demonstration platform");
 check(game.doors()[1].unlocked,"Upper plate unlocks its door when landed on");
 walkTo(game,{29.5F,11.1F});interact(game,relay::Interaction::Core);
 check(game.gateUnlocked(),"All showcase cores unlock the exit");

 walkTo(game,{29.5F,14.5F});walkTo(game,{24,14.5F});wait(game,.6F);
 walkTo(game,{20,14.5F});walkTo(game,{20,8.7F});
 for(int i=0;i<180&&game.state()==relay::RunState::Playing;++i){relay::Input input;input.movement={0,-1};game.update(dt,input);}
 check(game.state()==relay::RunState::Won&&game.player().health==5,"Complete the full demonstration circuit without forced damage");
 game.reset(0);
 check(game.collectedCount()==0&&!game.gateUnlocked()&&!game.doors()[0].unlocked&&!game.doors()[1].unlocked&&
       !game.portals()[0].linked()&&game.teleports()==0&&game.elapsed()==0,"Restart restores every demonstration station");
 // The stairs offer a second route for demonstrating ordinary held jumps, without the pad.
 wait(game,.2F);walkTo(game,{22,32.8F});walkTo(game,{34.5F,32.8F});walkTo(game,{34,22});
 walkTo(game,{34,19.6F},true);wait(game,.5F);
 check(game.player().grounded&&std::abs(game.player().position.y-2)<.02F,"First stair is reachable by a normal jump");
 walkTo(game,{34,16.8F},true);wait(game,.5F);
 check(game.player().grounded&&std::abs(game.player().position.y-3)<.02F,"Second stair is reachable by a normal jump");
 walkTo(game,{34,14.8F},true);wait(game,.5F);
 check(game.player().grounded&&std::abs(game.player().position.y-4)<.02F,"Third stair reaches platform height");
 walkTo(game,{29.5F,14.5F});
 check(game.doors()[1].unlocked&&game.player().health==5,"Stairs give a safe alternative to the spring pad");
 std::cout<<"Showcase: walkable core/ground door/portal/jump pad/upper door/exit circuit, reset and normal-jump stairs passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
