#include "core/FollowCamera.h"
#include "core/KeyBindings.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
bool close(glm::vec3 a,glm::vec3 b) { return glm::distance(a,b)<.00001F; }
void checkMapFits(const relay::Game& game,const relay::FollowCamera& camera) {
  const auto r=game.room();int top=1;
  for(int y=1;y<=relay::LevelData::maxHeight;++y)
    for(int z=int(r.min.z)-1;z<=int(r.max.z);++z)
      for(int x=int(r.min.x)-1;x<=int(r.max.x);++x)
        if(game.world().cell(x,y,z))top=std::max(top,y+1);
  for(float aspect:{16.F/9,16.F/10,4.F/3,1.F,9.F/16,21.F/9}) {
    const auto vp=camera.viewProjection(aspect);
    const auto visible=[&](glm::vec3 at) {
      const auto clip=vp*glm::vec4(at,1);const auto p=glm::vec3(clip)/clip.w;
      check(std::abs(p.x)<=.911F&&p.y>=-.841F&&p.y<=.651F&&std::abs(p.z)<1,"Entire map must fit below the HUD and inside the clipping planes");
    };
    for(int i=0;i<8;++i)visible({i&1?r.max.x+1:r.min.x-1,i&2?float(top)+6:0,i&4?r.max.z+1:r.min.z-1});
    visible(game.spawnPoint());visible(game.gatePosition());
    for(const auto& c:game.cores())visible(c.position);
  }
}
void tick(relay::Game& game,relay::FollowCamera& camera,int count,relay::Input input={}) {
  for(int i=0;i<count;++i) {
    game.update(1.F/120,input); camera.update(game,1.F/120);
    input.jumpPressed=false;
  }
}
}
int main() { try {
  relay::Game game; relay::FollowCamera camera;
  for(int level=0;level<relay::Game::levelCount;++level) {
    game.reset(level); camera.reset(game);
    check(!camera.surveillance()&&glm::distance(camera.target(),game.player().position)<3,"Every level starts in third-person follow");
    camera.setSurveillance(true,game);
    const auto eye=camera.eye(),target=camera.target(),up=camera.up();
    const float fov=camera.fov(),yaw=camera.yaw();
    check(eye.y>game.player().position.y+10,"Monitoring mount is above the current area");
    const auto r=game.room();
    check(std::abs(target.x-(r.min.x+r.max.x)*.5F)<.001F&&std::abs(target.z-(r.min.z+r.max.z)*.5F)<.001F,"View is centred on the whole map");
    checkMapFits(game,camera);
    camera.orbit(1000,400); camera.zoom(30); tick(game,camera,30);
    check(close(camera.eye(),eye)&&close(camera.target(),target)&&close(camera.up(),up)&&camera.fov()==fov&&camera.yaw()==yaw,
          "Mouse, wheel and idle updates cannot move a fixed shot");
    const auto forward=camera.movement(0,1),right=camera.movement(1,0);
    const auto viewForward=glm::normalize(glm::vec2(target.x-eye.x,target.z-eye.z));
    check(glm::dot(forward,viewForward)>.999F&&std::abs(glm::dot(forward,right))<.0001F,"WASD uses the displayed camera basis");
    check(game.level()==level,"Camera never changes the selected map");
    game.warpPlayer({r.max.x-3,1,r.min.z+3});camera.update(game,.1F);
    camera.setSurveillance(false,game);camera.orbit(900,0);camera.setSurveillance(true,game);
    check(close(camera.eye(),eye)&&close(camera.target(),target),"Toggling from another location or orbit keeps the same global view");
  }
  game.reset(0); camera.setSurveillance(false,game); camera.reset(game);
  camera.orbit(130,-25); camera.zoom(2);
  const float followYaw=camera.yaw(),followDistance=camera.distance();
  camera.setSurveillance(true,game);
  const auto eye=camera.eye(),target=camera.target();
  relay::Input walk; walk.movement={0,-1};
  const auto start=game.player().position;
  tick(game,camera,60,walk);
  check(glm::distance(start,game.player().position)>1,"Player still moves in surveillance mode");
  check(close(camera.eye(),eye)&&close(camera.target(),target),"Walking does not track the player");
  relay::Input jump; jump.jumpPressed=jump.jumpHeld=true;
  tick(game,camera,100,jump);
  check(close(camera.eye(),eye)&&close(camera.target(),target),"Jump and landing do not shake the fixed shot");
  game.warpPlayer({25,1,24});
  while(game.trapPhase(0)!=relay::TrapPhase::Active)tick(game,camera,1);
  tick(game,camera,2);
  check(game.player().health<5,"Damage scenario actually hit the robot");
  check(close(camera.eye(),eye)&&close(camera.target(),target)&&camera.fov()==52,"Damage cannot shake or zoom the fixed shot");
  camera.setSurveillance(false,game);
  check(!camera.surveillance()&&std::abs(camera.yaw()-followYaw)<.001F&&camera.distance()==followDistance,"Returning to follow retains orbit and zoom");
  check(glm::distance(camera.target(),game.player().position)<3,"Follow resumes beside the current player");
  // Loading/restarting a level returns to third person. V can opt in again.
  camera.setSurveillance(true,game); game.reset(relay::Game::runLevel); camera.reset(game);
  check(!camera.surveillance()&&glm::distance(camera.target(),game.player().position)<3,"Level change restores third person");
  camera.setSurveillance(true,game);game.reset(relay::Game::runLevel);camera.reset(game);
  check(!camera.surveillance(),"Restart restores third person");
  camera.setSurveillance(true,game);checkMapFits(game,camera);
  // Falling into Corona's void increments the revision and returns to a saved area.
  game.warpPlayer({40.5F,1.2F,62.5F});
  camera.setSurveillance(false,game); camera.setSurveillance(true,game);
  const auto voidEye=camera.eye(); tick(game,camera,60);
  check(game.falls()>0&&close(camera.eye(),voidEye),"Checkpoint recovery preserves the whole-map shot");
  checkMapFits(game,camera);
  // Authored maps can be much wider/taller than the shipped levels.
  for(const auto size:{glm::ivec2(88,12),glm::ivec2(12,88),glm::ivec2(88,88)}) {
    auto map=relay::LevelData::blank(size.x,size.y,16);map.wall=16;
    map.setCell(map.maxX-2,16,map.minZ+2,relay::LevelData::Wall);
    game.applyLevel(map);camera.reset(game);camera.setSurveillance(true,game);checkMapFits(game,camera);
  }
  relay::KeyBindings keys;
  check(!keys.assign(relay::KeyAction::Forward,'V'),"V cannot be rebound over camera switching");
  std::cout<<"Global camera: all map bounds and objectives fit six aspect ratios; fixed pose, controls, follow restore and respawn passed\n";
} catch(const std::exception& error) { std::cerr<<"FAIL camera: "<<error.what()<<'\n'; return 1; } }
