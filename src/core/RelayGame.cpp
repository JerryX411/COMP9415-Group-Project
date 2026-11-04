#include "core/RelayGame.h"
#include "core/Collision.h"
#include "core/AssetPath.h"
#include <iostream>

#include <algorithm>
#include <cmath>

namespace relay {
namespace {
constexpr float epsilon = 0.0001F;
constexpr float pi = 3.14159265358979323846F;
}

bool Bounds::intersects(const Bounds& b) const {
  return min.x < b.max.x - epsilon && max.x > b.min.x + epsilon &&
         min.y < b.max.y - epsilon && max.y > b.min.y + epsilon &&
         min.z < b.max.z - epsilon && max.z > b.min.z + epsilon;
}

std::uint8_t VoxelWorld::cell(int x, int y, int z) const {
  if (x < 0 || y < 0 || z < 0 || x >= size || y >= size || z >= size) return 0;
  return cells_[x + size * (z + size * y)];
}
void VoxelWorld::set(int x, int y, int z, std::uint8_t material) {
  if (x >= 0 && y >= 0 && z >= 0 && x < size && y < size && z < size)
    cells_[x + size * (z + size * y)] = material;
}
void VoxelWorld::fill(int x0, int y0, int z0, int x1, int y1, int z1,
                      std::uint8_t material) {
  for (int y = y0; y < y1; ++y)
    for (int z = z0; z < z1; ++z)
      for (int x = x0; x < x1; ++x) set(x, y, z, material);
}
void VoxelWorld::clear() { std::fill(cells_.begin(), cells_.end(), std::uint8_t(0)); }
bool VoxelWorld::collides(const Bounds& box) const {
  for (int y = std::max(0, int(std::floor(box.min.y))); y <= std::min(size - 1, int(std::floor(box.max.y))); ++y)
    for (int z = std::max(0, int(std::floor(box.min.z))); z <= std::min(size - 1, int(std::floor(box.max.z))); ++z)
      for (int x = std::max(0, int(std::floor(box.min.x))); x <= std::min(size - 1, int(std::floor(box.max.x))); ++x)
        if (cell(x, y, z) && box.intersects({glm::vec3(x, y, z), glm::vec3(x + 1, y + 1, z + 1)})) return true;
  return false;
}
bool VoxelWorld::collidesCylinder(glm::vec3 feet,float radius,float height) const {
  for(int y=std::max(0,int(std::floor(feet.y)));y<=std::min(size-1,int(std::floor(feet.y+height)));++y)
    for(int z=std::max(0,int(std::floor(feet.z-radius)));z<=std::min(size-1,int(std::floor(feet.z+radius)));++z)
      for(int x=std::max(0,int(std::floor(feet.x-radius)));x<=std::min(size-1,int(std::floor(feet.x+radius)));++x)
        if(cell(x,y,z)&&cylinderIntersectsBox(feet,radius,height,{glm::vec3(x,y,z),glm::vec3(x+1,y+1,z+1)})) return true;
  return false;
}
Bounds FloorTrap::tileBounds(float x,float z) const {
  const glm::vec3 centre(x,area.min.y+.04F,z),extent(tileWidth*.5F,.04F,tileWidth*.5F);
  return {centre-extent,centre+extent};
}
bool FloorTrap::touchesFeet(glm::vec3 feet,float radius) const {
  // Only the sole contact patch can activate a plate; an arm above it cannot.
  for(float x=area.min.x+.5F;x<area.max.x;x+=1)
    for(float z=area.min.z+.5F;z<area.max.z;z+=1) {
      auto tile=tileBounds(x,z); tile.max.y=area.min.y+surfaceHeight;
      tile.min.y-=.025F;
      if(cylinderIntersectsBox(feet,radius,.08F,tile)) return true;
    }
  return false;
}
std::array<Bounds,3> MovingHazard::collisionParts() const {
  const auto stripe=position+glm::vec3(0,.15F,halfSize.z+.015F);
  const auto lamp=position+glm::vec3(0,halfSize.y+.035F,0);
  return {{{position-halfSize,position+halfSize},
    {stripe-glm::vec3(.45F,.08F,.0175F),stripe+glm::vec3(.45F,.08F,.0175F)},
    {lamp-glm::vec3(.275F,.035F,.275F),lamp+glm::vec3(.275F,.035F,.275F)}}};
}
Bounds MovingHazard::bounds() const {
  const auto parts=collisionParts(); auto result=parts[0];
  for(const auto& part:parts) { result.min=glm::min(result.min,part.min); result.max=glm::max(result.max,part.max); }
  return result;
}
bool MovingHazard::intersectsPlayer(glm::vec3 feet,float radius,float height) const {
  for(const auto& part:collisionParts()) if(cylinderIntersectsBox(feet,radius,height,part)) return true;
  return false;
}
int VoxelWorld::solidCount() const {
  return int(std::count_if(cells_.begin(), cells_.end(), [](auto c) { return c != 0; }));
}

void Game::buildShell() {
  // Floor and outer walls around room().
  const auto r = room();
  const int x0 = int(r.min.x) - 1, x1 = int(r.max.x), z0 = int(r.min.z) - 1, z1 = int(r.max.z);
  world_.fill(x0, 0, z0, x1 + 1, 1, z1 + 1, 1);
  const int top = 1 + wallHeight_;
  world_.fill(x0, 1, z0, x0 + 1, top, z1 + 1, 2);
  world_.fill(x1, 1, z0, x1 + 1, top, z1 + 1, 2);
  world_.fill(x0, 1, z0, x1 + 1, top, z0 + 1, 2);
  world_.fill(x0, 1, z1, x1 + 1, top, z1 + 1, 2);
}
Bounds Game::playerBounds() const {
  return {player_.position + glm::vec3(-playerRadius, 0, -playerRadius),
          player_.position + glm::vec3(playerRadius, bodyHeight(), playerRadius)};
}
bool Game::blocked(const Bounds& box,int ignoreMover) const {
  if(world_.collides(box)) return true;
  for(std::size_t i=0;i<movers_.size();++i) if(int(i)!=ignoreMover)
    for(const auto& part:movers_[i].collisionParts()) if(box.intersects(part)) return true;
  for(const auto& part:fixtures()) if(box.intersects(part)) return true;
  return false;
}
bool Game::blockedPlayer(glm::vec3 feet,int ignoreMover) const {
  const float height=bodyHeight();
  if(world_.collidesCylinder(feet,playerRadius,height)) return true;
  for(std::size_t i=0;i<movers_.size();++i)
    if(int(i)!=ignoreMover&&movers_[i].intersectsPlayer(feet,playerRadius,height)) return true;
  for(const auto& part:fixtures()) if(cylinderIntersectsBox(feet,playerRadius,height,part)) return true;
  return false;
}
TrapPhase Game::trapPhase(std::size_t i) const {
  const auto& t = traps_.at(i);
  if (t.disabledFor > 0) return TrapPhase::Disabled;
  const float phase = std::fmod(elapsed_ + t.cycleOffset, 6.0F);
  return phase < 3 ? TrapPhase::Safe : phase < 4 ? TrapPhase::Warning : TrapPhase::Active;
}
float Game::trapPhaseRemaining(std::size_t i) const {
  const auto& t = traps_.at(i);
  if (t.disabledFor > 0) return t.disabledFor;
  const float phase = std::fmod(elapsed_ + t.cycleOffset, 6.0F);
  return (phase < 3 ? 3.0F : phase < 4 ? 4.0F : 6.0F) - phase;
}
int Game::nearestSwitch() const {
  float closest = 1.8F;
  int result = -1;
  for (std::size_t i = 0; i < switches_.size(); ++i) {
    const auto delta = player_.position - switches_[i].position;
    const float distance = glm::length(glm::vec2(delta.x, delta.z));
    if (distance < closest && std::abs(delta.y) < 1.0F) { closest = distance; result = int(i); }
  }
  return result;
}
float Game::shadowHeight() const {
  const auto feet=player_.position;
  float support=1.F;
  for(int y=std::min(31,int(std::floor(feet.y)));y>=0;--y)
    if(world_.cell(int(std::floor(feet.x)),y,int(std::floor(feet.z)))) {
      if(y+1<=feet.y+.03F) { support=float(y+1); break; }
    }
  for(const auto& h:movers_) for(const auto& box:h.collisionParts())
    if(feet.x>=box.min.x&&feet.x<=box.max.x&&feet.z>=box.min.z&&feet.z<=box.max.z&&box.max.y<=feet.y+.03F) support=std::max(support,box.max.y);
  return support;
}
void Game::updateMovers(float) {
  for(std::size_t i=0;i<movers_.size();++i) {
    auto& h=movers_[i]; const auto previous=h.position;
    const float t=(1-std::cos((elapsed_+h.offset)*2*pi/h.period))*.5F;
    h.position=glm::mix(h.start,h.end,t);
    if(h.intersectsPlayer(player_.position,playerRadius,bodyHeight())) {
      if(!separateFromMover(i)) h.position=previous;
      applyDamage(h.position,100+int(i));
    }
  }
}
bool Game::separateFromMover(std::size_t index) {
  const auto& h=movers_[index]; const auto original=player_.position;
  const auto lo=h.bounds().min,hi=h.bounds().max;
  std::vector<glm::vec3> candidates{
    {lo.x-playerRadius-.003F,original.y,original.z},{hi.x+playerRadius+.003F,original.y,original.z},
    {original.x,original.y,lo.z-playerRadius-.003F},{original.x,original.y,hi.z+playerRadius+.003F}};
  for(const auto& part:h.collisionParts()) {
    const glm::vec2 closest(std::clamp(original.x,part.min.x,part.max.x),std::clamp(original.z,part.min.z,part.max.z));
    const auto radial=glm::vec2(original.x,original.z)-closest;
    if(glm::length(radial)>.001F) {
      const auto p=closest+glm::normalize(radial)*(playerRadius+.003F);
      candidates.push_back({p.x,original.y,p.y});
    }
  }
  std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b){return glm::distance(a,original)<glm::distance(b,original);});
  candidates.push_back({original.x,hi.y+.003F,original.z});
  for(const auto candidate:candidates) {
    bool clear=true;
    const int steps=std::max(1,int(std::ceil(glm::distance(original,candidate)/.05F)));
    for(int j=1;j<=steps;++j) {
      player_.position=glm::mix(original,candidate,float(j)/steps);
      if(blockedPlayer(player_.position,int(index))) { clear=false; break; }
    }
    if(clear&&!blockedPlayer(player_.position)) return true;
  }
  player_.position=original; return false;
}
Game::Game() { reset(0); }
void Game::reset(int level) {
 level_=std::clamp(level,0,1); ++revision_; state_=RunState::Playing;
 elapsed_=0; remaining_=0; gateOpen_=0; crouchToggle_=false; hitStop_=0;
 player_=Player{}; spawn_={8,1,25}; player_.position=spawn_; player_.motion.spawnAge=10;
 room_={{4,1,4},{36,4,36}}; wallHeight_=3;
 cores_.clear(); interactionIndex_=-1; interactionCommitted_=false;
 traps_.clear(); switches_.clear(); movers_.clear(); particles_.fill(Particle{});
 world_.clear(); buildShell();
 // A jumpable block and a full-width one-cell crawl passage.
 world_.fill(12,1,22,15,2,26,2);
 world_.fill(19,2,20,23,4,27,2);
 world_.fill(19,1,19,23,4,20,2); world_.fill(19,1,27,23,4,28,2);
 if(level_==0) {
  world_.fill(5,1,24,6,3,27,0);
  traps_.push_back({{{12,1,12},{17,1.7F,15}},0,0});
  switches_.push_back({{10,1,16},0,0});
  movers_.push_back({{25,1.75F,10},{25,1.75F,24},{25,1.75F,10},{.65F,.75F,.65F},6,0});
 } else {
  traps_.push_back({{{8,1,9},{13,1.7F,12}},1,0});
  traps_.push_back({{{25,1,26},{29,1.7F,29}},3,0});
  switches_.push_back({{7,1,14},0,0}); switches_.push_back({{30,1,30},1,0});
  movers_.push_back({{14,1.75F,16},{29,1.75F,16},{14,1.75F,16},{.65F,.75F,.65F},7,0});
  world_.fill(28,1,8,31,2,11,2);
 }
 cores_={{{9,1.8F,22},false},{{16.5F,1.8F,23},false},{{28,1.8F,13},false}};
 if(level_==1) cores_={{{8,1.8F,22},false},{{24.5F,1.8F,23},false},{{29,2.8F,9},false}};
 message_="Collect 3 cores with E, then walk through the exit."; messageTime_=5;
}
const std::vector<Bounds>& Game::fixtures() const {
 fixtures_.clear();
 for(const auto& sw:switches_) fixtures_.push_back({sw.position+glm::vec3(-.375F,0,-.325F),sw.position+glm::vec3(.375F,1.05F,.325F)});
 const auto g=gatePosition();
 for(float side:{-1.F,1.F}) {
  const auto post=g+glm::vec3(side*1.8F,1.5F,0);
  fixtures_.push_back({post-glm::vec3(.2F,1.5F,.3F),post+glm::vec3(.2F,1.5F,.3F)});
  const auto door=g+glm::vec3(side*(.8F+gateOpen_*1.6F),1.4F,0);
  fixtures_.push_back({door-glm::vec3(.8F,1.4F,.2F),door+glm::vec3(.8F,1.4F,.2F)});
 }
 fixtures_.push_back({g+glm::vec3(-2,2.8F,-.3F),g+glm::vec3(2,3.2F,.3F)});
 return fixtures_;
}
const char* Game::nameOf(int level) { return level==0?"TUTORIAL YARD":"OBSTACLE YARD"; }
bool Game::gateUnlocked() const { return !cores_.empty() && collectedCount()==int(cores_.size()); }
int Game::collectedCount() const { return int(std::count_if(cores_.begin(),cores_.end(),[](const Core& c){return c.collected;})); }
int Game::liveParticleCount() const { return 0; }
float Game::boostMeter() const { return 0; }
int Game::nearestCore() const {
 float closest=1.8F; int result=-1; const auto hand=player_.position+glm::vec3(0,.8F,0);
 for(std::size_t i=0;i<cores_.size();++i) if(!cores_[i].collected) {
  const auto delta=cores_[i].position-hand; const float dist=glm::length(delta);
  if(dist>=closest)continue;
  bool clear=true; for(int j=1;j<20;++j) {auto q=hand+delta*(float(j)/20);if(world_.cell(int(std::floor(q.x)),int(std::floor(q.y)),int(std::floor(q.z)))){clear=false;break;}}
  if(clear){closest=dist;result=int(i);}
 }
 return result;
}
const char* Game::tutorialHint() const {
 if(gateUnlocked())return "Exit ready: walk through the green door at the far right.";
 if(collectedCount()==0)return "1. Move to the blue core near the start. Press your interact key.";
 if(collectedCount()==1)return "2. Jump over the low block; crouch to pass under the tunnel.";
 return "3. Avoid moving blocks and spikes. Collect the last core to open the exit.";
}
Interaction Game::availableInteraction() const { return nearestCore()>=0?Interaction::Core:nearestSwitch()>=0?Interaction::Switch:Interaction::None; }
void Game::warpPlayer(glm::vec3 p) { player_.position=p; player_.velocity={0,0,0}; player_.locomotion={0,0}; player_.grounded=false; }
void Game::moveAxis(int axis,float distance) {
 if(distance==0) return;
 const float before=player_.position[axis]; player_.position[axis]+=distance;
 if(!blockedPlayer(player_.position)) return;
 float lo=0,hi=1;
 for(int i=0;i<15;++i) { float t=(lo+hi)*.5F; player_.position[axis]=before+distance*t; if(blockedPlayer(player_.position)) hi=t; else lo=t; }
 player_.position[axis]=before+distance*lo;
 if(axis==1) {
  if(distance<0) { player_.grounded=true; if(player_.velocity.y<-2.5F) { player_.motion.landAge=0; player_.landingPulse=1; player_.motion.landStrength=std::clamp(-player_.velocity.y/12.F,.2F,1.F); } }
  player_.velocity.y=0; player_.jumpActive=false;
 } else player_.locomotion[axis==0?0:1]=0;
}
void Game::applyDamage(glm::vec3 source,int,bool,DamageKind) {
 if(player_.hurtCooldown>0||state_!=RunState::Playing) return;
 --player_.health; player_.hurtCooldown=1; player_.hitStun=.12F; player_.motion.hitAge=0;
 auto direction=glm::vec2(player_.position.x-source.x,player_.position.z-source.z);
 if(glm::length(direction)<.01F) direction={0,1}; else direction=glm::normalize(direction);
 player_.knockback=direction*3.F;
 player_.motion.hitSide=direction.x*std::cos(player_.yaw)-direction.y*std::sin(player_.yaw);
 player_.motion.hitForward=-direction.x*std::sin(player_.yaw)-direction.y*std::cos(player_.yaw);
 message_="Impact! Avoid the spikes and moving blocks."; messageTime_=2;
 player_.interaction=Interaction::None; player_.interactionTime=0;
 if(player_.health<=0) { state_=RunState::Lost; player_.deathTime=.01F; player_.locomotion={0,0}; message_="Robot disabled. Press R to restart."; }
}
void Game::updateInteraction(float dt,bool pressed) {
 if(player_.interactionTime>0) {
  player_.interactionTime=std::max(0.F,player_.interactionTime-dt);
  if(!interactionCommitted_&&player_.interactionTime<.2F) {
   interactionCommitted_=true;
   if(player_.interaction==Interaction::Core) {
    if(nearestCore()==interactionIndex_) {cores_[interactionIndex_].collected=true;message_=gateUnlocked()?"All cores collected. Exit unlocked.":"Core collected.";messageTime_=2;}
   } else if(player_.interaction==Interaction::Switch&&nearestSwitch()==interactionIndex_) {
    auto& sw=switches_.at(interactionIndex_);traps_.at(sw.trap).disabledFor=5;sw.animation=1;
    message_="Spikes retracted for 5 seconds.";messageTime_=3;
   }
  }
  if(player_.interactionTime==0)player_.interaction=Interaction::None;
 }
 if(pressed&&player_.interactionTime==0&&player_.hitStun==0) {
  const auto kind=availableInteraction(); if(kind==Interaction::None)return;
  interactionIndex_=kind==Interaction::Core?nearestCore():nearestSwitch();interactionCommitted_=false;
  player_.interaction=kind;player_.interactionTime=.4F;
  player_.interactionTarget=kind==Interaction::Core?cores_[interactionIndex_].position:switches_[interactionIndex_].position+glm::vec3(0,1,0);
 }
}
void Game::update(float seconds,const Input& input) {
 if(!std::isfinite(seconds)||seconds<=0) return;
 const int n=std::max(1,int(std::ceil(std::min(seconds,1.F)*120.F)));
 for(int i=0;i<n;++i) { Input sub=input; if(i) sub.jumpPressed=sub.crouchPressed=sub.interactPressed=false; step(std::min(seconds,1.F)/n,sub); }
}
void Game::step(float dt,const Input& input) {
 if(state_!=RunState::Playing) return;
 elapsed_+=dt; messageTime_=std::max(0.F,messageTime_-dt);
 player_.hurtCooldown=std::max(0.F,player_.hurtCooldown-dt); player_.hitStun=std::max(0.F,player_.hitStun-dt);
 player_.landingPulse=std::max(0.F,player_.landingPulse-5*dt);
 for(auto& t:traps_) t.disabledFor=std::max(0.F,t.disabledFor-dt);
 for(auto& sw:switches_) sw.animation=std::max(0.F,sw.animation-3*dt);
 updateMovers(dt); if(state_!=RunState::Playing) return;
 updateInteraction(dt,input.interactPressed);
 if(gateUnlocked())gateOpen_=std::min(1.F,gateOpen_+dt*1.5F);
 const auto previous=player_.position;
 if(input.crouchPressed) crouchToggle_=!crouchToggle_;
 if(crouchToggle_||input.crouchHeld) player_.crouching=true;
 else if(player_.crouching) { player_.crouching=false; if(blockedPlayer(player_.position)) player_.crouching=true; }
 glm::vec2 movement=input.movement;
 if(glm::length(movement)>1) movement=glm::normalize(movement);
 if(player_.hitStun>0||player_.interactionTime>0) movement={0,0};
 const auto target=movement*(player_.crouching?crouchSpeed:speed);
 player_.locomotion=glm::mix(player_.locomotion,target,1-std::exp(-18.F*dt));
 float turn=0;
 if(glm::length(movement)>.01F) { turn=std::remainder(std::atan2(-movement.x,-movement.y)-player_.yaw,2*pi); player_.yaw+=turn*(1-std::exp(-16*dt)); }
 player_.jumpBuffer=input.jumpPressed?jumpBufferDuration:std::max(0.F,player_.jumpBuffer-dt);
 player_.coyoteTime=player_.grounded?coyoteDuration:std::max(0.F,player_.coyoteTime-dt);
 if(player_.jumpBuffer>0&&player_.coyoteTime>0&&player_.interactionTime<=0&&player_.hitStun<=0&&!player_.crouching) {
  player_.velocity.y=jumpSpeed; player_.grounded=false; player_.jumpActive=true;
  player_.jumpBuffer=player_.coyoteTime=0; player_.motion.launchAge=0;
 }
 if(player_.jumpActive&&!input.jumpHeld&&player_.velocity.y>3) { player_.velocity.y=3; player_.jumpActive=false; }
 player_.knockback*=std::exp(-8*dt);
 player_.velocity.x=player_.locomotion.x+player_.knockback.x; player_.velocity.z=player_.locomotion.y+player_.knockback.y;
 moveAxis(0,player_.velocity.x*dt); moveAxis(2,player_.velocity.z*dt);
 player_.velocity.y-=(player_.velocity.y<0?fallGravity:gravity)*dt;
 player_.grounded=false; moveAxis(1,player_.velocity.y*dt);
 for(std::size_t i=0;i<traps_.size();++i) if(trapPhase(i)==TrapPhase::Active&&traps_[i].touchesFeet(player_.position,footRadius)) applyDamage((traps_[i].area.min+traps_[i].area.max)*.5F,int(i));
 if(player_.position.y<-5) { state_=RunState::Lost; player_.health=0; }
 if(state_==RunState::Playing&&gateUnlocked()&&gateOpen_>.98F&&std::abs(player_.position.x-gatePosition().x)<1.1F&&player_.position.z<gatePosition().z-.35F&&player_.position.z>gatePosition().z-1.8F&&player_.position.y<1.5F) {
  state_=RunState::Won;player_.velocity={0,0,0};player_.locomotion={0,0};player_.moveBlend=0;player_.interaction=Interaction::None;
 }
 updateAnimation(dt,previous,turn);
}
void Game::updateAnimation(float dt,glm::vec3 previous,float turn) {
 const auto blend=[dt](float a,float b,float rate){return glm::mix(a,b,1-std::exp(-rate*dt));};
 const float travelled=glm::length(glm::vec2(player_.position.x-previous.x,player_.position.z-previous.z));
 player_.moveBlend=blend(player_.moveBlend,std::min(1.F,travelled/dt/speed),18);
 player_.airBlend=blend(player_.airBlend,player_.grounded?0.F:1.F,20);
 player_.crouchBlend=blend(player_.crouchBlend,player_.crouching?1.F:0.F,14);
 player_.landingBlend=blend(player_.landingBlend,player_.landingPulse,35);
 player_.hitBlend=blend(player_.hitBlend,player_.hitStun>0?1.F:0.F,20);
 player_.idlePhase+=dt;
 auto& a=player_.motion;
 for(float* age:{&a.startAge,&a.stopAge,&a.pivotAge,&a.launchAge,&a.landAge,&a.stepAge,&a.hitAge}) *age=std::min(10.F,*age+dt);
 a.verticalVelocity=blend(a.verticalVelocity,player_.velocity.y,18);
 motionSpring(a.lean,a.leanVelocity,-.13F*player_.moveBlend,20,.65F,dt);
 motionSpring(a.lookYaw,a.lookVelocity,std::clamp(turn*.45F,-.6F,.6F),26,.85F,dt);
 if(player_.grounded) player_.walkPhase+=travelled*3.8F/modelScale;
}
} // namespace relay
