#include "core/RelayGame.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void advance(relay::Game& g,float seconds,relay::Input in={}){for(int i=0;i<int(seconds*120);++i){g.update(1.F/120,in);in.jumpPressed=in.crouchPressed=in.interactPressed=false;}}
int main(){try{
 relay::Game g;
 for(int level=0;level<2;++level){
  g.reset(level); check(g.cores().empty()&&g.portals().empty()&&g.checkpoints().empty()&&g.pickups().empty()&&g.speedPads().empty()&&g.doors().empty(),"Removed objects still present");
  check(!g.traps().empty()&&!g.movers().empty(),"Missing retained hazards");check(g.player().motion.spawnAge>=10,"Arrival effect active");
 }
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
 std::cout<<"Lite: movement, jump/land, toggle/headroom/tunnel, interaction, spike timing/height/cooldown, moving collision and reset passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
