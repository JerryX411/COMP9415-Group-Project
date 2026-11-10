#include "core/RelayGame.h"
#include "core/Collision.h"
#include "core/AssetPath.h"
#include <iostream>

#include <algorithm>
#include <cmath>
#include <stdexcept>

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
std::array<Bounds,2> BlastDoor::panels() const {
  const int axis=alongZ?2:0;
  const float shift=open*half[axis]*1.02F;
  Bounds a=bounds(),b=bounds();
  a.max[axis]=centre[axis]; b.min[axis]=centre[axis];
  a.min[axis]-=shift; a.max[axis]-=shift; b.min[axis]+=shift; b.max[axis]+=shift;
  return {a,b};
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
// Plates beat every 6 s: 3 s safe, a 1 s warning, 2 s of shock. A random plate (Helios's big shock
// floor) has its own offset, and in every 6 s slot its warning starts at a random moment between
// 0.5 and 2.5 s. The warning always comes first, and at least 1 s of safe floor separates two
// shocks. The schedule is a hash of the slot and the plate, so it is the same on every replay.
namespace {
float randomTrapStart(float slot, std::size_t index) {
  const float h = std::sin(slot * 12.9898F + float(index) * 78.233F + 4.1F) * 43758.5453F;
  return .5F + 2.F * (h - std::floor(h));
}
}
TrapPhase Game::trapPhase(std::size_t i) const {
  const auto& t = traps_.at(i);
  if (t.disabledFor > 0) return TrapPhase::Disabled;
  if (t.random) {
    const float time = elapsed_ + t.cycleOffset, slot = std::floor(time / 6.F), in = time - slot * 6.F;
    const float start = randomTrapStart(slot, i);
    return in < start || in >= start + 3 ? TrapPhase::Safe : in < start + 1 ? TrapPhase::Warning : TrapPhase::Active;
  }
  const float phase = std::fmod(elapsed_ + t.cycleOffset, 6.0F);
  return phase < 3 ? TrapPhase::Safe : phase < 4 ? TrapPhase::Warning : TrapPhase::Active;
}
float Game::trapPhaseRemaining(std::size_t i) const {
  const auto& t = traps_.at(i);
  if (t.disabledFor > 0) return t.disabledFor;
  if (t.random) {
    const float time = elapsed_ + t.cycleOffset, slot = std::floor(time / 6.F), in = time - slot * 6.F;
    const float start = randomTrapStart(slot, i);
    if (in < start) return start - in;
    if (in < start + 1) return start + 1 - in;
    if (in < start + 3) return start + 3 - in;
    return 6.F - in + randomTrapStart(slot + 1, i);
  }
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
  for(int y=std::min(VoxelWorld::size-1,int(std::floor(feet.y)));y>=0;--y)
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
 level_=std::clamp(level,0,levelCount-1); ++revision_; state_=RunState::Playing;
 liftTrail_=liftTick_=0;
 elapsed_=0; timeLimit_=challengeTimeLimit; gateOpen_=0; crouchToggle_=false; hitStop_=0;
 player_=Player{}; spawn_={8,1,25}; gate_={32,1,7}; player_.position=spawn_; player_.motion.spawnAge=10; footStep_=0;
 doors_.clear(); plates_.clear(); portals_.clear(); portalWait_=0; portalHold_=-1; teleports_=0;
 falls_=0; checkpoints_.clear(); respawn_=spawn_; lives_=livesPerLevel; respawnPending_=false;
 if(level_==0||level_==heliosLevel||level_==runLevel) {
  LevelData data; std::string error; bool loaded=false;
  const char* map=level_==0?"levels/tutorial_showcase.txt":level_==heliosLevel?"levels/helios_yard.txt":"levels/corona_run.txt";
  try { loaded=LevelData::load(craft::core::assetPath(map),data,&error); }
  catch(const std::exception& e) { error=e.what(); }
  if(loaded) { applyLevel(data); return; }
  std::cerr<<"Level could not be loaded ("<<error<<"); using the fallback yard."<<std::endl;
  level_=0;
 }
 // Fallback only (a missing map file): the original small tutorial yard, built in code.
 remaining_=level_==0?0:timeLimit_;
 room_={{4,1,4},{36,4,36}}; wallHeight_=3;
 cores_.clear(); interactionIndex_=-1; interactionCommitted_=false;
 traps_.clear(); switches_.clear(); movers_.clear(); particles_.fill(Particle{});
 world_.clear(); buildShell();
 // A jumpable block and a full-width one-cell crawl passage.
 world_.fill(12,1,22,15,2,26,2);
 world_.fill(19,2,20,23,4,27,2);
 world_.fill(19,1,19,23,4,20,2); world_.fill(19,1,27,23,4,28,2);
 world_.fill(5,1,24,6,3,27,0);
 traps_.push_back({{{12,1,12},{17,1.7F,15}},0,0});
 switches_.push_back({{10,1,16},0,0});
 movers_.push_back({{25,1.75F,10},{25,1.75F,24},{25,1.75F,10},{.65F,.75F,.65F},6,0});
 cores_={{{9,1.8F,22},false},{{16.5F,1.8F,23},false},{{28,1.8F,13},false}};
 jumpPads_={JumpPad{glm::vec3(10,1,19)}};
 message_="Collect 3 cores. Cyan arrow pads launch you higher."; messageTime_=5;
}
void Game::applyLevel(const LevelData& d) {
 // Shared authored-map loader for the tutorial showcase and Helios Yard: voxels and floor
 // paint, cores, floor traps (and their relays), moving blocks, blast doors with their plates
 // (on any floor), the jump pad and the portal pair.
 room_={{float(d.minX),1,float(d.minZ)},{float(d.maxX),4,float(d.maxZ)}};
 wallHeight_=std::clamp(d.wall,LevelData::minHeight,LevelData::maxHeight);
 timeLimit_=std::max(1.F,d.time); remaining_=timedLevel()?timeLimit_:0; elapsed_=0;
 gate_={d.gate.x,1,d.gate.y};
 world_.clear(); buildShell();
 for(int y=1;y<=d.height;++y) for(int z=d.minZ;z<d.maxZ;++z) for(int x=d.minX;x<d.maxX;++x)
  if(const auto m=d.cell(x,y,z)) world_.set(x,y,z,m);
 for(int z=d.minZ;z<d.maxZ;++z) for(int x=d.minX;x<d.maxX;++x)
  if(const auto m=d.floorPaint(x,z)) world_.set(x,0,z,m);
 // The spawn stands on whatever is built there (the parkour run starts on a raised platform).
 spawn_={d.spawn.x,surfaceAt(d.spawn.x,d.spawn.y),d.spawn.y}; respawn_=spawn_;
 player_=Player{}; player_.position=spawn_; player_.yaw=d.spawnYaw; player_.motion.spawnAge=10; footStep_=0;
 cores_.clear(); traps_.clear(); switches_.clear(); movers_.clear(); jumpPads_.clear();
 doors_.clear(); plates_.clear(); portals_.clear(); particles_.fill(Particle{});
 interactionIndex_=-1; interactionCommitted_=false; portalWait_=0; portalHold_=-1; teleports_=0;
 // Cores, the jump pad and portal ends stand on top of whatever is built at their spot.
 for(const auto& c:d.cores) cores_.push_back({{c.x,surfaceAt(c.x,c.y)+.8F,c.y},false});
 for(const auto& t:d.traps) { const float y=float(t.level); traps_.push_back({{{std::min(t.min.x,t.max.x),y,std::min(t.min.y,t.max.y)},{std::max(t.min.x,t.max.x),y+.1075F,std::max(t.min.y,t.max.y)}},t.offset,0.F});
  // A random plate also gets its own offset (0..6 s from its index), so neighbours fall out of step.
  if(t.random) { auto& f=traps_.back(); f.random=true; const float h=std::sin(float(traps_.size())*91.7F+.3F)*43758.5453F; f.cycleOffset+=6.F*(h-std::floor(h)); } }
 for(const auto& s:d.switches) switches_.push_back({{s.at.x,1.F,s.at.y},s.trap,0.F});
 for(const auto& m:d.movers) {
  const float y=float(m.level)+.55F;
  MovingHazard h{{m.a.x,y,m.a.y},{m.b.x,y,m.b.y},{},{.65F,.55F,.65F},m.period,m.offset};
  h.position=glm::mix(h.start,h.end,(1-std::cos(h.offset*2*pi/h.period))*.5F);
  movers_.push_back(h);
 }
 // Doors and plates stand on their level (1: the ground; higher: an upper floor).
 for(const auto& o:d.doors) doors_.push_back({{o.at.x,float(o.level)+1,o.at.y},o.alongZ?glm::vec3(.25F,1,o.halfWidth):glm::vec3(o.halfWidth,1,.25F),o.alongZ});
 for(const auto& p:d.plates) plates_.push_back({{p.at.x,float(p.level),p.at.y},p.door});
 checkpoints_.clear(); falls_=0; lives_=livesPerLevel; respawnPending_=false;
 for(const auto& c:d.checkpoints) checkpoints_.push_back({{c.at.x,surfaceAt(c.at.x,c.at.y),c.at.y},{c.respawn.x,surfaceAt(c.respawn.x,c.respawn.y),c.respawn.y},false,10});
 for(const auto& p:d.jumpPads) jumpPads_.push_back(JumpPad{{p.x,surfaceAt(p.x,p.y),p.y}});
 for(const auto& p:d.portals) portals_.push_back({{p.from.x,surfaceAt(p.from.x,p.from.y),p.from.y},{p.to.x,surfaceAt(p.to.x,p.to.y),p.to.y}});
 message_.clear(); messageTime_=0;
 ++revision_;
}
float Game::surfaceAt(float x,float z) const {
 const int cx=int(std::floor(x)),cz=int(std::floor(z));
 int y=1; while(y<VoxelWorld::size-2&&world_.cell(cx,y,cz)) ++y;
 return float(y);
}
const std::vector<Bounds>& Game::fixtures() const {
 fixtures_.clear();
 for(const auto& d:doors_) for(const auto& panel:d.panels()) fixtures_.push_back(panel);
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
const char* Game::nameOf(int level) { return level==0?"TUTORIAL YARD":level==heliosLevel?"HELIOS YARD":"CORONA RUN"; }
bool Game::gateUnlocked() const { return !cores_.empty() && collectedCount()==int(cores_.size()); }
int Game::collectedCount() const { return int(std::count_if(cores_.begin(),cores_.end(),[](const Core& c){return c.collected;})); }
int Game::liveParticleCount() const { return int(std::count_if(particles_.begin(),particles_.end(),[](const Particle& p){return p.remaining>0;})); }
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
 if(wordless())return "";
 if(gateUnlocked())return "Exit ready: walk through the green door at the far right.";
 if(collectedCount()==0)return "1. Move to the blue core near the start. Press your interact key.";
 if(collectedCount()==1)return "2. Jump over the low block; crouch to pass under the tunnel.";
 return "3. Avoid moving blocks and shock plates. Collect the last core to open the exit.";
}
Interaction Game::availableInteraction() const { return nearestCore()>=0?Interaction::Core:nearestSwitch()>=0?Interaction::Switch:nearestPortal()>=0?Interaction::Portal:Interaction::None; }
// ---- Portals (full edition): wake each end with E; once both are awake, step in to cross. ----
int Game::nearestPortal() const {
 float closest=1.6F; int result=-1;
 for(std::size_t i=0;i<portals_.size();++i) for(int end=0;end<2;++end) {
  const auto& link=portals_[i];
  if(end==0?link.fromOn:link.toOn) continue;
  const glm::vec3 d=player_.position-(end==0?link.from:link.to);
  const float distance=glm::length(glm::vec2(d.x,d.z));
  if(distance<closest&&std::abs(d.y)<1.F) { closest=distance; result=int(i)*2+end; }
 }
 return result;
}
glm::vec3 Game::portalEnd(int code) const { const auto& link=portals_.at(std::size_t(code/2)); return code%2==0?link.from:link.to; }
void Game::activatePortal(int code) {
 auto& link=portals_.at(std::size_t(code/2));
 const bool from=code%2==0; (from?link.fromOn:link.toOn)=true; link.pulse=1;
 const glm::vec3 colour=link.linked()?glm::vec3(.35F,1,.85F):glm::vec3(1,.7F,.25F);
 emitBurst((from?link.from:link.to)+glm::vec3(0,.3F,0),colour,14);
 if(link.linked()) { emitBurst((from?link.to:link.from)+glm::vec3(0,.3F,0),colour,10); message_="Portals linked. Step into either one to cross."; }
 else message_="Portal online. Activate its pair to link them.";
 messageTime_=3;
}
void Game::updatePortals(float dt) {
 portalWait_=std::max(0.F,portalWait_-dt);
 for(auto& link:portals_) link.pulse=std::max(0.F,link.pulse-dt*1.5F);
 const auto reach=[&](glm::vec3 at,float radius) { const glm::vec3 d=player_.position-at; return glm::length(glm::vec2(d.x,d.z))<=radius&&std::abs(d.y)<=.5F; };
 // The end just arrived at wakes again once the courier has stepped off it.
 if(portalHold_>=0&&(std::size_t(portalHold_/2)>=portals_.size()||!reach(portalEnd(portalHold_),portalRadius+.4F))) portalHold_=-1;
 if(state_!=RunState::Playing||portalWait_>0||player_.hitStun>0||player_.interactionTime>0) return;
 for(std::size_t i=0;i<portals_.size();++i) {
  auto& link=portals_[i];
  if(!link.linked()) continue;
  int end=-1;
  for(int e=0;e<2&&end<0;++e) if(int(i)*2+e!=portalHold_&&reach(e==0?link.from:link.to,portalRadius)) end=e;
  if(end<0) continue;
  const glm::vec3 departure=end==0?link.from:link.to,arrival=end==0?link.to:link.from;
  emitBurst(departure+glm::vec3(0,.5F,0),{.35F,1,.85F},10); emitBurst(arrival+glm::vec3(0,.5F,0),{.5F,.95F,1},10);
  player_.position=arrival; player_.velocity=glm::vec3(0); player_.locomotion*=.2F; player_.knockback={0,0};
  player_.grounded=true; player_.jumpActive=false; player_.coyoteTime=player_.jumpBuffer=0;
  link.pulse=1; portalWait_=portalCooldown; portalHold_=int(i)*2+(1-end); ++teleports_;
  ++revision_;  // Snaps the follow camera to the new position.
  break;
 }
}
// ---- Pressure plates and blast doors (full edition): one step unlocks the door for good. ----
void Game::updateDoors(float dt) {
 const glm::vec2 feet(player_.position.x,player_.position.z);
 for(auto& plate:plates_) {
  auto& door=doors_[std::size_t(plate.door)];
  const bool down=player_.grounded&&glm::distance(feet,glm::vec2(plate.position.x,plate.position.z))<plateRadius&&player_.position.y<plate.position.y+.35F&&player_.position.y>plate.position.y-.35F;
  if(down&&!plate.down&&!door.unlocked) { message_="Pressure plate down. Blast door unlocked."; messageTime_=2; }
  plate.down=down;
  plate.press=glm::mix(plate.press,down?1.F:0.F,1-std::exp(-18.F*dt));
  if(down) door.unlocked=true;
 }
 for(auto& door:doors_) {
  door.hold=door.unlocked?doorHold:std::max(0.F,door.hold-dt);
  // Never close on the courier.
  if(door.hold<=0&&door.open>0&&cylinderIntersectsBox(player_.position,playerRadius,bodyHeight(),door.bounds())) door.hold=.05F;
  door.open=std::clamp(door.open+(door.hold>0?2.6F:-2.2F)*dt,0.F,1.F);
 }
}
void Game::updateCheckpoints(float dt) {
 for(auto& c:checkpoints_) {
  c.activeAge=std::min(10.F,c.activeAge+dt);
  // Saved on reaching the beacon or the floor it guards (the corner it stands at).
  const glm::vec3 d=player_.position-c.position,r=player_.position-c.respawn;
  const bool near=(glm::length(glm::vec2(d.x,d.z))<1.6F&&std::abs(d.y)<1.2F)||(glm::length(glm::vec2(r.x,r.z))<2.F&&std::abs(r.y)<1.2F);
  if(!c.active&&near) {
   for(auto& other:checkpoints_) other.active=false;
   c.active=true; c.activeAge=0; respawn_=c.respawn;
   emitBurst(c.position+glm::vec3(0,1.1F,0),{.35F,1,.85F},12);
   message_="Checkpoint saved."; messageTime_=2;
  }
 }
 // Landing on the void floor (the abyss under the run) is a fall.
 if(player_.grounded&&player_.position.y<1.2F&&world_.cell(int(std::floor(player_.position.x)),0,int(std::floor(player_.position.z)))==LevelData::Void) fallBack();
}
void Game::fallBack() {
 ++falls_;
 --player_.health; player_.hurtCooldown=1; player_.motion.hitAge=0; player_.hitBlend=.85F;
 if(player_.health<=0) { beginDeath(); return; }
 player_.position=respawn_; player_.velocity=glm::vec3(0); player_.locomotion={0,0}; player_.knockback={0,0};
 player_.grounded=true; player_.jumpActive=false; player_.coyoteTime=player_.jumpBuffer=0; player_.motion.spawnAge=0;
 emitBurst(respawn_+glm::vec3(0,.6F,0),{.5F,.95F,1},14);
 message_="Fell. Back to the last checkpoint."; messageTime_=2;
 ++revision_;  // Snaps the follow camera to the checkpoint.
}
void Game::beginDeath() {
 if(state_==RunState::Lost) return;
 state_=RunState::Lost; player_.deathTime=.01F; player_.locomotion={0,0}; player_.sprinting=false;
 player_.interaction=Interaction::None; player_.interactionTime=0;
 if(livesLimited()) lives_=std::max(0,lives_-1);
 respawnPending_=!livesLimited()||lives_>0;
 message_=respawnPending_?"Robot disabled. Rebooting.":"All lives lost. Restarting the level."; messageTime_=3;
}
void Game::reboot() {
 // Beam back in at the saved spot with full armour and a second of grace (shown as a blink).
 const float yaw=player_.yaw;
 player_=Player{}; player_.position=respawn_; player_.yaw=yaw; player_.grounded=true;
 player_.motion.spawnAge=0; player_.hurtCooldown=1;
 crouchToggle_=false; interactionIndex_=-1; interactionCommitted_=false;
 state_=RunState::Playing; respawnPending_=false;
 emitBurst(respawn_+glm::vec3(0,.6F,0),{.4F,1,.9F},16);
 message_=livesLimited()?std::string("Rebooted. ")+std::to_string(lives_)+(lives_==1?" life left.":" lives left."):"Rebooted.";
 messageTime_=3;
 ++revision_;  // Snaps the follow camera to the respawn point.
}
void Game::emitBurst(glm::vec3 at,glm::vec3 colour,int count) {
 int emitted=0;
 for(auto& p:particles_) {
  if(p.remaining>0) continue;
  const int i=emitted++; const float angle=float(i)*2.399963F;
  p=Particle{}; p.kind=ParticleKind::Spark; p.position=at;
  p.velocity={std::cos(angle)*(1.4F+.2F*float(i%4)),1.6F+.3F*float(i%5),std::sin(angle)*(1.4F+.2F*float(i%4))};
  p.colour=i%4==0?glm::vec3(.9F,1,1):colour; p.size=.045F; p.lifetime=p.remaining=.45F+.05F*float(i%6); p.drag=2; p.gravity=2.5F;
  if(emitted==count) break;
 }
}
void Game::warpPlayer(glm::vec3 p) { player_.position=p; player_.velocity={0,0,0}; player_.locomotion={0,0}; player_.grounded=false; }
void Game::moveAxis(int axis,float distance) {
 if(distance==0) return;
 const float before=player_.position[axis]; player_.position[axis]+=distance;
 if(!blockedPlayer(player_.position)) return;
 float lo=0,hi=1;
 for(int i=0;i<15;++i) { float t=(lo+hi)*.5F; player_.position[axis]=before+distance*t; if(blockedPlayer(player_.position)) hi=t; else lo=t; }
 player_.position[axis]=before+distance*lo;
 if(axis==1) {
  if(distance<0) { player_.grounded=true; if(player_.velocity.y<-2.5F) { player_.motion.landAge=0; player_.landingPulse=1; player_.motion.landStrength=std::clamp(-player_.velocity.y/12.F,.2F,1.F);
    const float k=player_.motion.landStrength; emitDust(player_.position,int(6+8*k),.3F,1.4F+2.4F*k,.35F+.3F*k,.09F+.05F*k); } }
  player_.velocity.y=0; player_.jumpActive=false;
 } else player_.locomotion[axis==0?0:1]=0;
}
void Game::applyDamage(glm::vec3 source,int,bool,DamageKind) {
 if(player_.hurtCooldown>0||state_!=RunState::Playing) return;
 --player_.health; player_.hurtCooldown=1; player_.hitStun=.32F; player_.motion.hitAge=0;
 auto direction=glm::vec2(player_.position.x-source.x,player_.position.z-source.z);
 if(glm::length(direction)<.01F) direction={0,1}; else direction=glm::normalize(direction);
 // A short recoil of roughly two cells, resolved by the normal collision sweep.
 player_.knockback=direction*16.5F;
 player_.locomotion={0,0};
 player_.hitBlend=.85F; // Immediate recoil, then a short smooth recovery.
 player_.motion.hitKind=0;
 emitImpact({direction.x,0,direction.y});
 player_.motion.hitSide=direction.x*std::cos(player_.yaw)-direction.y*std::sin(player_.yaw);
 player_.motion.hitForward=-direction.x*std::sin(player_.yaw)-direction.y*std::cos(player_.yaw);
 message_="Impact! Avoid the shock floor and moving blocks."; messageTime_=2;
 player_.interaction=Interaction::None; player_.interactionTime=0;
 if(player_.health<=0) beginDeath();
}
void Game::emitImpact(glm::vec3 direction) {
 int emitted=0;
 for(auto& p:particles_) {
  if(p.remaining>0) continue;
  const int i=emitted++;
  const float angle=float(i)*2.399963F+elapsed_*1.7F;
  p=Particle{};
  p.kind=i%3==0?ParticleKind::Debris:ParticleKind::Spark;
  p.position=player_.position+glm::vec3(0,.85F,0)-direction*.32F;
  p.velocity=direction*1.7F+glm::vec3(std::cos(angle)*(1.4F+.17F*i),1.8F+.23F*(i%5),std::sin(angle)*(1.4F+.17F*i));
  p.lifetime=p.remaining=.35F+.065F*(i%7);
  p.size=p.kind==ParticleKind::Debris?.07F:.05F;
  p.colour=p.kind==ParticleKind::Debris?glm::vec3(.62F,.68F,.72F):i%4==1?glm::vec3(1,.92F,.7F):glm::vec3(1,.6F,.2F);
  p.gravity=10; p.drag=1.3F; p.seed=angle;
  if(emitted==24) break;
 }
}
void Game::emitCoreSparks(glm::vec3 position) {
 int emitted=0;
 for(auto& p:particles_) {
  if(p.remaining>0)continue;
  const int i=emitted++;
  const float angle=i*2.399963F;
  p=Particle{};p.kind=ParticleKind::Spark;p.position=position;
  p.velocity={std::cos(angle)*(1.6F+.2F*(i%4)),1.2F+.35F*(i%5),std::sin(angle)*(1.6F+.2F*(i%4))};
  p.colour=i%4==0?glm::vec3(.85F,1,1):glm::vec3(.2F,.85F,1);
  p.size=.045F;p.lifetime=p.remaining=.45F+.05F*(i%6);p.drag=2;p.gravity=3.5F;
  if(emitted==20)break;
 }
}
void Game::emitDust(glm::vec3 at,int count,float radius,float outward,float rise,float size) {
 int emitted=0;
 for(auto& p:particles_) {
  if(p.remaining>0) continue;
  const int i=emitted++; const float a=float(i)*2.399963F+elapsed_*5.3F;
  const glm::vec3 dir(std::cos(a),0,std::sin(a));
  // Starts just clear of the floor, so the particle sweep never catches it in the ground.
  p=Particle{}; p.kind=ParticleKind::Dust; p.position=at+dir*radius+glm::vec3(0,.1F,0);
  p.velocity=dir*outward*(.7F+.15F*float(i%3))+glm::vec3(0,rise*(.6F+.2F*float(i%3)),0);
  p.colour={.86F,.89F,.92F}; p.size=size*(.85F+.1F*float(i%3)); p.growth=size*2.6F;
  p.lifetime=p.remaining=.36F+.06F*float(i%4); p.drag=5.5F; p.gravity=-.35F; p.seed=a;
  if(emitted==count) break;
 }
}
void Game::emitLiftParticles(glm::vec3 position,bool trail) {
 int emitted=0;
 for(auto& p:particles_) {
  if(p.remaining>0)continue;
  const int i=emitted++;const float angle=i*2.399963F+elapsed_*9;
  const float radius=trail?.32F:.6F;
  p=Particle{};p.kind=ParticleKind::Ray;
  p.position=position+glm::vec3(std::cos(angle)*radius,.12F,std::sin(angle)*radius);
  p.velocity={std::cos(angle)*.4F,trail?1.2F:6.F+float(i%5)*.7F,std::sin(angle)*.4F};
  p.colour=i%3?glm::vec3(.15F,.8F,1):glm::vec3(.8F,1,1);
  p.size=trail?.045F:.06F;p.lifetime=p.remaining=trail?.32F:.55F+.035F*(i%5);
  p.extent={0,trail?.28F:.48F,0};p.drag=.8F;p.gravity=trail?1.F:3.F;
  if(emitted==(trail?3:20))break;
 }
}
void Game::updateParticles(float dt) {
 for(auto& p:particles_) if(p.remaining>0) {
  p.remaining=std::max(0.F,p.remaining-dt);
  if(p.remaining==0)continue;
  p.velocity*=std::exp(-p.drag*dt);p.velocity.y-=p.gravity*dt;
  for(int axis=0;axis<3;++axis) {
   const float before=p.position[axis];p.position[axis]+=p.velocity[axis]*dt;
   const glm::vec3 radius(p.size*.5F);
   if(blocked({p.position-radius,p.position+radius})) {
    p.position[axis]=before;p.velocity[axis]*=-.3F;p.bounced=true;
   }
  }
 }
}
void Game::updateInteraction(float dt,bool pressed) {
 if(player_.interactionTime>0) {
  player_.interactionTime=std::max(0.F,player_.interactionTime-dt);
  if(!interactionCommitted_&&player_.interactionTime<.2F) {
   interactionCommitted_=true;
   if(player_.interaction==Interaction::Core) {
    if(nearestCore()==interactionIndex_) {cores_[interactionIndex_].collected=true;emitCoreSparks(cores_[interactionIndex_].position);message_=gateUnlocked()?"All cores collected. Exit unlocked.":"Core collected.";messageTime_=2;}
   } else if(player_.interaction==Interaction::Switch&&nearestSwitch()==interactionIndex_) {
    auto& sw=switches_.at(interactionIndex_);traps_.at(sw.trap).disabledFor=5;sw.animation=1;
    message_="Shock floor disabled for 5 seconds.";messageTime_=3;
   } else if(player_.interaction==Interaction::Portal&&nearestPortal()==interactionIndex_) {
    activatePortal(interactionIndex_);
   }
  }
  if(player_.interactionTime==0)player_.interaction=Interaction::None;
 }
 if(pressed&&player_.interactionTime==0&&player_.hitStun==0) {
  const auto kind=availableInteraction(); if(kind==Interaction::None)return;
  interactionIndex_=kind==Interaction::Core?nearestCore():kind==Interaction::Switch?nearestSwitch():nearestPortal();interactionCommitted_=false;
  player_.interaction=kind;player_.interactionTime=.4F;
  player_.interactionTarget=kind==Interaction::Core?cores_[interactionIndex_].position:kind==Interaction::Switch?switches_[interactionIndex_].position+glm::vec3(0,1,0):portalEnd(interactionIndex_)+glm::vec3(0,.3F,0);
 }
}
void Game::update(float seconds,const Input& input) {
 if(!std::isfinite(seconds)||seconds<=0) return;
 const int n=std::max(1,int(std::ceil(std::min(seconds,1.F)*120.F)));
 for(int i=0;i<n;++i) { Input sub=input; if(i) sub.jumpPressed=sub.crouchPressed=sub.interactPressed=false; step(std::min(seconds,1.F)/n,sub); }
}
void Game::step(float dt,const Input& input) {
 updateParticles(dt);
 if(state_!=RunState::Playing) {
  // Presentation settles after the result while the run clock stays frozen. A destroyed robot
  // topples, and reboots after respawnDelay while it still has lives.
  player_.hitStun=std::max(0.F,player_.hitStun-dt);
  if(player_.health<=0) player_.deathTime+=dt;
  updateAnimation(dt,player_.position,0);
  if(respawnPending_&&player_.deathTime>=respawnDelay) reboot();
  return;
 }
 if(timedLevel()) {
  dt=std::min(dt,remaining_);
  elapsed_+=dt;
  remaining_=std::max(0.F,timeLimit_-elapsed_);
  if(remaining_<=0) {
   state_=RunState::Lost;player_.velocity={0,0,0};player_.locomotion={0,0};player_.moveBlend=0;
   player_.interaction=Interaction::None;player_.interactionTime=0;
   message_="Time up. Press R to retry.";messageTime_=3;
   return;
  }
 } else elapsed_+=dt;
 messageTime_=std::max(0.F,messageTime_-dt);
 player_.hurtCooldown=std::max(0.F,player_.hurtCooldown-dt); player_.hitStun=std::max(0.F,player_.hitStun-dt);
 player_.landingPulse=std::max(0.F,player_.landingPulse-5*dt);
 for(auto& pad:jumpPads_){pad.cooldown=std::max(0.F,pad.cooldown-dt);pad.pulse=std::max(0.F,pad.pulse-dt*3);}
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
 // Shift sprint (full edition): faster while moving and standing.
 player_.sprinting=input.sprintHeld&&glm::length(movement)>.1F&&!player_.crouching;
 const auto target=movement*(player_.crouching?crouchSpeed:player_.sprinting?sprintSpeed:speed);
 player_.locomotion=glm::mix(player_.locomotion,target,1-std::exp(-18.F*dt));
 player_.jumpBuffer=input.jumpPressed?jumpBufferDuration:std::max(0.F,player_.jumpBuffer-dt);
 player_.coyoteTime=player_.grounded?coyoteDuration:std::max(0.F,player_.coyoteTime-dt);
 if(player_.jumpBuffer>0&&player_.coyoteTime>0&&player_.interactionTime<=0&&player_.hitStun<=0&&!player_.crouching) {
  player_.velocity.y=jumpSpeed; player_.grounded=false; player_.jumpActive=true;
  player_.jumpBuffer=player_.coyoteTime=0; player_.motion.launchAge=0;
  emitDust(player_.position,6,.22F,1.1F,.45F,.085F);
 }
 if(player_.jumpActive&&!input.jumpHeld&&player_.velocity.y>3) { player_.velocity.y=3; player_.jumpActive=false; }
 player_.knockback*=std::exp(-8*dt);
 player_.velocity.x=player_.locomotion.x+player_.knockback.x; player_.velocity.z=player_.locomotion.y+player_.knockback.y;
 moveAxis(0,player_.velocity.x*dt); moveAxis(2,player_.velocity.z*dt);
 // Collisions can stop horizontal motion, so choose the facing after resolving them.
 float turn=0;
 if(input.facingYaw&&std::isfinite(*input.facingYaw)) {
  if(player_.cameraMode()==CameraMode::Follow) {
   turn=std::remainder(*input.facingYaw-player_.yaw,2*pi);
   player_.yaw=*input.facingYaw;
  }
 } else if(glm::length(movement)>.01F) {
  turn=std::remainder(std::atan2(-movement.x,-movement.y)-player_.yaw,2*pi);
  player_.yaw+=turn*(1-std::exp(-16*dt));
 }
 player_.velocity.y-=(player_.velocity.y<0?fallGravity:gravity)*dt;
 player_.grounded=false; moveAxis(1,player_.velocity.y*dt);
 // Pads are flush with the floor: feet must actually land/step on their surface.
 for(auto& pad:jumpPads_) if(player_.grounded&&player_.hitStun<=0&&pad.cooldown==0&&
   std::abs(player_.position.y-pad.position.y)<.08F&&
   std::abs(player_.position.x-pad.position.x)<.8F&&std::abs(player_.position.z-pad.position.z)<.8F) {
  player_.velocity.y=jumpPadSpeed;player_.grounded=false;player_.jumpActive=false;
  player_.jumpBuffer=player_.coyoteTime=0;player_.landingPulse=0;player_.motion.launchAge=0;
  pad.cooldown=.4F;pad.pulse=1;
  emitLiftParticles(pad.position,false);liftTrail_=.4F;liftTick_=0;
  message_="Jump pad! Steer in the air with movement keys.";messageTime_=2;
  break;
 }
 if(liftTrail_>0) {
  liftTrail_=std::max(0.F,liftTrail_-dt);
  if(player_.velocity.y<=0||player_.grounded)liftTrail_=0;
  else {liftTick_+=dt;while(liftTick_>=.04F){liftTick_-=.04F;emitLiftParticles(player_.position,true);}}
 }
 for(std::size_t i=0;i<traps_.size();++i) if(trapPhase(i)==TrapPhase::Active&&traps_[i].touchesFeet(player_.position,footRadius)) applyDamage((traps_[i].area.min+traps_[i].area.max)*.5F,int(i),true,DamageKind::Electric);
 updateDoors(dt); updatePortals(dt); updateCheckpoints(dt);
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
 player_.sprintBlend=blend(player_.sprintBlend,player_.sprinting?1.F:0.F,12);
 player_.backGlow=blend(player_.backGlow,player_.sprinting?1.F:0.F,10);
 player_.airBlend=blend(player_.airBlend,player_.grounded?0.F:1.F,20);
 player_.crouchBlend=blend(player_.crouchBlend,player_.crouching?1.F:0.F,14);
 player_.landingBlend=blend(player_.landingBlend,player_.landingPulse,35);
 player_.hitBlend=blend(player_.hitBlend,player_.motion.hitAge<.08F?1.F:0.F,12);
 player_.idlePhase+=dt;
 auto& a=player_.motion;
 // spawnAge and teleportAge drive the beam-in pop after a respawn; they must keep counting.
 for(float* age:{&a.startAge,&a.stopAge,&a.pivotAge,&a.launchAge,&a.landAge,&a.stepAge,&a.hitAge,&a.spawnAge,&a.teleportAge}) *age=std::min(10.F,*age+dt);
 a.verticalVelocity=blend(a.verticalVelocity,player_.velocity.y,18);
 motionSpring(a.lean,a.leanVelocity,-.13F*player_.moveBlend,20,.65F,dt);
 motionSpring(a.lookYaw,a.lookVelocity,std::clamp(turn*.45F,-.6F,.6F),26,.85F,dt);
 if(player_.grounded) player_.walkPhase+=travelled*3.8F/modelScale;
 const int step=int(std::floor(player_.walkPhase/pi));
 if(step!=footStep_) {
  footStep_=step;
  const float v=glm::length(player_.locomotion);
  if(player_.grounded&&v>speed*.6F&&state_==RunState::Playing) {
   const glm::vec3 forward(-std::sin(player_.yaw),0,-std::cos(player_.yaw)),side(std::cos(player_.yaw),0,-std::sin(player_.yaw));
   const glm::vec3 foot=player_.position+side*((step%2?.17F:-.17F)*modelScale)-forward*.12F;
   emitDust(foot,player_.sprinting?3:2,.06F,.5F+.08F*v,.3F,player_.sprinting?.075F:.06F);
  }
 }
}
} // namespace relay
