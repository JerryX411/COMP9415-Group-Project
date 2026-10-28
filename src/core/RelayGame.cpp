#include "core/RelayGame.h"
#include "core/Collision.h"

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
void VoxelWorld::clear() { cells_.fill(0); }
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

Game::Game() { reset(0); }
void Game::reset(int level) {
  level_ = std::clamp(level, 0, 1);
  ++revision_;
  state_ = RunState::Playing;
  interactionIndex_ = -1; interactionCommitted_ = false;
  elapsed_ = gateOpen_ = sparkClock_ = 0.0F;
  remaining_ = level_ == 0 ? 150.0F : 180.0F;
  random_ = 0x52454c41 + unsigned(level_);
  player_ = Player{};
  particles_.fill(Particle{});
  cores_.clear(); traps_.clear(); switches_.clear(); movers_.clear();
  world_.clear();
  world_.fill(3, 0, 3, 29, 1, 29, 1);
  world_.fill(3, 1, 3, 4, 4, 29, 2);
  world_.fill(28, 1, 3, 29, 4, 29, 2);
  world_.fill(3, 1, 3, 29, 4, 4, 2);
  world_.fill(3, 1, 28, 29, 4, 29, 2);
  if (level_ == 0) {
    world_.fill(10, 1, 19, 14, 2, 20, 3);
    world_.fill(19, 1, 20, 20, 3, 25, 2);
    world_.fill(8, 1, 10, 12, 3, 11, 2);
    cores_ = {{{9, 1.65F, 22}}, {{17, 1.65F, 16}}, {{25, 1.65F, 10}}};
    traps_.push_back({{{15, 1.0F, 12}, {19, 1.1075F, 14}}, 0.0F, 0.0F});
    switches_.push_back({{12.5F, 1.0F, 15.8F}, 0});
  } else {
    world_.fill(11, 1, 5, 12, 3, 17, 2);
    world_.fill(11, 1, 21, 12, 3, 27, 2);
    world_.fill(20, 1, 9, 21, 3, 14, 2);
    world_.fill(20, 1, 18, 21, 3, 27, 2);
    world_.fill(14, 1, 20, 18, 2, 21, 3);
    cores_ = {{{7, 1.65F, 18}}, {{15, 1.65F, 24}}, {{17, 1.65F, 13}},
              {{24, 1.65F, 20}}, {{25, 1.65F, 9}}};
    traps_.push_back({{{10, 1.0F, 17}, {13, 1.1075F, 21}}, 0.0F, 0.0F});
    traps_.push_back({{{19, 1.0F, 14}, {22, 1.1075F, 18}}, 2.3F, 0.0F});
    switches_.push_back({{7.2F, 1.0F, 15.5F}, 0});
    switches_.push_back({{17.8F, 1.0F, 18.5F}, 1});
    movers_.push_back({{14, 1.55F, 10.5F}, {19, 1.55F, 10.5F}, {}, {0.65F, 0.55F, 0.65F}, 4.8F, 0.0F});
    movers_.push_back({{23, 1.55F, 12}, {26, 1.55F, 19}, {}, {0.65F, 0.55F, 0.65F}, 6.0F, 1.2F});
  }
  for (auto& h : movers_) {
    const float t = (1.0F - std::cos(h.offset * 2.0F * pi / h.period)) * 0.5F;
    h.position = glm::mix(h.start, h.end, t);
  }
  notify(level_ == 0 ? "Collect 3 cores with E. Shift sprints. Avoid the hazards."
                     : "Five cores. Two relays. Watch the moving hazards.");
}

Bounds Game::playerBounds() const {
  return {player_.position + glm::vec3(-playerRadius, 0, -playerRadius),
          player_.position + glm::vec3(playerRadius, playerHeight, playerRadius)};
}
std::vector<Bounds> Game::fixtures() const {
  std::vector<Bounds> boxes; boxes.reserve(switches_.size()+5);
  for(const auto& sw:switches_) boxes.push_back({sw.position+glm::vec3(-.375F,0,-.325F),sw.position+glm::vec3(.375F,1.05F,.325F)});
  const auto p=gatePosition();
  for(float side:{-1.F,1.F}) {
    const auto post=p+glm::vec3(side*1.5F,1.5F,0);
    boxes.push_back({post-glm::vec3(.2F,1.5F,.25F),post+glm::vec3(.2F,1.5F,.25F)});
    const auto door=p+glm::vec3(side*(.65F+gateOpen_*1.35F),1.35F,0);
    boxes.push_back({door-glm::vec3(.64F,1.35F,.23F),door+glm::vec3(.64F,1.35F,.23F)});
  }
  boxes.push_back({p+glm::vec3(-1.7F,2.8F,-.25F),p+glm::vec3(1.7F,3.2F,.25F)});
  return boxes;
}
bool Game::blocked(const Bounds& box,int ignoreMover) const {
  if(world_.collides(box)) return true;
  for(std::size_t i=0;i<movers_.size();++i) if(int(i)!=ignoreMover)
    for(const auto& part:movers_[i].collisionParts()) if(box.intersects(part)) return true;
  for(const auto& part:fixtures()) if(box.intersects(part)) return true;
  return false;
}
bool Game::blockedPlayer(glm::vec3 feet,int ignoreMover) const {
  if(world_.collidesCylinder(feet,playerRadius,playerHeight)) return true;
  for(std::size_t i=0;i<movers_.size();++i)
    if(int(i)!=ignoreMover&&movers_[i].intersectsPlayer(feet,playerRadius,playerHeight)) return true;
  for(const auto& part:fixtures()) if(cylinderIntersectsBox(feet,playerRadius,playerHeight,part)) return true;
  return false;
}
void Game::moveAxis(int axis, float distance) {
  if (distance == 0.0F) return;
  const float before = player_.position[axis];
  player_.position[axis] += distance;
  if (!blockedPlayer(player_.position)) return;
  int contact=-1;
  for(std::size_t i=0;i<movers_.size();++i) if(movers_[i].intersectsPlayer(player_.position,playerRadius,playerHeight)) contact=int(i);
  // Binary search the last non-penetrating position; substeps bound the sweep.
  float lo = 0, hi = 1;
  for (int i = 0; i < 14; ++i) {
    const float t = (lo + hi) * .5F;
    player_.position[axis] = before + distance * t;
    if (blockedPlayer(player_.position)) hi = t; else lo = t;
  }
  player_.position[axis] = before + distance * lo;
  if (axis == 1) {
    if (distance < 0) {
      player_.grounded = true;
      if (player_.velocity.y < -2.5F) {
        player_.landingPulse = 1.0F;
        player_.motion.landAge=0; player_.motion.landStrength=std::clamp((-player_.velocity.y-2.F)/10.F,.25F,1.55F);
        burst(player_.position + glm::vec3(0, .05F, 0), {.62F, .72F, .74F}, 10, .65F);
      }
    }
    player_.velocity.y = 0;
    player_.jumpActive=false;
  } else {
    player_.knockback[axis==0?0:1]=0;
    player_.locomotion[axis==0?0:1]=0;
    player_.velocity[axis]=0;
  }
  if(contact>=0 && state_==RunState::Playing) applyDamage(movers_[std::size_t(contact)].position);
}

void Game::update(float seconds, const Input& input) {
  if (!std::isfinite(seconds) || seconds <= 0) return;
  // Physics runs at <= 1/120 s even during a slow render frame, preventing
  // narrow trap/wall tunnelling and keeping jump height independent of FPS.
  const int steps = std::max(1, int(std::ceil(std::min(seconds, 1.0F) * 120.0F)));
  const float dt = std::min(seconds, 1.0F) / float(steps);
  for (int i = 0; i < steps; ++i) {
    Input sub = input;
    if (i != 0) sub.jumpPressed = sub.interactPressed = false;
    step(dt, sub);
  }
}

void Game::step(float dt, const Input& input) {
  if(state_==RunState::Won) {
    player_.motion.victoryAge+=dt; player_.idlePhase+=dt;
    player_.moveBlend*=std::exp(-18.F*dt); player_.sprintBlend*=std::exp(-18.F*dt);
    player_.airBlend*=std::exp(-18.F*dt); updateParticles(dt); return;
  }
  if(state_==RunState::Lost) {
    player_.deathTime+=dt;
    sparkClock_+=dt;
    if(player_.deathTime<.85F && sparkClock_>.08F) {
      sparkClock_=0;
      burst(player_.position+glm::vec3(0,.85F,0),{.8F,.55F,.22F},3,1.2F);
    }
    player_.knockback*=std::exp(-5.F*dt);
    moveAxis(0,player_.knockback.x*dt); moveAxis(2,player_.knockback.y*dt);
    player_.velocity.y-=gravity*dt; player_.grounded=false;
    moveAxis(1,player_.velocity.y*dt);
    if(player_.position.y < -5) { player_.position={7,1,24}; player_.velocity={0,0,0}; }
    updateParticles(dt); return;
  }
  elapsed_ += dt;
  remaining_ = std::max(0.0F, remaining_ - dt);
  messageTime_ = std::max(0.0F, messageTime_ - dt);
  player_.hurtCooldown = std::max(0.0F, player_.hurtCooldown - dt);
  player_.landingPulse = std::max(0.0F, player_.landingPulse - dt * 5);
  player_.hitStun = std::max(0.F,player_.hitStun-dt);
  player_.knockback*=std::exp(-(player_.grounded?7.F:2.5F)*dt);
  for (auto& t : traps_) t.disabledFor = std::max(0.0F, t.disabledFor - dt);
  for (auto& s : switches_) s.animation = std::max(0.0F, s.animation - dt * 3);

  updateMovers(dt);
  if(state_!=RunState::Playing) return;
  updateInteraction(dt,input.interactPressed);

  const auto previousPosition=player_.position;
  glm::vec2 movement = input.movement;
  if (glm::length(movement) > 1) movement = glm::normalize(movement);
  const bool busy=player_.interactionTime>0, stunned=player_.hitStun>0;
  const bool controlled=!busy&&!stunned;
  player_.sprinting=input.sprintHeld&&controlled&&glm::length(movement)>.05F;
  const bool intent=controlled&&glm::length(movement)>.05F;
  auto& animation=player_.motion;
  if(intent&&!animation.movingIntent&&player_.grounded) animation.startAge=0;
  if(!intent&&animation.movingIntent&&player_.grounded) {
    animation.stopAge=0; animation.stopStrength=std::clamp(animation.lastSpeed/sprintSpeed,0.F,1.F);
  }
  animation.movingIntent=intent;
  const float moveSpeed=player_.sprinting?sprintSpeed:speed;
  const auto targetVelocity=controlled?movement*moveSpeed:glm::vec2(0);
  const auto difference=targetVelocity-player_.locomotion;
  const float distance=glm::length(difference);
  const bool reversing=glm::dot(targetVelocity,player_.locomotion)<0;
  const float acceleration=player_.grounded?(busy?110.F:glm::length(movement)<.05F?90.F:reversing?110.F:65.F):24.F;
  if(distance>0) player_.locomotion+=difference*std::min(1.F,acceleration*dt/distance);
  player_.velocity.x=player_.locomotion.x+player_.knockback.x;
  player_.velocity.z=player_.locomotion.y+player_.knockback.y;
  float turn=0;
  if (glm::length(movement) > .05F && controlled) {
    const float target=std::atan2(-movement.x,-movement.y);
    turn=std::remainder(target-player_.yaw,2*pi);
    if(std::abs(turn)>1.6F&&animation.lastSpeed>2&&animation.pivotAge>.4F) animation.pivotAge=0;
    const float wanted=std::clamp(turn*10.F,-8.0F,8.0F);
    animation.yawVelocity+=std::clamp(wanted-animation.yawVelocity,-55.F*dt,55.F*dt);
    float rotation=animation.yawVelocity*dt;
    if(std::abs(rotation)>std::abs(turn)&&rotation*turn>0) { rotation=turn; animation.yawVelocity=0; }
    player_.yaw=std::remainder(player_.yaw+rotation,2*pi);
  } else if(busy) {
    const auto delta=player_.interactionTarget-player_.position;
    const float target=std::atan2(-delta.x,-delta.z);
    turn=std::remainder(target-player_.yaw,2*pi);
    player_.yaw+=std::clamp(turn*(1-std::exp(-12.F*dt)),-6.F*dt,6.F*dt);
    animation.yawVelocity=0;
  } else animation.yawVelocity*=std::exp(-16.F*dt);
  player_.coyoteTime=player_.grounded?coyoteDuration:std::max(0.F,player_.coyoteTime-dt);
  player_.jumpBuffer=input.jumpPressed?jumpBufferDuration:std::max(0.F,player_.jumpBuffer-dt);
  if(!controlled) player_.jumpBuffer=player_.coyoteTime=0;
  if(player_.jumpBuffer>0&&player_.coyoteTime>0&&controlled) {
    player_.velocity.y=jumpSpeed; player_.grounded=false;
    player_.jumpBuffer=player_.coyoteTime=0;
    player_.jumpActive=true; player_.jumpAge=0; player_.jumpCutQueued=false;
    player_.motion.launchAge=0;
    burst(player_.position+glm::vec3(0,.05F,0),{.45F,.68F,.68F},6,.5F);
  }
  if(player_.jumpActive) {
    player_.jumpAge+=dt;
    if(!input.jumpHeld) player_.jumpCutQueued=true;
    if(player_.jumpCutQueued&&player_.jumpAge>=.055F) player_.velocity.y=std::min(player_.velocity.y,5.8F);
  }
  player_.velocity.y-=(player_.velocity.y>0?gravity:fallGravity)*dt;
  moveAxis(0,player_.velocity.x*dt); moveAxis(2,player_.velocity.z*dt);
  player_.grounded=false; moveAxis(1,player_.velocity.y*dt);

  const Bounds body=playerBounds();
  for(std::size_t i=0;i<traps_.size();++i)
    if(trapPhase(i)==TrapPhase::Active&&traps_[i].touchesFeet(player_.position,footRadius))
      applyDamage((traps_[i].area.min+traps_[i].area.max)*.5F);
  updateAnimation(dt,previousPosition,turn);
  if (player_.position.y < -5) {
    player_.position = {7, 1, 24};
    player_.velocity = glm::vec3(0);
    player_.grounded = true;
    applyDamage(player_.position-glm::vec3(0,0,1),false);
  }

  if (gateUnlocked()) gateOpen_ = std::min(1.0F, gateOpen_ + dt * 1.4F);
  if (remaining_ <= 0 || player_.health<=0) {
    beginDeath();
  } else if (gateOpen_ > .95F && body.intersects({{23.9F, 1, 4.5F}, {26.1F, 3.2F, 6.1F}})) {
    state_ = RunState::Won; player_.motion.victorious=true; player_.motion.victoryAge=0;
    notify("Station restored. Relay complete.");
  }

  sparkClock_ += dt;
  if (sparkClock_ >= .12F) {
    sparkClock_ = 0;
    for (std::size_t i = 0; i < traps_.size(); ++i) if (trapPhase(i) == TrapPhase::Active) {
      const auto& b = traps_[i].area;
      burst({glm::mix(b.min.x, b.max.x, random01()), 1.08F, glm::mix(b.min.z, b.max.z, random01())}, {1, .42F, .16F}, 2, 1.0F);
    }
  }
  updateParticles(dt);
}

void Game::updateAnimation(float dt,glm::vec3 previous,float turn) {
  const auto blend=[dt](float current,float target,float rate) { return glm::mix(current,target,1-std::exp(-rate*dt)); };
  const float travelled=glm::length(glm::vec2(player_.position.x-previous.x,player_.position.z-previous.z));
  const float motion=player_.hitStun>0?0.F:std::min(1.F,travelled/dt/speed);
  player_.moveBlend=blend(player_.moveBlend,motion,18.F);
  player_.sprintBlend=blend(player_.sprintBlend,player_.sprinting?1.F:0.F,12.F);
  player_.airBlend=blend(player_.airBlend,player_.grounded?0.F:1.F,20.F);
  player_.landingBlend=blend(player_.landingBlend,player_.landingPulse,35.F);
  player_.hitBlend=blend(player_.hitBlend,player_.hitStun>0?1.F:0.F,player_.hitStun>0?30.F:10.F);
  player_.turnLean=blend(player_.turnLean,std::clamp(turn*.06F,-.08F,.08F)*motion,14.F);
  player_.idlePhase+=dt;
  auto& a=player_.motion;
  for(float* age:{&a.startAge,&a.stopAge,&a.pivotAge,&a.launchAge,&a.landAge,&a.celebrateAge}) *age=std::min(10.F,*age+dt);
  a.idleFor=motion<.04F&&player_.grounded&&player_.interactionTime<=0&&player_.hitStun<=0?a.idleFor+dt:0;
  const float actualSpeed=travelled/dt;
  const float acceleration=std::clamp((actualSpeed-a.lastSpeed)/dt,-90.F,65.F);
  a.lastSpeed=actualSpeed;
  a.verticalVelocity=blend(a.verticalVelocity,player_.velocity.y,18.F);
  const float targetLean=(-.13F*player_.moveBlend-.09F*player_.sprintBlend-acceleration*.002F)*(1-player_.airBlend);
  motionSpring(a.lean,a.leanVelocity,targetLean,20.F,.65F,dt);
  motionSpring(a.bank,a.bankVelocity,std::clamp(turn*.14F,-.13F,.13F)*motion,19.F,.7F,dt);
  motionSpring(a.lookYaw,a.lookVelocity,std::clamp(turn*.45F,-.6F,.6F),26.F,.85F,dt);
  motionSpring(a.torsoYaw,a.torsoVelocity,std::clamp(turn*.18F,-.22F,.22F),16.F,.82F,dt);
  a.turnWeight=blend(a.turnWeight,std::clamp(std::abs(a.yawVelocity)/6.F,0.F,1.F),12.F);
  motionSpring(a.antenna,a.antennaVelocity,-a.lean*.35F+a.bank*.5F,27.F,.45F,dt);
  if(player_.grounded) {
    const int oldStep=int(player_.walkPhase/3.14159265F);
    // A stance foot travels backwards at root speed; swing returns it quickly.
    player_.walkPhase+=travelled*glm::mix(3.8F,2.6F,player_.sprintBlend);
    if(int(player_.walkPhase/3.14159265F)!=oldStep&&motion>.4F&&player_.hitStun<=0)
      burst(player_.position+glm::vec3(0,.03F,0),{.55F,.65F,.66F},player_.sprinting?4:2,.35F);
  }
}

void Game::updateParticles(float dt) {
  for (auto& p : particles_) if (p.remaining > 0) {
    p.remaining = std::max(0.0F, p.remaining - dt);
    p.velocity.y -= 5 * dt;
    p.position += p.velocity * dt;
    if (p.position.y < 1.025F) { p.position.y = 1.025F; p.velocity.y *= -.25F; }
  }
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
int Game::collectedCount() const { return int(std::count_if(cores_.begin(), cores_.end(), [](const Core& c) { return c.collected; })); }
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
int Game::liveParticleCount() const { return int(std::count_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.remaining > 0; })); }
bool Game::gateUnlocked() const { return !cores_.empty() && collectedCount() == int(cores_.size()); }
const char* Game::levelName() const { return level_ == 0 ? "COURTYARD" : "RELAY CORRIDORS"; }
void Game::warpPlayer(glm::vec3 feet) { player_.position = feet; player_.motion=CourierMotion{}; player_.velocity = glm::vec3(0); player_.locomotion={0,0}; player_.coyoteTime=player_.jumpBuffer=0; player_.jumpActive=player_.jumpCutQueued=false; player_.moveBlend=player_.sprintBlend=player_.airBlend=player_.landingBlend=player_.hitBlend=player_.turnLean=0; player_.knockback={0,0}; player_.hitStun=0; player_.interactionTime=0; player_.interaction=Interaction::None; player_.grounded = false; }
void Game::setTimeForDebug(float seconds) { remaining_ = std::max(0.0F, seconds); }
void Game::notify(std::string message) { message_ = std::move(message); messageTime_ = 3.5F; }
void Game::applyDamage(glm::vec3 source,bool launch) {
  if (player_.hurtCooldown > 0 || state_!=RunState::Playing) return;
  remaining_ = std::max(0.0F, remaining_ - hazardPenalty);
  player_.health=std::max(0,player_.health-1);
  player_.hurtCooldown=1.1F;
  player_.hitStun=.32F;
  player_.locomotion={0,0}; player_.jumpActive=false; player_.jumpBuffer=player_.coyoteTime=0;
  player_.interaction=Interaction::None; player_.interactionTime=0; interactionIndex_=-1;
  player_.sprinting=false;
  player_.motion.celebrateAge=10;
  const auto impact=player_.position-source;
  player_.motion.hitSide=std::clamp((std::cos(player_.yaw)*impact.x-std::sin(player_.yaw)*impact.z),-1.F,1.F);
  if(launch) {
    glm::vec2 away(player_.position.x-source.x,player_.position.z-source.z);
    if(glm::length(away)<.01F) away={-std::sin(player_.yaw),-std::cos(player_.yaw)};
    player_.knockback=glm::normalize(away)*8.F;
    player_.velocity.y=5.8F; player_.grounded=false;
  }
  burst(player_.position+glm::vec3(0,.8F,0),{1,.3F,.15F},32,3.F);
  notify("Impact! -1 armour / -5 seconds.");
  if(player_.health==0 || remaining_<=0) beginDeath();
}
void Game::beginDeath() {
  if(state_==RunState::Lost) return;
  state_=RunState::Lost; player_.deathTime=0; player_.sprinting=false;
  player_.interaction=Interaction::None; player_.interactionTime=0; interactionIndex_=-1;
  burst(player_.position+glm::vec3(0,1,0),{1,.55F,.18F},65,3.8F);
  notify(player_.health<=0?"Courier destroyed. Five impacts exhausted the armour.":"Power depleted. Courier shutting down.");
}
int Game::nearestCore() const {
  float closest=1.8F; int result=-1;
  const auto reach=player_.position+glm::vec3(0,.9F,0);
  for(std::size_t i=0;i<cores_.size();++i) {
    const float distance=glm::distance(reach,cores_[i].position);
    if(!cores_[i].collected && distance<closest) { closest=distance; result=int(i); }
  }
  return result;
}
Interaction Game::availableInteraction() const {
  if(state_!=RunState::Playing||player_.interactionTime>0||player_.hitStun>0) return Interaction::None;
  const int core=nearestCore(),sw=nearestSwitch();
  if(core<0) return sw<0?Interaction::None:Interaction::Switch;
  if(sw<0) return Interaction::Core;
  const auto p=player_.position+glm::vec3(0,.9F,0);
  return glm::distance(p,cores_[std::size_t(core)].position)<glm::distance(p,switches_[std::size_t(sw)].position+glm::vec3(0,.9F,0))?Interaction::Core:Interaction::Switch;
}
void Game::updateInteraction(float dt,bool pressed) {
  constexpr float duration=.46F;
  if(player_.interactionTime>0) {
    player_.interactionTime=std::max(0.F,player_.interactionTime-dt);
    if(!interactionCommitted_ && player_.interactionTime<duration*.5F) {
      interactionCommitted_=true;
      if(player_.interaction==Interaction::Core && interactionIndex_>=0) {
        auto& core=cores_[std::size_t(interactionIndex_)];
        if(!core.collected && glm::distance(player_.position+glm::vec3(0,.9F,0),core.position)<2.F) {
          core.collected=true; player_.motion.celebrateAge=0; burst(core.position,{.2F,1,.82F},38,2.8F);
          notify(gateUnlocked()?"All cores recovered. Exit gate opening!":"Energy core recovered.");
        }
      } else if(player_.interaction==Interaction::Switch && interactionIndex_>=0) {
        auto& sw=switches_[std::size_t(interactionIndex_)];
        traps_[std::size_t(sw.trap)].disabledFor=switchDuration; sw.animation=1;
        burst(sw.position+glm::vec3(0,1,0),{.25F,.95F,.75F},14,1.2F);
        notify("Relay active: linked floor trap safe for 5 seconds.");
      }
    }
    if(player_.interactionTime<=0) player_.interaction=Interaction::None;
  }
  if(!pressed || player_.interactionTime>0 || player_.hitStun>0) return;
  const auto kind=availableInteraction(); if(kind==Interaction::None) return;
  interactionIndex_=kind==Interaction::Core?nearestCore():nearestSwitch();
  player_.interactionTarget=kind==Interaction::Core?cores_[std::size_t(interactionIndex_)].position:switches_[std::size_t(interactionIndex_)].position+glm::vec3(0,1,0);
  player_.interaction=kind; player_.interactionTime=duration; interactionCommitted_=false;
}
void Game::updateMovers(float) {
  for(std::size_t i=0;i<movers_.size();++i) {
    auto& h=movers_[i]; const auto previous=h.position;
    const float t=(1-std::cos((elapsed_+h.offset)*2*pi/h.period))*.5F;
    h.position=glm::mix(h.start,h.end,t);
    if(h.intersectsPlayer(player_.position,playerRadius,playerHeight)) {
      if(!separateFromMover(i)) h.position=previous;
      applyDamage(h.position);
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
float Game::random01() {
  random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
  return float(random_ & 0x00ffffff) / float(0x01000000);
}
void Game::burst(glm::vec3 position, glm::vec3 colour, int count, float energy) {
  for (auto& p : particles_) {
    if (count <= 0) break;
    if (p.remaining > 0) continue;
    p.position = position;
    p.velocity = glm::vec3((random01() - .5F) * 2, .4F + random01(), (random01() - .5F) * 2) * energy;
    p.colour = colour;
    p.lifetime = p.remaining = .35F + random01() * .65F;
    p.size = .035F + random01() * .045F;
    --count;
  }
}

}  // namespace relay
