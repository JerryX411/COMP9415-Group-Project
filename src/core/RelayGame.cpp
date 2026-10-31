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

Game::Game() { reset(0); }
void Game::reset(int level) {
  level_ = std::clamp(level, 0, practiceLevel);
  ++revision_;
  state_ = RunState::Playing;
  interactionIndex_ = -1; interactionCommitted_ = false;
  elapsed_ = gateOpen_ = sparkClock_ = 0.0F;
  hitStop_ = exhaustClock_ = 0.0F; lastWake_ = -1.0F; crouchToggle_ = false; slideClock_ = slideCooldown_ = 0;
  // Built-in layout anchors (a level file may replace them below).
  room_ = tutorial() ? Bounds{{4, 1, 4}, {60, 4, 46}} : practice() ? Bounds{{4, 1, 4}, {60, 4, 60}} : Bounds{{4, 1, 4}, {28, 4, 28}};
  spawn_ = tutorial() ? glm::vec3(7, 1, 39) : practice() ? glm::vec3(15.5F, 1, 53) : glm::vec3(7, 1, 24);
  gate_ = tutorial() ? glm::vec3(8, 1, 7.5F) : practice() ? glm::vec3(30, 1, 6) : glm::vec3(25, 1, 7);  // practice: none shown
  spawnYaw_ = tutorial() ? -pi * .5F : 0.F;  // Training faces east down the first hall.
  remaining_ = startTime_ = level_ == 0 ? 150.0F : 180.0F;
  wallHeight_ = 3;
  random_ = 0x52454c41 + unsigned(level_);
  player_ = Player{};
  player_.position = spawnPoint();
  player_.yaw = spawnYaw_;
  particles_.fill(Particle{});
  cores_.clear(); traps_.clear(); switches_.clear(); movers_.clear(); checkpoints_.clear();
  respawnIndex_ = -1; respawnPending_ = false; lastHitSource_ = -1; sourceImmunity_ = graceTime_ = 0;
  lives_ = livesPerLevel; tutorialStep_ = 0; tutorialAge_ = bounceTime_ = bounceCooldown_ = jetClock_ = boostTime_ = slideSurge_ = 0; resumeFlight_ = false; carrying_ = false; trailClock_ = 0; wallTouch_ = wallKick_ = 0; wallJumps_ = 0; wallJumpsDone_ = 0; pads_.clear(); plates_.clear(); doors_.clear(); pickups_.clear(); bonusAge_ = 10; drains_.clear(); drainCooldown_ = 0; penaltyAge_ = 10;
  portals_.clear(); teleports_ = jetsUsed_ = 0; portalWait_ = 0; portalHold_ = -1; jetting_ = false; jetTrailClock_ = 0; lastSprintTap_ = -10; wallActionWait_ = 0; wallQueue_ = 0;
  world_.clear();
  buildShell();
  if (tutorial()) {
    // Training ground: three halls in a snake, each teaching one system. Hall 3 loops: the exit
    // is at its near (west) end, locked until the last core, which sits on an upper floor at the
    // far end; a one-way portal pair brings the courier back.
    // Hall 1 (south, heading east): move, sprint, jump the low barrier, crawl, slide the tunnel.
    world_.fill(26, 1, 32, 27, 2, 46, 3);                                         // low barrier
    world_.fill(34, 1, 32, 35, 3, 46, 2); world_.fill(34, 1, 37, 35, 2, 41, 0);   // crawl hole
    world_.fill(44, 1, 32, 52, 3, 46, 2); world_.fill(44, 1, 36, 52, 2, 42, 0);   // slide tunnel
    world_.fill(4, 1, 30, 53, 3, 32, 2);                                          // hall 1 / hall 2
    // Hall 2 (heading west): a core and checkpoint, a relay-switched trap, a hazard gap,
    // then a pressure plate and speed pad for the blast door north.
    world_.fill(29, 1, 17, 33, 3, 21, 2); world_.fill(29, 1, 26, 33, 3, 30, 2);   // hazard gap
    world_.fill(4, 1, 15, 6, 3, 17, 2); world_.fill(10, 1, 15, 60, 3, 17, 2);     // hall 2 / hall 3, door at x 6..10
    // Hall 3 (heading east from the door): the exit gate and a portal at the door, a pad to fly
    // the first trap, a jetpack to burst across a long second one, the wall-jump lane, spring
    // boots, and an upper floor of two-high blocks (with a gap to jump) carrying the last core and
    // the portal that leads back to the exit.
    world_.fill(47, 1, 4, 53, 3, 8, 2); world_.fill(55, 1, 4, 60, 3, 9, 2);       // upper floor
    cores_ = {{{56, 1.65F, 24}}, {{37, 1.65F, 23.5F}}, {{57.5F, 3.65F, 5.5F}}};
    traps_.push_back({{{40, 1.0F, 17}, {44, 1.1075F, 30}}, 0.0F, 0.0F});
    traps_.push_back({{{24, 1.0F, 4}, {28, 1.1075F, 15}}, 1.2F, 0.0F});
    traps_.push_back({{{33, 1.0F, 4}, {43, 1.1075F, 15}}, 2.5F, 0.0F});  // jet across
    switches_.push_back({{47.5F, 1.0F, 21.F}, 0});
    checkpoints_ = {{{51.5F, 1, 27.5F}, {53.1F, 1, 27.5F}}, {{13.5F, 1, 9.5F}, {13.5F, 1, 11.1F}}};
    movers_.push_back({{31, 1.55F, 21.8F}, {31, 1.55F, 25.2F}, {}, {0.65F, 0.55F, 0.65F}, 3.2F, 0.0F});
    doors_.push_back({{8, 2, 16}, {2, 1, .25F}, false});
    plates_.push_back({{16, 1, 26.5F}, 0});
    pads_.push_back({{{10.5F, 1, 19.5F}, {14.5F, 1.02F, 22}}, {-1, 0}});
    pads_.push_back({{{46, 1, 9}, {49, 1.02F, 11.5F}}, {1, 0}});
    pads_.push_back({{{19, 1, 8.5F}, {22, 1.02F, 11}}, {1, 0}});  // sprint onto it to fly over the trap
    // Power-ups: a shield before the relay trap; in hall 2 a clock behind a crimson strip (in a
    // mission one adds 30 s and the other costs 5 s; training has no clock, so they only show it);
    // in hall 3 the jetpack before the long trap and spring boots below the upper floor.
    pickups_ = {{{49.5F, 1, 18.6F}, PowerUp::Shield}, {{51.F, 1, 12.5F}, PowerUp::Spring}, {{22.5F, 1, 28.5F}, PowerUp::Time}, {{30.5F, 1, 9.75F}, PowerUp::Jet}};
    drains_.push_back({{{20, 1, 26}, {25, 1.02F, 27.6F}}});
    // The portals: one end on the upper floor beside the last core, the other just inside the door,
    // in front of the exit gate.
    portals_.push_back({{57.5F, 3, 7.6F}, {8, 1, 11.5F}});
    // Hall 3's east wall past the last pad is the boosted wall-jump lesson.
  } else if (practice()) {
    // Practice field: one big open floor. Near the spawn every power-up waits in a row (two of
    // each, one clock); along the east side a two-lane runway marked by low curbs, mostly plain
    // floor with a few speed pads: north up the west lane, south down the east lane.
    world_.fill(43, 1, 10, 44, 2, 28, 3); world_.fill(43, 1, 36, 44, 2, 54, 3);   // west curb, gap mid-way
    world_.fill(51, 1, 12, 52, 2, 52, 3);                                         // lane divider
    pads_.push_back({{{44.5F, 1, 44}, {50.5F, 1.02F, 46.5F}}, {0, -1}});
    pads_.push_back({{{44.5F, 1, 30}, {50.5F, 1.02F, 32.5F}}, {0, -1}});
    pads_.push_back({{{44.5F, 1, 16}, {50.5F, 1.02F, 18.5F}}, {0, -1}});
    pads_.push_back({{{52.5F, 1, 20}, {59.5F, 1.02F, 22.5F}}, {0, 1}});
    pads_.push_back({{{52.5F, 1, 40}, {59.5F, 1.02F, 42.5F}}, {0, 1}});
    pickups_ = {{{8.F, 1, 47}, PowerUp::Spring}, {{10.5F, 1, 47}, PowerUp::Spring}, {{13.F, 1, 47}, PowerUp::Shield}, {{15.5F, 1, 47}, PowerUp::Shield},
                {{18.F, 1, 47}, PowerUp::Jet}, {{20.5F, 1, 47}, PowerUp::Jet}, {{23.F, 1, 47}, PowerUp::Time}};
  } else if (level_ == 0) {
    world_.fill(10, 1, 19, 14, 2, 20, 3);
    world_.fill(19, 1, 20, 20, 3, 25, 2);
    world_.fill(8, 1, 10, 12, 3, 11, 2);
    cores_ = {{{9, 1.65F, 22}}, {{17, 1.65F, 16}}, {{25, 1.65F, 10}}};
    traps_.push_back({{{15, 1.0F, 12}, {19, 1.1075F, 14}}, 0.0F, 0.0F});
    switches_.push_back({{12.5F, 1.0F, 15.8F}, 0});
    checkpoints_ = {{{16.5F, 1, 23.5F}, {16.5F, 1, 21.9F}}, {{22.5F, 1, 12.5F}, {24.1F, 1, 12.5F}}};
    // East partition: its blast door (opened by the plate in the north-east room) is the short
    // way down to the last core, and the pad behind it launches the courier through.
    world_.fill(20, 1, 17, 25, 3, 18, 2); world_.fill(27, 1, 17, 28, 3, 18, 2);
    doors_.push_back({{26, 2, 17.5F}, {1, 1, .25F}, false});
    plates_.push_back({{23, 1, 24.5F}, 0});
    pads_.push_back({{{8, 1, 26.1F}, {10.5F, 1.02F, 27.7F}}, {1, 0}});
    pads_.push_back({{{25.2F, 1, 19}, {26.8F, 1.02F, 21.5F}}, {0, -1}});
    // The clock sits in the quiet south-east corner: a detour that buys 30 s.
    pickups_ = {{{12.5F, 1, 13.4F}, PowerUp::Shield}, {{6.F, 1, 6.5F}, PowerUp::Spring}, {{26.5F, 1, 25.5F}, PowerUp::Time}};
    // Crimson tiles guard the clock from the west (clear of the door plate): jump them, go round by
    // the north, or pay 5 s.
    drains_.push_back({{{24, 1, 23}, {26, 1.02F, 28}}});
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
    checkpoints_ = {{{15.5F, 1, 15.5F}, {17.1F, 1, 15.5F}}, {{24.5F, 1, 24.5F}, {26.1F, 1, 24.5F}}};
    // A blast door in the west wall bypasses the first trap. Its plate is at the far end of the
    // west corridor, with a speed pad on the way back to help beat the hold.
    world_.fill(11, 1, 23, 12, 3, 25, 0);
    doors_.push_back({{11.5F, 2, 24}, {.25F, 1, 1}, true});
    plates_.push_back({{5.4F, 1, 9.5F}, 0});
    pads_.push_back({{{4.6F, 1, 11}, {6.2F, 1.02F, 13.5F}}, {0, 1}});
    pads_.push_back({{{26.F, 1, 20.5F}, {27.6F, 1.02F, 23}}, {0, -1}});
    // The clock waits at the south end of the west corridor.
    pickups_ = {{{9.F, 1, 11.5F}, PowerUp::Shield}, {{16.F, 1, 6.5F}, PowerUp::Spring}, {{5.5F, 1, 25.5F}, PowerUp::Time}};
    // A crimson strip across the middle hall, short of the south core.
    drains_.push_back({{{12, 1, 21}, {16, 1.02F, 22}}});
    movers_.push_back({{14, 1.55F, 10.5F}, {19, 1.55F, 10.5F}, {}, {0.65F, 0.55F, 0.65F}, 4.8F, 0.0F});
    movers_.push_back({{23, 1.55F, 12}, {26, 1.55F, 19}, {}, {0.65F, 0.55F, 0.65F}, 6.0F, 1.2F});
  }
  std::string intro = tutorial() ? "Training: follow the green beacon. No time limit."
                     : practice() ? "Practice field: power-ups return in 1 s."
                     : level_ == 0 ? "Collect 3 cores with E. Shift sprints. Avoid the hazards."
                                   : "Five cores. Two relays. Watch the moving hazards.";
  if (useLevelFiles) {
    // An edited map (assets/levels/levelN.txt, from RelayEditor) replaces the built-in layout.
    // A missing file keeps the built-in one; a broken one is reported and skipped.
    std::filesystem::path path;
    try { path = craft::core::assetPath("levels/" + LevelData::fileName(level_)); } catch (const std::exception&) {}
    LevelData file; std::string error;
    if (!path.empty()) {
      if (LevelData::load(path, file, &error)) { applyLevel(file); if (!file.message.empty()) intro = file.message; }
      else std::cerr << "Level file skipped: " << error << '\n';
    }
  }
  for (auto& h : movers_) {
    const float t = (1.0F - std::cos(h.offset * 2.0F * pi / h.period)) * 0.5F;
    h.position = glm::mix(h.start, h.end, t);
  }
  notify(intro);
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
LevelData Game::captureLevel() const {
  LevelData d;
  d.name = tutorial() ? "Training" : practice() ? "Practice" : level_ == 0 ? "Courtyard" : "Corridors";
  d.message = message_;
  d.minX = int(room_.min.x); d.minZ = int(room_.min.z); d.maxX = int(room_.max.x); d.maxZ = int(room_.max.z);
  d.time = startTime_; d.wall = wallHeight_;
  d.spawn = {spawn_.x, spawn_.z}; d.spawnYaw = spawnYaw_; d.gate = {gate_.x, gate_.z};
  // As many layers as the tallest structure inside the room needs (at least 3).
  d.height = LevelData::minHeight;
  for (int y = LevelData::maxHeight; y > LevelData::minHeight && d.height == LevelData::minHeight; --y)
    for (int z = d.minZ; z < d.maxZ && d.height == LevelData::minHeight; ++z)
      for (int x = d.minX; x < d.maxX; ++x) if (world_.cell(x, y, z)) { d.height = y; break; }
  d.cells.assign(std::size_t(d.height * d.width() * d.depth()), LevelData::Empty);
  for (int y = 1; y <= d.height; ++y)
    for (int z = d.minZ; z < d.maxZ; ++z)
      for (int x = d.minX; x < d.maxX; ++x) d.setCell(x, y, z, world_.cell(x, y, z));
  for (const auto& c : cores_) d.cores.push_back({c.position.x, c.position.z});
  for (const auto& t : traps_) d.traps.push_back({{t.area.min.x, t.area.min.z}, {t.area.max.x, t.area.max.z}, t.cycleOffset});
  for (const auto& s : switches_) d.switches.push_back({{s.position.x, s.position.z}, s.trap});
  for (const auto& c : checkpoints_) d.checkpoints.push_back({{c.position.x, c.position.z}, {c.respawn.x, c.respawn.z}});
  for (const auto& m : movers_) d.movers.push_back({{m.start.x, m.start.z}, {m.end.x, m.end.z}, m.period, m.offset});
  for (const auto& o : doors_) d.doors.push_back({{o.centre.x, o.centre.z}, o.alongZ ? o.half.z : o.half.x, o.alongZ});
  for (const auto& p : plates_) d.plates.push_back({{p.position.x, p.position.z}, p.door});
  for (const auto& p : pads_) d.pads.push_back({{p.area.min.x, p.area.min.z}, {p.area.max.x, p.area.max.z}, p.direction});
  for (const auto& p : pickups_) d.pickups.push_back({{p.position.x, p.position.z}, int(p.kind)});
  for (const auto& t : drains_) d.drains.push_back({{t.area.min.x, t.area.min.z}, {t.area.max.x, t.area.max.z}});
  for (const auto& p : portals_) d.portals.push_back({{p.from.x, p.from.z}, {p.to.x, p.to.z}});
  return d;
}
void Game::applyLevel(const LevelData& d) {
  room_ = {{float(d.minX), 1, float(d.minZ)}, {float(d.maxX), 4, float(d.maxZ)}};
  spawn_ = {d.spawn.x, 1, d.spawn.y}; spawnYaw_ = d.spawnYaw; gate_ = {d.gate.x, 1, d.gate.y};
  wallHeight_ = std::clamp(d.wall, LevelData::minHeight, LevelData::maxHeight);
  if (timed()) remaining_ = startTime_ = std::max(1.F, d.time);
  player_.position = spawn_; player_.yaw = spawnYaw_;
  world_.clear(); buildShell();
  for (int y = 1; y <= d.height; ++y)
    for (int z = d.minZ; z < d.maxZ; ++z)
      for (int x = d.minX; x < d.maxX; ++x)
        if (const auto m = d.cell(x, y, z)) world_.set(x, y, z, m);
  cores_.clear(); traps_.clear(); switches_.clear(); checkpoints_.clear(); movers_.clear();
  doors_.clear(); plates_.clear(); pads_.clear(); pickups_.clear(); drains_.clear(); portals_.clear();
  // Heights are fixed per item type, as in the built-in levels; cores, pickups and portals stand on
  // top of whatever is built at their spot.
  for (const auto& c : d.cores) cores_.push_back({{c.x, surfaceAt(c.x, c.y) + .65F, c.y}});
  for (const auto& p : d.portals) portals_.push_back({{p.from.x, surfaceAt(p.from.x, p.from.y), p.from.y}, {p.to.x, surfaceAt(p.to.x, p.to.y), p.to.y}});
  for (const auto& t : d.traps) traps_.push_back({{{std::min(t.min.x, t.max.x), 1.0F, std::min(t.min.y, t.max.y)}, {std::max(t.min.x, t.max.x), 1.1075F, std::max(t.min.y, t.max.y)}}, t.offset, 0.0F});
  for (const auto& s : d.switches) switches_.push_back({{s.at.x, 1.0F, s.at.y}, s.trap});
  for (const auto& c : d.checkpoints) checkpoints_.push_back({{c.at.x, 1, c.at.y}, {c.respawn.x, 1, c.respawn.y}});
  for (const auto& m : d.movers) movers_.push_back({{m.a.x, 1.55F, m.a.y}, {m.b.x, 1.55F, m.b.y}, {}, {0.65F, 0.55F, 0.65F}, m.period, m.offset});
  for (const auto& o : d.doors) doors_.push_back({{o.at.x, 2, o.at.y}, o.alongZ ? glm::vec3(.25F, 1, o.halfWidth) : glm::vec3(o.halfWidth, 1, .25F), o.alongZ});
  for (const auto& p : d.plates) plates_.push_back({{p.at.x, 1, p.at.y}, p.door});
  for (const auto& p : d.pads) pads_.push_back({{{std::min(p.min.x, p.max.x), 1, std::min(p.min.y, p.max.y)}, {std::max(p.min.x, p.max.x), 1.02F, std::max(p.min.y, p.max.y)}}, glm::length(p.direction) > .01F ? glm::normalize(p.direction) : glm::vec2(1, 0)});
  for (const auto& p : d.pickups) pickups_.push_back({{p.at.x, surfaceAt(p.at.x, p.at.y), p.at.y}, PowerUp(std::clamp(p.kind, 0, 3))});
  for (const auto& t : d.drains) drains_.push_back({{{std::min(t.min.x, t.max.x), 1, std::min(t.min.y, t.max.y)}, {std::max(t.min.x, t.max.x), 1.02F, std::max(t.min.y, t.max.y)}}});
  for (auto& h : movers_) {
    const float t = (1.0F - std::cos(h.offset * 2.0F * pi / h.period)) * 0.5F;
    h.position = glm::mix(h.start, h.end, t);
  }
  ++revision_;  // The terrain changed: renderers rebuild their mesh.
}

float Game::surfaceAt(float x, float z) const {
  const int cx = int(std::floor(x)), cz = int(std::floor(z));
  int y = 1;
  while (y < VoxelWorld::size - 2 && world_.cell(cx, y, cz)) ++y;
  return float(y);
}
Bounds Game::playerBounds() const {
  return {player_.position + glm::vec3(-playerRadius, 0, -playerRadius),
          player_.position + glm::vec3(playerRadius, bodyHeight(), playerRadius)};
}
const std::vector<Bounds>& Game::fixtures() const {
  // Rebuilt in place: collision queries run hundreds of times per frame, so no allocation.
  auto& boxes=fixtures_; boxes.clear();
  for(const auto& sw:switches_) boxes.push_back({sw.position+glm::vec3(-.375F,0,-.325F),sw.position+glm::vec3(.375F,1.05F,.325F)});
  const auto p=gatePosition();
  if(!practice()) for(float side:{-1.F,1.F}) {  // the practice field has no exit
    const auto post=p+glm::vec3(side*1.5F,1.5F,0);
    boxes.push_back({post-glm::vec3(.2F,1.5F,.25F),post+glm::vec3(.2F,1.5F,.25F)});
    const auto door=p+glm::vec3(side*(.65F+gateOpen_*1.35F),1.35F,0);
    boxes.push_back({door-glm::vec3(.64F,1.35F,.23F),door+glm::vec3(.64F,1.35F,.23F)});
  }
  if(!practice()) boxes.push_back({p+glm::vec3(-1.7F,2.8F,-.25F),p+glm::vec3(1.7F,3.2F,.25F)});
  for(const auto& d:doors_) for(const auto& panel:d.panels()) boxes.push_back(panel);
  // Checkpoint beacons are solid: the floor pad and the pylon with its lamp head.
  for(const auto& c:checkpoints_) {
    boxes.push_back({c.position+glm::vec3(-.76F,0,-.76F),c.position+glm::vec3(.76F,.1F,.76F)});
    boxes.push_back({c.position+glm::vec3(-.2F,0,-.2F),c.position+glm::vec3(.2F,1.35F,.2F)});
  }
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
  const float height=bodyHeight();
  if(world_.collidesCylinder(feet,playerRadius,height)) return true;
  for(std::size_t i=0;i<movers_.size();++i)
    if(int(i)!=ignoreMover&&movers_[i].intersectsPlayer(feet,playerRadius,height)) return true;
  for(const auto& part:fixtures()) if(cylinderIntersectsBox(feet,playerRadius,height,part)) return true;
  return false;
}
void Game::moveAxis(int axis, float distance) {
  if (distance == 0.0F) return;
  const float before = player_.position[axis];
  player_.position[axis] += distance;
  if (!blockedPlayer(player_.position)) return;
  int contact=-1;
  for(std::size_t i=0;i<movers_.size();++i) if(movers_[i].intersectsPlayer(player_.position,playerRadius,bodyHeight())) contact=int(i);
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
        // Impact strength scales a billowing dust skirt; heavy drops crack the floor:
        // ring, flying chips, a scuff mark and a beat of hit-stop.
        const float s=player_.motion.landStrength;
        const auto feet=player_.position+glm::vec3(0,.04F,0);
        dust(feet,int(8+12*s),2.F+2.4F*s,.15F+.08F*s);
        if(s>.7F) shockwave(feet,{.86F,.90F,.92F},1.1F+1.F*s,.42F);
        if(s>.5F) burst(feet+glm::vec3(0,.03F,0),{.42F,.44F,.45F},int(3+5*s),1.2F+.6F*s);
        if(s>1.1F) { shockwave(feet,{1,1,1},.6F+.6F*s,.25F); scorch(feet,.7F+.4F*s,{.16F,.17F,.18F}); smoke(feet+glm::vec3(0,.2F,0),4,.28F,{.78F,.80F,.81F}); }
      }
    }
    player_.velocity.y = 0;
    player_.jumpActive=false;
  } else {
    // A shove that meets a wall thuds into it (dust, chips). Only the fallen body rebounds:
    // bouncing a live courier back into a hazard would change the difficulty.
    const int k=axis==0?0:1;
    if(std::abs(player_.knockback[k])>2.F) {
      glm::vec3 contact=player_.position+glm::vec3(0,.6F,0); contact[axis]+=(distance>0?1.F:-1.F)*playerRadius;
      dust(contact,5,1.2F,.14F); burst(contact,{.55F,.57F,.58F},3,1.4F);
      player_.knockback[k]=state_==RunState::Lost?player_.knockback[k]*-.4F:0.F;
    } else player_.knockback[k]=0;
    // Running into a wall at speed bounces off it and turns the courier with the rebound;
    // slower contact just stops. The cool-down stops a held key from bouncing it repeatedly.
    const float into=player_.locomotion[k];
    // Remember the wall for a moment so a boosted jump can kick off it.
    if(state_==RunState::Playing&&contact<0&&into*distance>0&&std::abs(into)>3.F) {
      wallTouch_=wallGrace; wallSpeed_=glm::length(player_.locomotion);
      wallNormal_={0,0}; wallNormal_[k]=distance>0?-1.F:1.F;
      wallPoint_=player_.position+glm::vec3(0,.55F,0); wallPoint_[axis]+=(distance>0?1.F:-1.F)*playerRadius;
    }
    // Moving hazards are excluded: their blow already knocks the courier back.
    if(state_==RunState::Playing&&contact<0&&player_.hitStun<=0&&into*distance>0&&std::abs(into)>bounceSpeed&&bounceCooldown_<=0) {
      player_.locomotion[k]=-into*bounceRestitution; player_.locomotion[1-k]*=.85F;
      bounceCooldown_=.6F; if(!player_.sliding) bounceTime_=bounceLock;
      if(player_.boost!=Boost::None) endBoost(false);
      auto& m=player_.motion; m.bounceAge=0; m.bounceStrength=std::clamp(std::abs(into)/sprintSpeed,.4F,1.6F);
      glm::vec3 contact=player_.position+glm::vec3(0,.7F,0),away(0); contact[axis]+=(distance>0?1.F:-1.F)*playerRadius; away[axis]=distance>0?-1.F:1.F;
      dust(contact,6,1.4F,.13F); burst(contact,{.55F,.57F,.58F},3,1.3F);
      if(std::abs(into)>7.F) sparks(contact,away+glm::vec3(0,.4F,0),{1,.75F,.4F},6,3.5F,1.2F);
    } else player_.locomotion[k]=0;
    player_.velocity[axis]=0;
  }
  if(contact>=0 && state_==RunState::Playing) applyDamage(movers_[std::size_t(contact)].position,100+contact);
}

void Game::update(float seconds, const Input& input) {
  if (!std::isfinite(seconds) || seconds <= 0) return;
  // Physics runs at <= 1/120 s even during a slow render frame, preventing
  // narrow trap/wall tunnelling and keeping jump height independent of FPS.
  const int steps = std::max(1, int(std::ceil(std::min(seconds, 1.0F) * 120.0F)));
  const float dt = std::min(seconds, 1.0F) / float(steps);
  for (int i = 0; i < steps; ++i) {
    Input sub = input;
    if (i != 0) sub.jumpPressed = sub.interactPressed = sub.crouchPressed = sub.sprintPressed = false;
    step(dt, sub);
  }
}

void Game::step(float dt, const Input& input) {
  for(auto& checkpoint:checkpoints_) checkpoint.activeAge=std::min(10.F,checkpoint.activeAge+dt);
  if(state_==RunState::Won) {
    // Confetti, rising motes and fireworks overhead while the courier celebrates.
    exhaustClock_+=dt; sparkClock_+=dt;
    if(player_.motion.victoryAge<2.8F&&sparkClock_>.32F) {
      sparkClock_=0;
      const glm::vec3 colours[]{{.2F,.9F,1},{1,.6F,.15F},{.35F,1,.75F},{1,.35F,.75F}};
      const float a=random01()*6.2831853F,r=1.2F+1.8F*random01(),h=random01(),pick=random01();
      firework(player_.position+glm::vec3(std::cos(a)*r,2.6F+1.4F*h,std::sin(a)*r),colours[int(pick*3.99F)]);
    }
    if(player_.motion.victoryAge<2.2F&&exhaustClock_>.07F) {
      exhaustClock_=0;
      const glm::vec3 colours[]{{.25F,.85F,1},{1,.62F,.18F},{.94F,.95F,.92F}};
      const float a=random01()*6.2831853F,r=.4F+.5F*random01(),up=random01(),pick=random01();
      const glm::vec3 v(std::cos(a)*1.6F,5.F+2.F*up,std::sin(a)*1.6F);
      for(int i=0;i<3;++i) if(auto* p=spawn(ParticleKind::Debris,player_.position+glm::vec3(std::cos(a+float(i)*2.1F)*r,1.6F,std::sin(a+float(i)*2.1F)*r),glm::vec3(std::cos(a+float(i)*2.1F)*1.6F,v.y,std::sin(a+float(i)*2.1F)*1.6F),colours[(int(pick*2.99F)+i)%3],.95F,.075F)) { p->gravity=7; p->drag=2.4F; p->growth=1; p->turbulence=7; }
      if(auto* p=spawn(ParticleKind::Glow,player_.position+glm::vec3(std::sin(a)*.9F,.3F,std::cos(a)*.9F),{0,1.4F,0},{.35F,.9F,1},.9F,.09F)) { p->gravity=-.8F; p->drag=.8F; }
    }
    player_.motion.victoryAge+=dt; player_.idlePhase+=dt;
    player_.moveBlend*=std::exp(-18.F*dt); player_.sprintBlend*=std::exp(-18.F*dt);
    player_.airBlend*=std::exp(-18.F*dt); updateParticles(dt); return;
  }
  if(state_==RunState::Lost) {
    player_.deathTime+=dt;
    sparkClock_+=dt;
    // Topple as a rigid body: free spin while airborne; on the ground gravity torque
    // (an inverted pendulum about the feet edge) pulls it over until it slaps flat,
    // rebounds a little and settles.
    auto& fall=player_.motion; fall.slapAge=std::min(10.F,fall.slapAge+dt);
    constexpr float lying=1.5708F;  // Flat on the floor.
    if(player_.grounded) fall.fallSpin+=(11.F*std::sin(std::max(fall.fallAngle,.04F))+.6F)*dt;
    fall.fallAngle+=fall.fallSpin*dt;
    bool slapped=false;
    if(fall.fallAngle>=lying) {
      fall.fallAngle=lying;
      if(player_.grounded&&fall.fallSpin>1.2F) { slapped=fall.slapAge>.3F; fall.slapAge=0; fall.fallSpin*=-.3F; }
      else if(player_.grounded||fall.fallSpin>0) fall.fallSpin=0;
    }
    if(slapped) {
      // The body slaps down: a line of dust along the fall, a ring and loose bits.
      const auto& m=player_.motion;
      glm::vec3 local(std::clamp(m.hitSide,-1.F,1.F),0,-std::clamp(m.hitForward,-1.F,1.F));
      if(glm::length(local)<.2F) local={0,0,-1};
      local=glm::normalize(local);
      const glm::vec3 dir(std::cos(player_.yaw)*local.x+std::sin(player_.yaw)*local.z,0,-std::sin(player_.yaw)*local.x+std::cos(player_.yaw)*local.z);
      for(int i=0;i<5;++i) dust(player_.position+dir*(.3F+.38F*float(i))+glm::vec3(0,.05F,0),3,1.7F,.16F);
      shockwave(player_.position+dir*.9F+glm::vec3(0,.04F,0),{.86F,.88F,.9F},1.4F,.35F);
      burst(player_.position+dir*.9F+glm::vec3(0,.1F,0),{.30F,.32F,.34F},6,1.8F);
    }
    player_.motion.hitAge=std::min(10.F,player_.motion.hitAge+dt); player_.motion.shockAge=std::min(10.F,player_.motion.shockAge+dt);
    player_.idlePhase+=dt; shockEffects(dt);
    if(player_.deathTime<1.4F && sparkClock_>.08F) {
      sparkClock_=0;
      // Thick smoke billows from the wreck; sparks and flare-ups thin out as power fades.
      const glm::vec3 back(std::sin(player_.yaw),0,std::cos(player_.yaw));
      smoke(player_.position+glm::vec3(0,1.f,0)+back*.3F,2,.32F,{.16F,.17F,.18F});
      if(random01()>player_.deathTime/1.4F) sparks(player_.position+glm::vec3(0,.95F,0),{0,1,0},{1,.66F,.28F},4,4.5F,1.2F);
      if(random01()>.55F+player_.deathTime*.3F) fireball(player_.position+glm::vec3((random01()-.5F)*.6F,.7F+random01()*.6F,(random01()-.5F)*.6F),{1,.5F,.15F},.16F,3.5F,.32F);
      // Short circuits crawl over the shell while the power drains.
      if(random01()>player_.deathTime/1.2F) {
        const float a=random01()*6.2831853F,h=random01();
        const glm::vec3 from=player_.position+glm::vec3(std::cos(a)*.3F,.6F+h*.9F,std::sin(a)*.3F);
        arc(from,from+glm::vec3(std::cos(a+1.4F)*.45F,(random01()-.5F)*.6F,std::sin(a+1.4F)*.45F),{.45F,.75F,1},.09F);
      }
    }
    // Little drag in flight; floor friction once the body is down, so it slides to a stop.
    player_.knockback*=std::exp(-(player_.grounded?(fall.fallAngle>1.F?7.F:4.F):.8F)*dt);
    moveAxis(0,player_.knockback.x*dt); moveAxis(2,player_.knockback.y*dt);
    player_.velocity.y-=gravity*dt; player_.grounded=false;
    moveAxis(1,player_.velocity.y*dt);
    if(player_.position.y < -5) { player_.position=spawnPoint(); player_.velocity={0,0,0}; }
    // Keep the pose blends live while the body flies and settles.
    player_.airBlend+=((player_.grounded?0.F:1.F)-player_.airBlend)*(1-std::exp(-12.F*dt));
    player_.hitBlend*=std::exp(-6.F*dt);
    if(respawnPending_&&player_.deathTime>=respawnDelay) respawn();
    updateParticles(dt); return;
  }
  elapsed_ += dt;
  if (timed()) remaining_ = std::max(0.0F, remaining_ - dt);
  messageTime_ = std::max(0.0F, messageTime_ - dt);
  player_.hurtCooldown = std::max(0.0F, player_.hurtCooldown - dt);
  sourceImmunity_ = std::max(0.0F, sourceImmunity_ - dt); graceTime_ = std::max(0.0F, graceTime_ - dt);
  slideCooldown_ = std::max(0.0F, slideCooldown_ - dt);
  slideSurge_ = player_.sliding ? std::max(0.0F, slideSurge_ - dt) : 0.0F;
  player_.motion.surge = slideSurge_ > 0 ? slideSurge_ / slideSurgeLength_ : 0.0F;
  bounceTime_ = std::max(0.0F, bounceTime_ - dt); bounceCooldown_ = std::max(0.0F, bounceCooldown_ - dt);
  wallTouch_ = std::max(0.0F, wallTouch_ - dt); wallKick_ = std::max(0.0F, wallKick_ - dt);
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
  // C toggles crouch; sprinting on the ground cancels it. Crouch starts on the ground, and
  // standing back up waits for headroom, so letting go inside a tunnel stands up at the exit.
  // Crouching at sprint speed slides instead (also on landing, if crouch was pressed in the
  // air); pressing crouch again mid-slide stands up and keeps the speed.
  const bool wasCrouching=player_.crouching;
  // Jetpack: with a charge, a double tap of sprint while moving on the ground fires one burst (the
  // first tap acts as a normal sprint press); mid-slide a single tap fires it, into a turbo slide.
  // The firing tap is taken by the jet, so a slide carries on instead of standing up.
  bool jetFired=false;
  if(input.sprintPressed) {
    const bool ready=player_.jets>0&&controlled&&player_.grounded&&player_.boost==Boost::None&&boostTime_<=0;
    const bool second=elapsed_-lastSprintTap_<=doubleTap;
    if(ready&&(player_.sliding||(second&&glm::length(player_.locomotion)>1.F))) { jetBurst(); jetFired=true; }
    lastSprintTap_=jetFired?-10.F:elapsed_;
  }
  const bool sprintEdge=input.sprintPressed&&!jetFired;
  // Boost transitions: sprint from a stride takes off into a flight; crouch from a flight dives
  // into a slide that keeps its speed (handled by startSlide); a hit ends either.
  // An interaction pauses a flight (it lifts off again afterwards); a hit just knocks it down.
  // Either way the flight clock keeps running.
  if(player_.boost==Boost::Flight&&busy) resumeFlight_=true;
  if(player_.boost!=Boost::None&&!controlled) endBoost(false);
  if(sprintEdge&&player_.boost==Boost::Stride&&controlled&&player_.grounded&&!player_.crouching) {
    const float v=glm::length(player_.locomotion);
    startFlight(v>.1F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw)),std::max(v+3.F,flightTakeoff),boostTime_<=0);
    startTrick(3);
  }
  // Sprint pressed mid-slide cancels it straight into a run (it stands only with headroom).
  if(sprintEdge&&player_.sliding&&controlled) crouchToggle_=false;
  if(input.crouchPressed&&controlled) crouchToggle_=player_.sliding?false:!crouchToggle_;
  if((input.crouchHeld||crouchToggle_)&&controlled&&player_.grounded&&!player_.sliding&&glm::length(player_.locomotion)>slideStartSpeed) startSlide();
  if(input.sprintHeld&&glm::length(movement)>.05F&&player_.grounded&&!player_.sliding) crouchToggle_=false;
  if((input.crouchHeld||crouchToggle_)&&controlled) { if(player_.grounded) player_.crouching=true; }
  else if(player_.crouching) standUp();
  if(!player_.crouching||!controlled||!player_.grounded) player_.sliding=false;
  // Standing out of a slide pops straight into the stride.
  if(wasCrouching&&!player_.crouching&&player_.slideBlend>.25F&&player_.grounded&&!input.jumpPressed&&player_.jumpBuffer<=0) popUp(sprintEdge);
  // While the flight clock runs, the end of an interaction or a fresh sprint press takes off
  // again (with the take-off move) without restarting the clock.
  if(boostTime_<=0) resumeFlight_=false;
  if(player_.boost==Boost::None&&boostTime_>0&&controlled&&player_.grounded&&!player_.crouching&&(resumeFlight_||sprintEdge)) {
    const float v=glm::length(player_.locomotion);
    const glm::vec2 heading=glm::length(movement)>.05F?glm::normalize(movement):v>.5F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw));
    startFlight(heading,std::max(v,flightTakeoff),false); resumeFlight_=false;
    startTrick(wasCrouching?2:3);
  }
  // Still boosted on foot: walking breaks back into the boosted stride.
  if(player_.boost==Boost::None&&boostTime_>0&&controlled&&player_.grounded&&!player_.crouching&&!input.sprintHeld&&glm::length(movement)>.05F&&glm::length(player_.locomotion)>speed+.5F) player_.boost=Boost::Stride;
  player_.sprinting=input.sprintHeld&&controlled&&!player_.crouching&&glm::length(movement)>.05F;
  const bool intent=controlled&&(glm::length(movement)>.05F||player_.sliding);
  {
    // The backpack light follows the sprint key (soft) and the pad boost (full).
    const float glow=boostMeter()>0?1.F:input.sprintHeld&&controlled?.55F:0.F;
    player_.backGlow+=(glow-player_.backGlow)*(1.F-std::exp(-(glow>player_.backGlow?14.F:8.F)*dt));
  }
  auto& animation=player_.motion;
  if(intent&&!animation.movingIntent&&player_.grounded) {
    animation.startAge=0;
    // Push-off: dust kicked back against the direction of travel.
    const glm::vec3 back(-movement.x,0,-movement.y);
    if(!player_.crouching) dust(player_.position+back*.3F+glm::vec3(0,.04F,0),player_.sprinting?7:4,1.5F,.13F);
  }
  if(!intent&&animation.movingIntent&&player_.grounded) {
    animation.stopAge=0; animation.stopStrength=std::clamp(animation.lastSpeed/sprintSpeed,0.F,1.F);
  }
  animation.movingIntent=intent;
  const float moveSpeed=player_.crouching?crouchSpeed:player_.sprinting?sprintSpeed:speed;
  auto targetVelocity=controlled?movement*moveSpeed:glm::vec2(0);
  // Momentum: speed above the run speed (slide jumps, speed pads, slide exits) carries and bleeds
  // off slowly while the stick points ahead. On the ground only boosted speed carries, and
  // pulling against it brakes as usual.
  const float current=glm::length(player_.locomotion);
  // A flight runs on its clock, not on speed or heading.
  if(boostTime_>0) { boostTime_=std::max(0.F,boostTime_-dt); if(boostTime_<=0&&player_.boost==Boost::Flight) endBoost(true); }
  if(player_.boost==Boost::Stride&&current<=speed+.15F) endBoost(false);
  if(current<=moveSpeed+.05F||player_.crouching) carrying_=false;
  if(boostTime_>0&&current>moveSpeed&&!player_.crouching) carrying_=true;
  if(controlled&&current>moveSpeed&&(carrying_||!player_.grounded)&&player_.boost!=Boost::Flight&&wallKick_<=0) {
    const bool steering=glm::length(movement)>.05F;
    const glm::vec2 along=player_.locomotion/current,want=steering?glm::normalize(movement):along;
    const float drag=!player_.grounded?airCarryDrag:boostTime_>0?boostHoldDrag:player_.boost==Boost::Stride?strideDrag:steering?groundCarryDrag:coastDrag;
    // Letting go ends a stride like a walk; a flight coasts.
    if(!steering&&player_.boost==Boost::Stride) { carrying_=false; endBoost(false); }
    else if(glm::dot(want,along)>.3F&&(steering||carrying_)) targetVelocity=want*(current-drag*dt);
    else { carrying_=false; if(player_.boost!=Boost::None) endBoost(player_.boost==Boost::Flight); }
  }
  if(player_.sliding) {
    // Sliding: friction and drag bleed the speed; input bends the line a little and pulling
    // back brakes. The slide ends at crouch-walk speed (or at once against a wall).
    float v=glm::length(player_.locomotion);
    glm::vec2 dir=v>.01F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw));
    float friction=(slideFriction+slideDrag*v)*(slideSurge_>0||boostTime_>0?surgeFriction:1.F);
    if(glm::length(movement)>.05F) {
      const auto want=glm::normalize(movement);
      const float dot=glm::dot(dir,want),angle=std::clamp(std::atan2(dir.x*want.y-dir.y*want.x,dot),-slideSteer*dt,slideSteer*dt);
      if(dot<-.3F) friction+=6.F;
      else dir={dir.x*std::cos(angle)-dir.y*std::sin(angle),dir.x*std::sin(angle)+dir.y*std::cos(angle)};
    }
    v=std::max(0.F,v-friction*dt);
    player_.locomotion=dir*v;
    if(v<slideEndSpeed) player_.sliding=false;
  } else if(player_.boost==Boost::Flight) {
    // Flying runs on its clock (boostTime_), not on heading: the stick turns the line in an arc,
    // and pulling more than 60 degrees off it air-brakes (down to a hover crawl, turning quicker
    // as it slows) so even a reversal stays tight; the speed then builds back along the new line.
    // With no input it holds its line. Launched faster than the pad speed, it bleeds down.
    float v=glm::length(player_.locomotion);
    glm::vec2 dir=v>.01F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw));
    if(glm::length(movement)>.05F) {
      const auto want=glm::normalize(movement);
      // Slower flight turns quicker, so an air-braked reversal stays tight.
      const float rate=flightTurn*std::clamp(padSpeed/std::max(v,1.F),1.F,2.2F);
      const float angle=std::clamp(std::atan2(dir.x*want.y-dir.y*want.x,glm::dot(dir,want)),-rate*dt,rate*dt);
      const float align=glm::dot(dir,want);
      dir={dir.x*std::cos(angle)-dir.y*std::sin(angle),dir.x*std::sin(angle)+dir.y*std::cos(angle)};
      if(align<.5F) v=std::max(5.F,v-30.F*(.5F-align)*dt);
      else if(v<padSpeed) v=std::min(padSpeed,v+12.F*align*dt);
    }
    if(v>padSpeed) v=std::max(padSpeed,v-flightDrag*dt);
    player_.locomotion=dir*v;
  } else if(wallKick_>0) {
    // Kicking off a wall: the stick waits a beat so the kick is not steered straight back.
  } else if(bounceTime_>0) {
    // Rebounding: the stick waits a beat while the rebound eases off.
    player_.locomotion*=std::exp(-3.F*dt);
  } else {
    const auto difference=targetVelocity-player_.locomotion;
    const float distance=glm::length(difference);
    const bool reversing=glm::dot(targetVelocity,player_.locomotion)<0;
    // A stride steers a little wider than a walk.
    const float acceleration=player_.boost==Boost::Stride?40.F:
      player_.grounded?(busy?110.F:glm::length(movement)<.05F?90.F:reversing?110.F:65.F):24.F;
    if(distance>0) player_.locomotion+=difference*std::min(1.F,acceleration*dt/distance);
  }
  player_.velocity.x=player_.locomotion.x+player_.knockback.x;
  player_.velocity.z=player_.locomotion.y+player_.knockback.y;
  float turn=0;
  // A slide faces along its own line, not the stick.
  // A rebound turns the courier with it.
  const bool coasting=player_.sliding||bounceTime_>0||wallKick_>0||player_.boost==Boost::Flight;
  const glm::vec2 heading=coasting&&glm::length(player_.locomotion)>.1F?player_.locomotion:movement;
  if ((glm::length(movement) > .05F || coasting) && controlled) {
    const float target=std::atan2(-heading.x,-heading.y);
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
  if(player_.grounded) { groundY_=player_.position.y; wallJumps_=0; }
  const bool boosted=boostTime_>0||player_.boost!=Boost::None;
  // Boosted, a jump just after meeting a wall kicks off it (on the ground or in the air).
  if(player_.jumpBuffer>0&&wallTouch_>0&&boosted&&controlled&&!player_.crouching&&wallJumps_<wallJumpsPerAir) wallJump();
  wallActionWait_=std::max(0.F,wallActionWait_-dt);
  if(input.wallPressed&&controlled&&wallActionWait_<=0) wallAction();
  if(wallQueue_>0) {
    // A pressed wall action on its way: fires as soon as that wall is within reach.
    wallQueue_=controlled?std::max(0.F,wallQueue_-dt):0.F;
    glm::vec2 n; glm::vec3 point; float distance=0;
    if(wallQueue_>0&&findWall(wallHeading(),wallReach,n,point,distance)&&glm::dot(n,wallQueueNormal_)>.9F) wallActionAt(n,point);
  }
  // A jump from a crouch first stands up; under a low ceiling it is refused.
  const bool fromSlide=player_.sliding||(player_.crouching&&player_.slideBlend>.4F);
  if(player_.jumpBuffer>0&&player_.coyoteTime>0&&controlled&&player_.crouching) { crouchToggle_=false; standUp(); }
  if(player_.jumpBuffer>0&&player_.coyoteTime>0&&controlled&&!player_.crouching) {
    const bool sprung=player_.spring>0;
    player_.velocity.y=jumpSpeed*(sprung?springJump:1.F); player_.grounded=false;
    if(sprung) {
      // Spring boots: a green coil-burst under the boots and rings snapping outward.
      const auto floor=player_.position+glm::vec3(0,.05F,0);
      shockwave(floor,{.45F,1,.6F},1.6F,.3F); shockwave(floor,{.8F,1,.5F},.9F,.2F);
      sparks(floor,{0,1,0},{.55F,1,.6F},12,6.F,1.1F);
      for(bool right:{false,true}) if(auto* p=spawn(ParticleKind::Glow,footPosition(right)+glm::vec3(0,.05F,0),{0,-1.F,0},{.5F,1,.6F},.3F,.3F)) p->drag=5;
    }
    // A boost survives the jump: a stride or flight carries on through the air and after landing,
    // and pad speed jumped out of a slide keeps carrying once the courier lands.
    if(glm::length(player_.locomotion)>sprintSpeed+.3F) carrying_=true;
    // Out of a slide the jump is a scissor-kick twist (a corkscrew when boosted); any other
    // boosted jump is a front flip.
    if(fromSlide) startTrick(boosted?7:6);
    else if(boosted) startTrick(2);
    player_.jumpBuffer=player_.coyoteTime=0;
    player_.jumpActive=true; player_.jumpAge=0; player_.jumpCutQueued=false;
    player_.motion.launchAge=0;
    // Boot jets: blue flame tongues under each foot, a ring and a dust kick.
    dust(player_.position+glm::vec3(0,.04F,0),9,1.8F,.13F);
    for(bool right:{false,true}) {
      const auto foot=footPosition(right)+glm::vec3(0,.05F,0);
      if(auto* p=spawn(ParticleKind::Glow,foot,{0,-1.2F,0},{.40F,.82F,1},.24F,.34F)) p->drag=6;
      if(auto* p=spawn(ParticleKind::Fireball,foot,{0,-2.2F,0},{.30F,.70F,1},.22F,.10F)) { p->growth=4.F; p->drag=5; }
    }
    sparks(player_.position+glm::vec3(0,.05F,0),{0,-.2F,0},{.45F,.85F,1},8,4.F,1.6F);
    shockwave(player_.position+glm::vec3(0,.03F,0),{.45F,.85F,1},1.1F,.3F);
    // Push-off blast: a small shell and light rays from the boots sell the leg drive.
    if(auto* p=spawn(ParticleKind::Shell,player_.position+glm::vec3(0,.08F,0),{},{.55F,.88F,1},.2F,.1F)) p->growth=.9F;
    for(int i=0;i<6;++i) { const float a=float(i)*1.0472F+random01()*.5F; if(auto* p=spawn(ParticleKind::Ray,player_.position+glm::vec3(0,.05F,0),{},{.75F,.92F,1},.16F,.04F)) p->extent=glm::normalize(glm::vec3(std::cos(a),-.05F,std::sin(a)))*1.3F; }
  }
  if(player_.jumpActive) {
    player_.jumpAge+=dt;
    if(!input.jumpHeld) player_.jumpCutQueued=true;
    if(player_.jumpCutQueued&&player_.jumpAge>=.055F) player_.velocity.y=std::min(player_.velocity.y,5.8F*(player_.spring>0?springJump:1.F));
  }
  player_.velocity.y-=(player_.velocity.y>0?gravity:fallGravity)*dt;
  moveAxis(0,player_.velocity.x*dt); moveAxis(2,player_.velocity.z*dt);
  player_.grounded=false; moveAxis(1,player_.velocity.y*dt);
  updateDevices(dt);
  updatePickups(dt);
  updatePortals(dt);

  const Bounds body=playerBounds();
  for(std::size_t i=0;i<traps_.size();++i)
    if(trapPhase(i)==TrapPhase::Active&&player_.boost!=Boost::Flight&&traps_[i].touchesFeet(player_.position,footRadius))
      applyDamage((traps_[i].area.min+traps_[i].area.max)*.5F,int(i),true,DamageKind::Electric);
  updateAnimation(dt,previousPosition,turn);
  shockEffects(dt);
  slideEffects(dt);
  speedTrail(dt);
  boostEffects(dt);
  // While a jet burst's boost runs: yellow sparks and motes streaming back off the pack.
  if(boostTime_<=0||state_!=RunState::Playing) jetting_=false;
  if(jetting_) {
    jetTrailClock_+=dt;
    const float v=glm::length(player_.locomotion);
    const glm::vec2 dir=v>.1F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw));
    const glm::vec3 ahead(dir.x,0,dir.y),pack=player_.position+glm::vec3(0,.9F,0)-ahead*.35F;
    while(jetTrailClock_>.025F) {
      jetTrailClock_-=.025F;
      sparks(pack,-ahead+glm::vec3(0,-.3F,0),{1,.85F,.3F},2,7.F,.5F);
      if(auto* p=spawn(ParticleKind::Glow,pack+glm::vec3(random01()-.5F,random01()*.6F-.3F,random01()-.5F)*.5F,-ahead*(3.F+3.F*random01()),{1,.9F,.35F},.3F,.06F)) p->drag=3;
    }
  } else jetTrailClock_=0;
  {
    // Skids: braking hard or pivoting at speed scrapes dust (and sparks at sprint speed) from both boots.
    const auto& m=player_.motion;
    const bool skid=player_.grounded&&!player_.crouching&&((m.stopAge<.22F&&m.stopStrength>.4F)||(m.pivotAge<.25F&&m.lastSpeed>3.F));
    skidClock_=skid?skidClock_+dt:0;
    if(skid&&skidClock_>.03F) {
      skidClock_=0;
      for(bool right:{false,true}) dust(footPosition(right)+glm::vec3(0,.03F,0),1,.9F,.11F);
      if(m.stopStrength>.8F||m.lastSpeed>6.F) sparks(footPosition(random01()>.5F)+glm::vec3(0,.04F,0),{0,.6F,0},{1,.75F,.4F},2,3.F,1.2F);
    }
  }
  if(player_.sprinting&&player_.grounded) {
    exhaustClock_+=dt;
    if(exhaustClock_>.045F) {
      exhaustClock_=0;
      const glm::vec3 back(std::sin(player_.yaw),0,std::cos(player_.yaw));
      const float x=random01(),y=random01(),z=random01();
      const glm::vec3 jitter(x-.5F,y-.5F,z-.5F);
      if(auto* p=spawn(ParticleKind::Glow,player_.position+glm::vec3(0,1.02F,0)+back*.42F+jitter*.08F,back*1.6F+glm::vec3(0,.35F,0),{.35F,.80F,1},.32F,.15F)) { p->drag=3; p->gravity=-.5F; }
      if(auto* p=spawn(ParticleKind::Fireball,player_.position+glm::vec3(0,1.0F,0)+back*.45F,back*2.2F,{.25F,.65F,1},.2F,.06F)) { p->growth=3.F; p->drag=4; }
      // Exhaust: a grey puff rolls out behind the flame and hangs in the air.
      if(random01()<.6F) smokePuff(player_.position+glm::vec3(0,.95F,0)+back*.5F,back*1.8F+glm::vec3(0,.25F,0),.08F,.55F);
    }
  }
  {
    // Running dust: kicked off the heels and left hanging behind, heavier at a sprint.
    const float pace=glm::length(player_.locomotion);
    if(player_.grounded&&!player_.crouching&&pace>speed*.7F) {
      runDustClock_+=dt;
      if(runDustClock_>(player_.sprinting?.03F:.07F)) {
        runDustClock_=0;
        const glm::vec3 back(std::sin(player_.yaw),0,std::cos(player_.yaw));
        const auto heel=footPosition(random01()<.5F)+glm::vec3(0,.04F,0)+back*.1F;
        if(auto* p=spawn(ParticleKind::Dust,heel,back*(1.F+1.5F*random01())+glm::vec3(0,.35F+.4F*random01(),0),glm::mix(glm::vec3(.30F,.29F,.28F),glm::vec3(.46F,.45F,.43F),random01()),.55F+.3F*random01(),player_.sprinting?.19F:.12F)) {
          p->drag=3.F; p->gravity=-.25F; p->growth=2.2F; p->turbulence=1.6F; p->extent.x=.6F;  // heavy
        }
      }
    } else runDustClock_=0;
  }
  if (player_.position.y < -5) {
    player_.position = spawnPoint();
    player_.velocity = glm::vec3(0);
    player_.grounded = true;
    applyDamage(player_.position-glm::vec3(0,0,1),200,false);
  }

  if (gateUnlocked()) gateOpen_ = std::min(1.0F, gateOpen_ + dt * 1.4F);
  updateTutorial(dt);
  const auto gate=gatePosition();
  if ((timed() && remaining_ <= 0) || player_.health<=0) {
    beginDeath();
  } else if (gateOpen_ > .95F && body.intersects({gate+glm::vec3(-1.1F, 0, -2.5F), gate+glm::vec3(1.1F, 2.2F, -.9F)})) {
    state_ = RunState::Won; player_.motion.victorious=true; player_.motion.victoryAge=0;
    notify(tutorial() ? "Training complete." : "Station restored. Relay complete.");
  }

  sparkClock_ += dt;
  if (sparkClock_ >= .12F) {
    sparkClock_ = 0;
    for (std::size_t i = 0; i < traps_.size(); ++i) {
      const auto& b = traps_[i].area;
      const float x = glm::mix(b.min.x, b.max.x, random01()), z = glm::mix(b.min.z, b.max.z, random01());
      const auto phase = trapPhase(i);
      if (phase == TrapPhase::Active) {
        // Live plates: arcs leap between points and up into the air, throwing sparks and a hot flash.
        for (int k = 0; k < 2; ++k) {
          const float a = random01() * 6.2831853F, length = .6F + .9F * random01(), up = random01();
          arc({x, 1.12F, z}, {x + std::cos(a) * length, 1.12F + .5F * up * up, z + std::sin(a) * length}, {1, .42F, .16F}, .12F);
        }
        sparks({x, 1.11F, z}, {0, 1, 0}, {1, .55F, .22F}, 6, 4.8F, .9F);
        flash({x, 1.12F, z}, {1, .32F, .12F}, .5F, .12F);
      } else if (phase == TrapPhase::Warning && random01() < .45F) {
        // Warning: the plates fizz before they go live.
        embers({x, 1.12F, z}, {1, .62F, .2F}, 2, .8F);
      }
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
  player_.crouchBlend=blend(player_.crouchBlend,player_.crouching?1.F:0.F,14.F);
  player_.slideBlend=blend(player_.slideBlend,player_.sliding?1.F:0.F,player_.sliding?22.F:player_.crouching?9.F:15.F);
  player_.strideBlend=blend(player_.strideBlend,player_.boost==Boost::Stride?1.F:0.F,10.F);
  player_.flightBlend=blend(player_.flightBlend,player_.boost==Boost::Flight?1.F:0.F,player_.boost==Boost::Flight?9.F:12.F);
  auto& a=player_.motion;
  for(float* age:{&a.startAge,&a.stopAge,&a.pivotAge,&a.launchAge,&a.landAge,&a.celebrateAge,&a.stepAge,&a.hitAge,&a.shockAge,&a.spawnAge,&a.slideAge,&a.popAge,&a.boostAge,&a.bounceAge,&a.takeoffAge,&a.touchAge,&a.pickupAge,&a.shieldBreakAge,&a.trickAge,&a.hackAge,&a.jetAge,&a.teleportAge,&a.pushAge}) *age=std::min(10.F,*age+dt);
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
  // The feet do not step while sliding or flying.
  if(player_.grounded&&!player_.sliding&&player_.boost!=Boost::Flight) {
    const int oldStep=int(player_.walkPhase/3.14159265F);
    // A stance foot travels backwards at root speed; swing returns it quickly.
    // A stride takes longer walking steps (see gait() in CourierPose.cpp, which must match).
    player_.walkPhase+=travelled*glm::mix(glm::mix(3.8F,2.6F,player_.sprintBlend),3.F,player_.strideBlend)/modelScale;
    const int step=int(player_.walkPhase/3.14159265F);
    // Even half-cycles plant the left foot, odd ones the right.
    // Crouched steps are quiet: no dust.
    if(step!=oldStep&&motion>.4F&&player_.hitStun<=0) { a.stepAge=0; a.stepStrength=(player_.sprinting?1.F:.55F)*(player_.crouching?.3F:1.F); a.stepSide=step%2==1?1.F:-1.F; }
    if(step!=oldStep&&motion>.4F&&player_.hitStun<=0&&!player_.crouching) {
      const auto foot=footPosition(step%2==1)+glm::vec3(0,.03F,0);
      dust(foot,player_.sprinting?5:2,player_.sprinting?1.5F:.7F,player_.sprinting?.14F:.10F);
      if(player_.sprinting) burst(foot,{.40F,.42F,.43F},2,1.1F);
      // Striding steps flash: a blue footprint glow, sparks and a little ring.
      if(player_.boost==Boost::Stride) {
        if(auto* p=spawn(ParticleKind::Glow,foot+glm::vec3(0,.05F,0),{0,.6F,0},{.40F,.85F,1},.3F,.2F)) p->drag=4;
        sparks(foot,{0,.6F,0},{.55F,.9F,1},4,3.F,1.3F); shockwave(foot,{.45F,.85F,1},.6F,.2F);
      }
    }
  }
}

void Game::updateParticles(float dt) {
  // Solid bits (debris, sparks, embers) bounce off voxels axis by axis with restitution and
  // friction, so they ricochet off walls and settle on block tops, not just the floor plane.
  const auto solidAt = [&](glm::vec3 q) { return world_.cell(int(std::floor(q.x)), int(std::floor(q.y)), int(std::floor(q.z))) != 0; };
  for (auto& p : particles_) if (p.remaining > 0) {
    p.remaining = std::max(0.0F, p.remaining - dt);
    p.velocity *= std::exp(-p.drag * dt);
    p.velocity.y -= p.gravity * dt;
    if (p.turbulence > 0) {
      // Cheap swirl: each particle follows its own slowly turning gust.
      const float t = p.lifetime - p.remaining, k = p.seed * 40.F;
      p.velocity += glm::vec3(std::sin(t * 3.1F + k), .4F * std::sin(t * 2.3F + k * 1.7F), std::cos(t * 2.7F + k * 1.3F)) * p.turbulence * dt;
    }
    const glm::vec3 before = p.position;
    p.position += p.velocity * dt;
    const bool solid = p.kind == ParticleKind::Debris || p.kind == ParticleKind::Spark || p.kind == ParticleKind::Ember;
    if (p.kind == ParticleKind::Dust && solidAt(p.position)) { p.position.y = std::floor(p.position.y) + 1.02F; p.velocity.y = std::abs(p.velocity.y) * .25F; }
    if (!solid || !solidAt(p.position)) continue;
    const float impact = -p.velocity.y;
    const float restitution = p.kind == ParticleKind::Debris ? (p.growth > 0 ? .15F : .38F) : .45F;
    const float friction = p.kind == ParticleKind::Spark ? .55F : .68F;
    int hitAxis = -1;
    for (int axis : {1, 0, 2}) {
      glm::vec3 test = p.position; test[axis] = before[axis];
      if (!solidAt(test)) { hitAxis = axis; break; }
    }
    if (hitAxis < 0) { p.position = before; p.velocity *= -.3F; continue; }
    p.position[hitAxis] = before[hitAxis];
    p.velocity[hitAxis] *= -restitution;
    for (int axis = 0; axis < 3; ++axis) if (axis != hitAxis) p.velocity[axis] *= friction;
    if (hitAxis != 1 || p.velocity.y < 0) continue;
    // A hard spark landing splits into a lingering ember.
    if (p.kind == ParticleKind::Spark && !p.bounced && impact > 1.5F) {
      p.bounced = true;
      if (auto* e = spawn(ParticleKind::Ember, p.position, glm::vec3(p.velocity.x, .6F, p.velocity.z) * .5F, p.colour, .3F + .2F * p.seed, .03F)) { e->gravity = 2.5F; e->drag = 2.5F; e->turbulence = 2; }
    }
    // Early, hard debris impacts kick up a little dust (kept inside the one-second budget).
    if (p.kind == ParticleKind::Debris && p.growth == 0 && !p.bounced && impact > 2.F && p.remaining > p.lifetime - .5F) {
      p.bounced = true;
      if (auto* d = spawn(ParticleKind::Dust, p.position, {0, .4F, 0}, {.62F, .64F, .65F}, .3F, .07F)) { d->drag = 4; d->growth = 2.5F; }
    }
  }
}
void Game::present(float dt, float wake) {
  // Beats of the arrival scene; CourierPose holds the matching body poses.
  const auto at = [&](float t) { return lastWake_ < t && wake >= t; };
  const glm::vec3 back(std::sin(player_.yaw), 0, std::cos(player_.yaw)), feet = player_.position + glm::vec3(0, .04F, 0);
  if (at(.62F)) flash(player_.position + glm::vec3(0, 1.38F, 0) - back * .42F, {.45F, .95F, 1}, .55F, .22F);
  if (at(.80F)) for (int i = 0; i < 7; ++i) {
    const float x = random01(), y = random01(), z = random01(), s = random01();
    const glm::vec3 v = back * (1.2F + s) + glm::vec3(x - .5F, .9F + y, z - .5F);
    if (auto* p = spawn(ParticleKind::Dust, player_.position + glm::vec3(0, 1.1F, 0) + back * .4F, v, {.93F, .95F, .96F}, .75F, .12F)) { p->drag = 2.5F; p->gravity = -.6F; p->growth = 2.2F; }
  }
  if (at(1.72F)) { dust(feet, 9, 1.5F, .13F); shockwave(feet, {.86F, .90F, .92F}, 1.2F, .4F); }
  if (at(2.62F)) for (int i = 0; i < 8; ++i) {
    const float a = random01() * 6.2831853F, h = random01();
    const glm::vec3 out(std::cos(a), 0, std::sin(a));
    if (auto* p = spawn(ParticleKind::Dust, player_.position + glm::vec3(0, .8F + h * .6F, 0) + out * .3F, out * 1.8F + glm::vec3(0, .4F, 0), {.84F, .86F, .86F}, .55F, .09F)) { p->drag = 4; p->gravity = .8F; p->growth = 1.5F; }
  }
  if (at(3.12F)) { shockwave(feet, {.45F, .90F, 1}, 1.5F, .45F); flash(player_.position + glm::vec3(0, 2.02F, 0), {.45F, .95F, 1}, .35F, .25F); }
  lastWake_ = wake;
  updateParticles(dt);
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
int Game::nearestCheckpoint() const {
  float closest = 1.8F;
  int result = -1;
  for (std::size_t i = 0; i < checkpoints_.size(); ++i) {
    if (checkpoints_[i].active) continue;
    const auto delta = player_.position - checkpoints_[i].position;
    const float distance = glm::length(glm::vec2(delta.x, delta.z));
    if (distance < closest && std::abs(delta.y) < 1.0F) { closest = distance; result = int(i); }
  }
  return result;
}
void Game::activateCheckpoint(std::size_t index) {
  auto& checkpoint = checkpoints_[index];
  if (checkpoint.active) return;
  checkpoint.active = true; checkpoint.activeAge = 0; respawnIndex_ = int(index);
  // Boot-up: flash and burst at the lamp, a pillar of light, two floor rings and motes
  // spiralling up the beam; the courier fist-pumps.
  const auto lamp = checkpoint.position + glm::vec3(0, 1.2F, 0);
  flash(lamp, {.5F, 1, .92F}, 1.5F, .24F);
  pillar(checkpoint.position, {.35F, 1, .9F}, .9F);
  impactBurst(lamp, {.4F, 1, .9F}, 2.4F, 14, 22);
  shockwave(checkpoint.position + glm::vec3(0, .06F, 0), {.4F, 1, .9F}, 2.8F, .55F);
  shockwave(checkpoint.position + glm::vec3(0, .07F, 0), {1, 1, 1}, 1.5F, .3F);
  for (int i = 0; i < 16; ++i) {
    const float a = float(i) * .3927F;
    const glm::vec3 v(-std::sin(a) * 1.4F, 1.6F + .3F * float(i % 4), std::cos(a) * 1.4F);
    if (auto* p = spawn(ParticleKind::Glow, checkpoint.position + glm::vec3(std::cos(a) * .6F, .2F, std::sin(a) * .6F), v, i % 3 ? glm::vec3(.35F, 1, .9F) : glm::vec3(1), .8F, .08F)) { p->drag = 1.5F; p->gravity = -1.F; }
  }
  player_.motion.celebrateAge = 0;
  hitStop_ = std::max(hitStop_, .06F);
  notify("Checkpoint online. The courier will reboot here.");
}
glm::vec3 Game::rebootPosition() const {
  return respawnIndex_ >= 0 ? checkpoints_[std::size_t(respawnIndex_)].respawn : spawnPoint();
}
void Game::beamIn() {
  // A fresh body beside the checkpoint; the level (cores, relays, hazards, clock) carries on.
  const glm::vec3 at = rebootPosition();
  player_ = Player{};
  player_.position = at; player_.grounded = true;
  player_.motion.spawnAge = 0;
  interactionIndex_ = -1; interactionCommitted_ = false; crouchToggle_ = false; hitStop_ = 0;
  ++revision_;  // Snaps the follow camera to the new position.
  // Beam-in: pillar, flash, rings and a burst of light motes around the courier.
  pillar(at, {.35F, 1, .9F}, .8F);
  flash(at + glm::vec3(0, .8F, 0), {.6F, 1, .95F}, 1.7F, .28F);
  impactBurst(at + glm::vec3(0, .8F, 0), {.4F, 1, .9F}, 2.F, 12, 18);
  shockwave(at + glm::vec3(0, .05F, 0), {.4F, 1, .9F}, 2.2F, .45F);
  dust(at + glm::vec3(0, .04F, 0), 10, 1.8F, .14F);
}
void Game::respawn() {
  beamIn();
  player_.hurtCooldown = 1.F; graceTime_ = 1.F;  // A second of grace, shown as a blink.
  state_ = RunState::Playing; respawnPending_ = false;
  if (!timed()) { notify(respawnIndex_ >= 0 ? "Rebooted at checkpoint." : "Rebooted at the entry point."); return; }
  remaining_ = std::max(0.F, remaining_ - respawnPenalty);
  notify(std::string(respawnIndex_ >= 0 ? "Rebooted at checkpoint." : "Rebooted at the entry point.") + " -10 seconds. " +
         std::to_string(lives_) + (lives_ == 1 ? " life left." : " lives left."));
}
void Game::returnToCheckpoint() {
  if (state_ != RunState::Playing) return;
  const int health = player_.health;
  beamIn();
  player_.health = health;
  notify(respawnIndex_ >= 0 ? "Returned to checkpoint." : "Returned to the entry point.");
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
const char* Game::nameOf(int level) { return level == tutorialLevel ? "TRAINING GROUNDS" : level == practiceLevel ? "PRACTICE FIELD" : level == 0 ? "COURTYARD" : "RELAY CORRIDORS"; }
void Game::startSlide() {
  const float v=glm::length(player_.locomotion);
  const glm::vec2 dir=player_.locomotion/v;
  // The boost needs the cool-down, so tapping crouch cannot chain it.
  // A dive from a flight keeps its speed with no boost; a stride slides as usual.
  const bool fromFlight=player_.boost==Boost::Flight;
  if(fromFlight) startTrick(1);  // the dive is a barrel roll into the skid
  if(player_.boost!=Boost::None) endBoost(fromFlight);
  const bool boost=slideCooldown_<=0&&!fromFlight;
  player_.locomotion=dir*(boost?std::max(v,sprintSpeed)+slideBoost:v);
  if(boost) { slideCooldown_=slideBoostCooldown; slideSurge_=slideSurgeLength_=slideSurgeTime; }
  player_.motion.surge=boost?1.F:0.F;
  player_.sliding=player_.crouching=true; player_.sprinting=false;
  player_.motion.slideAge=0; slideClock_=0;
  // Drop-in: a dust wave thrown ahead, a scrape of sparks and a low ring; a boost adds a blue kick
  // of light and a billow of smoke off both heels.
  const glm::vec3 forward(dir.x,0,dir.y),feet=player_.position+glm::vec3(0,.04F,0),across(-forward.z,0,forward.x);
  dust(feet+forward*.3F,12,2.4F,.15F);
  sparks(feet+forward*.35F,forward*.5F+glm::vec3(0,.45F,0),{1,.72F,.38F},boost?14:6,6.F,1.3F);
  shockwave(feet,{.80F,.83F,.86F},1.3F,.3F);
  if(boost) {
    for(int i=0;i<10;++i) {
      const float spread=(random01()-.5F)*2.F;
      smokePuff(feet+glm::vec3(0,.12F,0)-forward*.2F,-forward*(2.F+2.5F*random01())+across*spread*1.6F+glm::vec3(0,.5F+.8F*random01(),0),.16F+.06F*random01(),.8F);
    }
    dust(feet-forward*.3F,8,2.F,.16F);
    flash(feet+glm::vec3(0,.35F,0),{.55F,.85F,1},.9F,.12F);
    if(auto* p=spawn(ParticleKind::Shell,feet+glm::vec3(0,.1F,0)-forward*.2F,{},{.55F,.88F,1},.22F,.1F)) p->growth=1.1F;
    for(int i=0;i<5;++i) { const float a=(float(i)-2.F)*.35F;
      if(auto* p=spawn(ParticleKind::Ray,feet+glm::vec3(0,.12F,0),{},{.75F,.92F,1},.16F,.04F)) p->extent=glm::normalize(glm::vec3(-forward.x*std::cos(a)+forward.z*std::sin(a),.1F,-forward.z*std::cos(a)-forward.x*std::sin(a)))*1.4F; }
    for(int i=0;i<14;++i) {
      const float a=(random01()-.5F)*2.2F;
      const glm::vec3 out(-forward.x*std::cos(a)+forward.z*std::sin(a),.15F+.35F*random01(),-forward.z*std::cos(a)-forward.x*std::sin(a));
      if(auto* p=spawn(ParticleKind::Glow,feet+glm::vec3(0,.15F,0),out*(5.F+4.F*random01()),i%2?glm::vec3(.72F,.5F,1):glm::vec3(.45F,.9F,1),.4F,.08F)) { p->drag=5; p->gravity=-.2F; }
    }
    for(bool rightFoot:{false,true}) if(auto* p=spawn(ParticleKind::Fireball,footPosition(rightFoot)+glm::vec3(0,.08F,0),-forward*3.F,{.45F,.7F,1},.22F,.08F)) { p->growth=4.F; p->drag=4; }
  }
}
void Game::slideEffects(float dt) {
  if(!player_.sliding||!player_.grounded) { slideClock_=0; return; }
  slideClock_+=dt;
  if(slideClock_<.03F) return;
  slideClock_=0;
  // Heel scrape: sparks spray back off the lead boot, dust boils from both feet and the
  // trailing hand, and scuffs streak the floor. All of it fades with the speed.
  const float v=glm::length(player_.locomotion),k=std::clamp((v-slideEndSpeed)/(sprintSpeed+slideBoost-slideEndSpeed),0.F,1.F);
  const glm::vec3 forward(-std::sin(player_.yaw),0,-std::cos(player_.yaw)),right(std::cos(player_.yaw),0,-std::sin(player_.yaw));
  const auto heel=footPosition(true)+glm::vec3(0,.04F,0),hand=player_.position-right*.42F*modelScale-forward*.1F+glm::vec3(0,.05F,0);
  if(random01()<.35F+.65F*k) sparks(heel,-forward+glm::vec3(0,.55F,0),{1,.70F,.35F},int(1+4*k),3.5F+3.F*k,.9F);
  dust(heel-forward*.15F,int(1+2*k),.6F+1.2F*k,.12F+.04F*k);
  dust(footPosition(false)+glm::vec3(0,.03F,0),1,.5F+.8F*k,.11F);
  dust(hand,1,.4F+.6F*k,.10F);
  if(k>.35F&&random01()<.5F) scorch(heel-glm::vec3(0,.03F,0),.30F+.2F*k,{.20F,.21F,.22F});
  // Smoke trail: grey puffs roll off the heels and swell behind, thicker with speed.
  if(random01()<.4F+.6F*k) smokePuff(heel+glm::vec3(0,.06F,0),-forward*(.8F+1.4F*k)+glm::vec3(0,.35F,0),.12F+.08F*k,.7F);
  // Surging (a boosted slide, or any slide while the pad boost runs): a thick smoke plume, heel
  // flames, rushing streaks and embers.
  if(slideSurge_>0||boostTime_>0) {
    const float surge=std::max(slideSurge_>0?slideSurge_/slideSurgeLength_:0.F,boostTime_/boostDuration);
    for(bool rightFoot:{false,true}) {
      const auto boot=footPosition(rightFoot)+glm::vec3(0,.08F,0)-forward*.2F;
      smokePuff(boot+glm::vec3(0,.02F,0),-forward*(2.F+2.F*surge)+right*(rightFoot?.5F:-.5F)+glm::vec3(0,.5F,0),.18F+.1F*surge,.9F);
      if(auto* p=spawn(ParticleKind::Fireball,boot,-forward*4.F+glm::vec3(0,.3F,0),rightFoot?glm::vec3(.6F,.45F,1):glm::vec3(.35F,.8F,1),.18F,.06F+.04F*surge)) { p->growth=3.F; p->drag=4; }
      if(auto* p=spawn(ParticleKind::Glow,boot,-forward*2.5F,{.9F,.96F,1},.14F,.1F)) p->drag=5;
    }
    for(int i=0;i<2;++i) {
      const glm::vec3 at=player_.position+forward*(.8F+1.4F*random01())+right*((random01()<.5F?-1.F:1.F)*(.5F+.9F*random01()))+glm::vec3(0,.15F+.9F*random01(),0);
      if(auto* p=spawn(ParticleKind::Ray,at,-forward*5.F,{.85F,.95F,1},.15F,.02F)) p->extent=-forward*(1.2F+1.5F*surge);
    }
    if(random01()<.5F*surge) embers(player_.position+glm::vec3(0,.3F,0)-forward*.5F,{.75F,.6F,1},1,1.5F);
  }
  // Pad speed: the smoke fans up higher, like a rooster tail of dust.
  if(v>padSpeed-.5F) for(bool rightFoot:{false,true}) {
    const auto boot=footPosition(rightFoot)+glm::vec3(0,.05F,0);
    smokePuff(boot,-forward*2.5F+right*(rightFoot?.9F:-.9F)+glm::vec3(0,1.6F,0),.15F,.8F);
    sparks(boot,-forward+glm::vec3(0,.9F,0)+right*(rightFoot?.3F:-.3F),rightFoot?glm::vec3(.72F,.5F,1):glm::vec3(.5F,.9F,1),3,8.F,.6F,1.2F);
  }
}
std::array<Bounds,2> BlastDoor::panels() const {
  const int axis=alongZ?2:0;
  const float shift=open*half[axis]*1.02F;
  Bounds a=bounds(),b=bounds();
  a.max[axis]=centre[axis]; b.min[axis]=centre[axis];
  a.min[axis]-=shift; a.max[axis]-=shift; b.min[axis]+=shift; b.max[axis]+=shift;
  return {a,b};
}
void Game::drainBite() {
  if(timed()) remaining_=std::max(0.F,remaining_-drainPenalty);
  penaltyAge_=0; player_.motion.hackAge=0;
  notify(timed() ? "Crimson tile: -5 seconds." : "Crimson tile: in a mission this costs 5 seconds.");
  // A crimson flare: a ring and red sparks at the feet, and dark-red motes torn up off the
  // courier like seconds draining away.
  const auto feet=player_.position+glm::vec3(0,.05F,0);
  shockwave(feet,{1,.15F,.2F},1.4F,.3F);
  sparks(feet,{0,1,0},{1,.25F,.25F},12,5.F,1.F);
  for(int i=0;i<10;++i) {
    const float a=float(i)*.628F;
    if(auto* p=spawn(ParticleKind::Glow,feet+glm::vec3(std::cos(a)*.4F,.1F,std::sin(a)*.4F),{std::cos(a)*.6F,2.4F,std::sin(a)*.6F},{.9F,.08F,.15F},.7F,.08F)) { p->drag=1.5F; p->gravity=-.4F; }
  }
}
void Game::popUp(bool sprintCancel) {
  // Spring out of a slide into the stride: a sprint cancel keeps at least sprint speed, extra
  // speed carries, and the lead (right) foot lands first so the legs flow out of the skid.
  const float v=glm::length(player_.locomotion);
  if(sprintCancel&&v>.1F) player_.locomotion*=std::max(v,sprintSpeed)/v;
  if(glm::length(player_.locomotion)>sprintSpeed) carrying_=true;
  player_.motion.popAge=0;
  startTrick(boostTime_>0||player_.boost!=Boost::None?5:4);  // a kip-up (spinning when boosted)
  player_.walkPhase=std::ceil(player_.walkPhase/(2*pi))*2*pi+pi-.15F;
  const glm::vec3 back(std::sin(player_.yaw),0,std::cos(player_.yaw)),feet=player_.position+glm::vec3(0,.04F,0);
  dust(feet+back*.3F,8,1.8F,.13F);
  for(bool right:{false,true}) if(auto* p=spawn(ParticleKind::Glow,footPosition(right)+glm::vec3(0,.05F,0),back*2.F,{.40F,.82F,1},.2F,.28F)) p->drag=6;
  if(sprintCancel) {
    shockwave(feet,{.45F,.85F,1},1.F,.25F);
    if(auto* p=spawn(ParticleKind::Shell,feet+glm::vec3(0,.3F,0),{},{.55F,.88F,1},.18F,.1F)) p->growth=.8F;
  }
}
void Game::updateDevices(float dt) {
  const glm::vec2 feet(player_.position.x,player_.position.z);
  // Crimson tiles: standing on one (on the floor, not flying over it) costs drainPenalty seconds
  // at once, then again every drainInterval while the courier stays on crimson.
  drainCooldown_=std::max(0.F,drainCooldown_-dt); penaltyAge_=std::min(10.F,penaltyAge_+dt);
  for(auto& tile:drains_) tile.flash=std::max(0.F,tile.flash-dt*2.F);
  if(state_==RunState::Playing&&player_.grounded&&player_.boost!=Boost::Flight&&player_.position.y<1.3F)
    for(auto& tile:drains_)
      if(feet.x>tile.area.min.x&&feet.x<tile.area.max.x&&feet.y>tile.area.min.z&&feet.y<tile.area.max.z) {
        if(drainCooldown_<=0) { drainCooldown_=drainInterval; tile.flash=1; drainBite(); }
        break;
      }
  // Speed pads: entering one launches the courier along its arrow (a slide keeps sliding, faster).
  for(auto& pad:pads_) {
    pad.flash=std::max(0.F,pad.flash-dt*2.5F);
    const bool on=player_.grounded&&feet.x>pad.area.min.x&&feet.x<pad.area.max.x&&feet.y>pad.area.min.z&&feet.y<pad.area.max.z&&player_.position.y<pad.area.max.y+.3F;
    // Crossing a strip against its arrow does nothing.
    const float along=glm::dot(player_.locomotion,pad.direction);
    if(on&&!pad.occupied&&player_.hitStun<=0&&along>-1.F) {
      // The launch depends on how the courier hits the strip: a slide just gets faster, a sprint
      // (or a flight) takes off into a low flight, a walk breaks into a fast stride.
      const glm::vec2 d=pad.direction;
      const glm::vec2 side=(player_.locomotion-d*along)*.4F;
      carrying_=true; pad.flash=1; player_.motion.boostAge=0;
      auto& m=player_.motion;
      const glm::vec3 ahead(d.x,0,d.y),floor=player_.position+glm::vec3(0,.05F,0);
      if(player_.sliding) {
        player_.locomotion=d*std::max(along+6.F,padSpeed+slidePadBonus)+side; m.boostKind=2;
        slideSurge_=slideSurgeLength_=padSurgeTime;  // turbo: the slide barely slows for a while
        boostTime_=boostDuration;
        // Slide burst: smoke rather than light. A billow fanned back and to the sides, a low smoke
        // ring rolling out, kicked-up dust and a scuff; only a soft grey ring for a flash.
        const glm::vec3 across(-ahead.z,0,ahead.x);
        for(int i=0;i<16;++i) smokePuff(floor+glm::vec3(0,.15F,0),-ahead*(2.5F+3.5F*random01())+across*(random01()-.5F)*5.F+glm::vec3(0,.6F+1.2F*random01(),0),.2F+.1F*random01(),1.F);
        for(int i=0;i<12;++i) { const float a=float(i)*.5236F; smokePuff(floor+glm::vec3(0,.08F,0),glm::vec3(std::cos(a),.1F,std::sin(a))*4.F,.16F,.8F); }
        dust(floor,14,3.F,.16F);
        scorch(floor-glm::vec3(0,.04F,0),.8F,{.20F,.21F,.22F});
        shockwave(floor,{.80F,.83F,.86F},1.8F,.3F);
        sparks(floor,-ahead+glm::vec3(0,.35F,0),{1,.85F,.55F},22,9.F,.9F); sparks(floor,-ahead+glm::vec3(0,.6F,0),{.55F,.9F,1},14,7.F,1.2F);
        if(auto* p=spawn(ParticleKind::Fireball,floor+glm::vec3(0,.2F,0)-ahead*.4F,-ahead*3.F,{.55F,.8F,1},.3F,.12F)) { p->growth=5.F; p->drag=4; }
      } else if(player_.boost==Boost::Flight||player_.sprinting) {
        const bool fresh=player_.boost!=Boost::Flight;
        startFlight(d,std::max(along+4.F,padSpeed)); player_.locomotion+=side; m.boostKind=1;
        if(fresh) startTrick(3);
      } else {
        player_.locomotion=d*std::max(along+2.5F,strideSpeed)+side; m.boostKind=0;
        player_.boost=Boost::Stride; m.popAge=0;  // The lunge into the stride.
        boostTime_=boostDuration;
        for(bool right:{false,true}) if(auto* p=spawn(ParticleKind::Glow,footPosition(right)+glm::vec3(0,.05F,0),-ahead*2.F,{.40F,.82F,1},.3F,.24F)) p->drag=5;
      }
      {
      // Launch: a flash, ring and shell, sparks and light rays blasting back off the strip.
      const glm::vec3 d3(d.x,0,d.y),base=player_.position+glm::vec3(0,.05F,0);
      flash(base+glm::vec3(0,.4F,0),{.45F,.85F,1},1.2F,.14F);
      shockwave(base,{.45F,.85F,1},1.5F,.3F);
      if(auto* p=spawn(ParticleKind::Shell,base+glm::vec3(0,.3F,0),{},{.55F,.88F,1},.22F,.1F)) p->growth=1.2F;
      sparks(base,-d3+glm::vec3(0,.5F,0),{.55F,.9F,1},12,7.F,1.1F);
      for(int i=0;i<7;++i) {
        const float a=(float(i)-3.F)*.22F;
        const glm::vec3 r(-d3.x*std::cos(a)+d3.z*std::sin(a),.08F,-d3.z*std::cos(a)-d3.x*std::sin(a));
        if(auto* p=spawn(ParticleKind::Ray,base+glm::vec3(0,.15F,0),{},{.75F,.92F,1},.18F,.04F)) p->extent=glm::normalize(r)*2.F;
      }
      dust(base,8,2.F,.13F);
      // Sonic boom: shells stacked ahead form a cone the courier punches through; sparks in cyan,
      // violet and pink; arcs crackle from the strip to the boots; speed lines stream past.
      const glm::vec3 heading(player_.locomotion.x,0,player_.locomotion.y);
      const glm::vec3 fwd=glm::length(heading)>.1F?glm::normalize(heading):d3,across(-fwd.z,0,fwd.x);
      const float power=m.boostKind==0?.6F:1.F;
      for(int i=0;i<3;++i) if(auto* p=spawn(ParticleKind::Shell,base+glm::vec3(0,.6F,0)+fwd*(.5F+.6F*float(i)),{},i==1?glm::vec3(.75F,.55F,1):glm::vec3(.55F,.88F,1),.16F+.05F*float(i),.08F)) p->growth=(.5F+.35F*float(i))*power;
      const glm::vec3 hues[3]{{.45F,.9F,1},{.72F,.5F,1},{1,.45F,.85F}};
      for(int i=0;i<3;++i) sparks(base+glm::vec3(0,.15F,0),-fwd+across*(float(i)-1.F)*.7F+glm::vec3(0,.5F,0),hues[i],int(8*power),7.5F,1.F,1.2F);
      for(int i=0;i<3;++i) arc(base+across*(random01()-.5F)*1.4F-fwd*.4F,footPosition(i%2==0)+glm::vec3(0,.1F,0),i==1?glm::vec3(.75F,.55F,1):glm::vec3(.5F,.9F,1),.14F);
      for(int i=0;i<int(14*power);++i) {
        const glm::vec3 at=player_.position+fwd*(.6F+1.6F*random01())+across*(random01()-.5F)*2.4F+glm::vec3(0,.2F+1.5F*random01(),0);
        if(auto* p=spawn(ParticleKind::Ray,at,{},random01()<.5F?glm::vec3(.85F,.95F,1):glm::vec3(.75F,.6F,1),.18F+.1F*random01(),.025F)) p->extent=-fwd*(2.F+1.5F*random01());
      }
      // Detonation: the launch bursts outward in every direction. A white-hot core flash and a
      // fireball bloom, an impact burst of rays, a sphere of glowing motes in all three hues that
      // blasts out then hangs on heavy drag, a flat ring of sparks skimming the floor, and confetti
      // shards. A beat of slow motion holds the bloom (shorter for a stride).
      {
        const glm::vec3 core=player_.position+glm::vec3(0,.7F,0);
        flash(core,{.9F,.95F,1},1.6F*power,.12F);
        fireball(core,{.55F,.6F,1},.3F*power,7.F,.32F);
        impactBurst(core,{.6F,.85F,1},2.6F*power,int(10*power),int(18*power));
        for(int i=0;i<int(36*power);++i) {
          const float u=random01()*2-1,a=random01()*6.2831853F,r=std::sqrt(1-u*u);
          const glm::vec3 out(std::cos(a)*r,std::abs(u)*.8F+.1F,std::sin(a)*r);
          if(auto* p=spawn(ParticleKind::Glow,core,out*(6.F+5.F*random01()),hues[i%3],.45F+.25F*random01(),.08F+.06F*random01())) { p->drag=5; p->gravity=-.3F; }
        }
        for(int i=0;i<int(20*power);++i) {
          const float a=float(i)*6.2831853F/float(int(20*power));
          sparks(base+glm::vec3(0,.08F,0),{std::cos(a),.08F,std::sin(a)},hues[i%3],1,9.F,.15F,1.4F);
        }
        burst(core,{.82F,.86F,1},int(6*power),3.F);
        shockwave(base,{.75F,.55F,1},2.6F*power,.35F);
      }
      }
      hitStop_=std::max(hitStop_,m.boostKind>0?.07F:.035F);
    }
    pad.occupied=on;
  }
  // Pressure plates hold their door open while pressed and for doorHold seconds afterwards.
  for(auto& plate:plates_) {
    auto& door=doors_[std::size_t(plate.door)];
    const bool down=player_.grounded&&glm::distance(feet,glm::vec2(plate.position.x,plate.position.z))<plateRadius&&player_.position.y<plate.position.y+.35F;
    if(down&&!plate.down) {
      const auto top=plate.position+glm::vec3(0,.08F,0);
      sparks(top,{0,1,0},{1,.75F,.35F},6,2.5F,1.2F); shockwave(top,{.35F,1,.9F},1.2F,.25F); dust(top,5,1.F,.1F);
      if(door.open<.05F) notify("Pressure plate down. Blast door open for 4 seconds.");
    }
    plate.down=down;
    plate.press=glm::mix(plate.press,down?1.F:0.F,1-std::exp(-18.F*dt));
    if(down) door.hold=doorHold+dt;
  }
  for(auto& door:doors_) {
    door.hold=std::max(0.F,door.hold-dt);
    // Never close on the courier.
    if(door.hold<=0&&door.open>0&&cylinderIntersectsBox(player_.position,playerRadius,bodyHeight(),door.bounds())) door.hold=.05F;
    const float before=door.open;
    door.open=std::clamp(door.open+(door.hold>0?2.6F:-2.2F)*dt,0.F,1.F);
    glm::vec3 along(0); along[door.alongZ?2:0]=1;
    if(before<=0&&door.open>0) {
      // Unlatch: steam vents from both jambs and the seam sparks.
      for(float side:{-1.F,1.F}) smoke(door.centre+along*(side*door.half[door.alongZ?2:0])+glm::vec3(0,.8F,0),3,.25F,{.85F,.88F,.9F});
      sparks(door.centre,{0,.3F,0},{1,.7F,.35F},6,3.F,1.4F);
    }
    if(before>0&&door.open<=0) {
      // Slam: dust along the sill and a few chips.
      const auto sill=glm::vec3(door.centre.x,1.05F,door.centre.z);
      dust(sill,8,1.6F,.14F); burst(sill+glm::vec3(0,.2F,0),{.55F,.57F,.58F},4,1.2F);
    }
  }
}
void Game::speedTrail(float dt) {
  // Above sprint speed the courier streaks: glow trails off the boots and backpack, plus light rays.
  const float v=glm::length(player_.locomotion),k=std::clamp((v-sprintSpeed-.5F)/4.F,0.F,1.F);
  if(k<=0) { trailClock_=0; return; }
  trailClock_+=dt;
  if(trailClock_<.02F) return;
  trailClock_=0;
  const glm::vec3 behind=-glm::vec3(player_.locomotion.x,0,player_.locomotion.y)/v;
  for(bool right:{false,true}) if(auto* p=spawn(ParticleKind::Glow,footPosition(right)+glm::vec3(0,.08F,0),behind*1.5F,{.40F,.82F,1},.22F,.06F+.16F*k)) p->drag=4;
  const float pack=modelScale*(1.F-.45F*player_.crouchBlend);
  if(auto* p=spawn(ParticleKind::Glow,player_.position+glm::vec3(0,pack,0)+behind*.35F,behind*2.F,{.55F,.9F,1},.25F,.08F+.18F*k)) p->drag=3;
  if(random01()<.4F*k) {
    const glm::vec3 side(-behind.z,0,behind.x);
    if(auto* p=spawn(ParticleKind::Ray,player_.position+glm::vec3(0,.3F+.8F*random01(),0)+side*(random01()-.5F)*.9F,{},{.75F,.92F,1},.12F,.03F)) p->extent=behind*1.6F;
  }
}
// Training course, step by step (hall 1 heads east, hall 2 west, hall 3 east to the exit). Each
// step completes on its action or once the courier is past that part of the course. An edited
// training map may lack an item a step points at: that step then counts as done.
bool Game::tutorialStepDone(int step) const {
  const auto p=player_.position;
  const bool hall3=p.z<15.F;
  const auto core=[&](std::size_t i) { return i>=cores_.size()||cores_[i].collected; };
  const auto pickupGone=[&](PowerUp kind) {
    for(const auto& item:pickups_) if(item.kind==kind) return item.taken||item.respawn>0;
    return true;
  };
  switch(step) {
    case 0: return glm::distance(glm::vec2(p.x,p.z),glm::vec2(spawnPoint().x,spawnPoint().z))>3.F;  // move
    case 1: return p.x>22.F||p.z<30.F;                                                              // sprint
    case 2: return p.x>27.6F||p.z<30.F;                                                             // jump the barrier
    case 3: return p.x>35.6F||p.z<30.F;                                                             // crawl
    case 4: return p.x>52.5F||p.z<30.F;                                                             // slide the tunnel
    case 5: return core(0);
    case 6: return checkpoints_.empty()||checkpoints_[0].active||p.x<45.F||hall3;                   // checkpoint
    case 7: return player_.shielded||pickupGone(PowerUp::Shield)||p.x<44.5F||hall3;                // shield
    case 8: return traps_.empty()||traps_[0].disabledFor>0||p.x<39.5F||hall3;                      // relay
    case 9: return core(1);
    case 10: return (p.x<28.5F&&p.z>17.F)||hall3;                                                   // hazard gap
    case 11: return pickupGone(PowerUp::Time)||p.x<19.F||hall3;                                     // clock / crimson
    case 12: return plates_.empty()||plates_[0].down||hall3;                                        // plate
    case 13: return p.z<14.5F;                                                                      // door, by the pad
    case 14: return portals_.empty()||portals_[0].toOn;                                             // the exit-side portal
    case 15: return hall3&&p.x>29.F;                                                                // fly over the trap
    case 16: return player_.jets>0||jetsUsed_>0||pickupGone(PowerUp::Jet)||(hall3&&p.x>44.F);       // jetpack
    case 17: return (jetsUsed_>0&&hall3&&p.x>43.5F)||(hall3&&p.x>50.F);                             // jet across the long trap
    case 18: return wallJumpsDone_>0||core(2);                                                      // wall jump
    case 19: return player_.spring>0||p.y>2.5F||core(2);                                            // spring boots
    case 20: return (hall3&&p.y>2.5F)||core(2);                                                     // up to the upper floor
    case 21: return core(2);
    case 22: return portals_.empty()||portals_[0].linked();                                         // link the portals
    case 23: return portals_.empty()||teleports_>0;                                                 // take the portal
    default: return false;
  }
}
glm::vec3 Game::tutorialTarget() const {
  const auto coreAt=[&](std::size_t i) { return i<cores_.size()?glm::vec3(cores_[i].position.x,cores_[i].position.y-.65F,cores_[i].position.z):gatePosition(); };
  const auto pickupAt=[&](PowerUp kind,glm::vec3 fallback) { for(const auto& item:pickups_) if(item.kind==kind) return item.position; return fallback; };
  const auto portalAt=[&](bool from) { return portals_.empty()?gatePosition():from?portals_[0].from:portals_[0].to; };
  switch(tutorialStep_) {
    case 0: return {16,1,39};
    case 1: return {24,1,39};
    case 2: return {27,1,39};
    case 3: return {34.5F,1,39};
    case 4: return {48,1,39};
    case 5: return coreAt(0);
    case 6: return checkpoints_.empty()?gatePosition():checkpoints_[0].position;
    case 7: return pickupAt(PowerUp::Shield,{49.5F,1,18.6F});
    case 8: return switches_.empty()?glm::vec3(47.5F,1,21):switches_[0].position;
    case 9: return coreAt(1);
    case 10: return {31,1,23.5F};
    case 11: return pickupAt(PowerUp::Time,{22.5F,1,28.5F});
    case 12: return plates_.empty()?glm::vec3(16,1,26.5F):plates_[0].position;
    case 13: return doors_.empty()?glm::vec3(8,1,16):glm::vec3(doors_[0].centre.x,1,doors_[0].centre.z);
    case 14: return portalAt(false);
    case 15: return {20.5F,1,9.75F};
    case 16: return pickupAt(PowerUp::Jet,{30.5F,1,9.75F});
    case 17: return {45,1,9.75F};
    case 18: return {59,1,10.25F};
    case 19: return pickupAt(PowerUp::Spring,{51,1,12.5F});
    case 20: return {50,3,6};
    case 21: return coreAt(2);
    case 22: case 23: return portalAt(true);
    default: return gatePosition();
  }
}
const char* Game::tutorialHint() const {
  static const char* hints[tutorialSteps]{
    "MOVE", "SPRINT", "JUMP THE BARRIER", "CROUCH UNDER", "SLIDE THROUGH", "RECOVER CORE",
    "ACTIVATE CHECKPOINT", "SHIELD: BLOCKS ONE HIT", "RELAY: DISABLE THE TRAP", "RECOVER CORE",
    "DODGE THE HAZARD", "CLOCK +30S / CRIMSON -5S", "STAND ON THE PLATE", "SPEED PAD: BEAT THE DOOR",
    "ACTIVATE THE PORTAL", "SPRINT ONTO THE PAD: FLY", "JETPACK", "DOUBLE-TAP SPRINT: JET",
    "PAD, THEN JUMP AT THE WALL", "SPRING BOOTS", "SPRING UP TO THE UPPER FLOOR", "RECOVER CORE",
    "ACTIVATE ITS PAIR", "STEP IN: SHORTCUT TO THE EXIT", "EXIT"};
  return hints[std::clamp(tutorialStep_,0,tutorialSteps-1)];
}
void Game::updateTutorial(float dt) {
  if(!tutorial()) return;
  tutorialAge_+=dt;
  // Steps cascade, so running ahead skips the ones already covered.
  bool advanced=false;
  while(tutorialStep_<tutorialSteps-1&&tutorialStepDone(tutorialStep_)) { ++tutorialStep_; advanced=true; }
  if(!advanced) return;
  tutorialAge_=0;
  // Step complete: a ring of mint motes rises around the courier.
  for(int i=0;i<10;++i) {
    const float a=float(i)*.628F;
    if(auto* q=spawn(ParticleKind::Glow,player_.position+glm::vec3(std::cos(a)*.6F,.3F,std::sin(a)*.6F),{std::cos(a)*.6F,2.2F,std::sin(a)*.6F},{.35F,1,.8F},.6F,.12F)) { q->drag=2; q->gravity=-.5F; }
  }
  shockwave(player_.position+glm::vec3(0,.04F,0),{.35F,1,.8F},1.2F,.3F);
}
void Game::startTrick(int kind) {
  // A short, readable trick marks the switch between moves (see CourierPose.cpp). A ring of
  // light motes whips around the body in the trick's direction and a small ring snaps out.
  auto& m=player_.motion; m.trickKind=kind; m.trickAge=0;
  // Boosted tricks freeze the frame for an instant (presentation only: main drains hitStop) so
  // the switch lands with weight.
  if(kind==5||kind==7||(kind==2&&boostTime_>0)) hitStop_=std::max(hitStop_,.04F);
  const glm::vec3 chest=player_.position+glm::vec3(0,.7F,0),forward(-std::sin(player_.yaw),0,-std::cos(player_.yaw)),side(std::cos(player_.yaw),0,-std::sin(player_.yaw));
  for(int i=0;i<12;++i) {
    const float a=float(i)*.5236F;
    // The ring lies across the trick's axis: around forward for a roll, around the side for a
    // flip, around up for a spin; motes are flung along it.
    const bool spin=kind==3||kind==5||kind==6;
    const glm::vec3 axis=kind==1?forward:spin?glm::vec3(0,1,0):side;
    const glm::vec3 u=spin?side:glm::vec3(0,1,0),w=glm::normalize(glm::cross(axis,u));
    const glm::vec3 radial=u*std::cos(a)+w*std::sin(a),tangent=glm::cross(axis,radial);
    if(auto* p=spawn(ParticleKind::Glow,chest+radial*.55F,tangent*5.F+radial*1.5F,i%2?glm::vec3(.85F,.95F,1):glm::vec3(.45F,.85F,1),.3F,.07F)) p->drag=4;
  }
  shockwave(player_.position+glm::vec3(0,.05F,0),{.6F,.88F,1},1.1F,.22F);
}
void Game::wallJump() {
  // Kick off the wall: away from it at wall-kick speed (or most of the run-in speed), keeping
  // some of the speed along it, with a hop capped at a floor jump's apex. The courier turns to
  // face away at once and the wall-kick trick swings it round from facing the wall.
  const glm::vec2 out=wallNormal_;
  const glm::vec2 along=player_.locomotion-glm::dot(player_.locomotion,out)*out;
  player_.locomotion=along*.7F+out*std::max(wallKickSpeed,wallSpeed_*.85F);
  const float apex=jumpSpeed*jumpSpeed/(2*gravity),room=groundY_+apex+.25F-player_.position.y;
  player_.velocity.y=std::max(3.F,std::sqrt(2*gravity*std::max(0.F,room)));
  player_.grounded=false; carrying_=true; bounceTime_=0; bounceCooldown_=.3F;
  wallTouch_=0; wallKick_=wallKickLock; ++wallJumps_; ++wallJumpsDone_;
  if(player_.boost==Boost::Flight) endBoost(false);
  player_.jumpBuffer=player_.coyoteTime=0;
  player_.jumpActive=true; player_.jumpAge=0; player_.jumpCutQueued=false;
  const float v=glm::length(player_.locomotion);
  player_.yaw=std::atan2(-player_.locomotion.x/v,-player_.locomotion.y/v);
  auto& m=player_.motion; m.yawVelocity=0; m.launchAge=0;
  startTrick(8);
  hitStop_=std::max(hitStop_,.05F);
  wallImpact(out,wallPoint_);
}
bool Game::findWall(glm::vec2 direction,float reach,glm::vec2& normal,glm::vec3& point,float& distance) const {
  // Walls are voxel faces, so look along the axis the heading leans on, if it leans on it at
  // wallAngle or more: march the body forward (lifted off the floor so the floor never counts)
  // until it would touch something, up to reach.
  const glm::vec3 feet=player_.position+glm::vec3(0,.15F,0);
  for(int axis=0;axis<2;++axis) {
    const float d=axis==0?direction.x:direction.y;
    if(std::abs(d)<wallAngle) continue;
    const float side=d>0?1.F:-1.F;
    for(float step=.1F;step<=reach+1e-4F;step+=.1F) {
      glm::vec3 probe=feet; probe[axis==0?0:2]+=side*step;
      if(!blockedPlayer(probe)) continue;
      normal=axis==0?glm::vec2(-side,0):glm::vec2(0,-side); distance=step;
      point=player_.position+glm::vec3(axis==0?side*(playerRadius+step-.1F):0.F,.9F,axis==1?side*(playerRadius+step-.1F):0.F);
      return true;
    }
  }
  return false;
}
glm::vec2 Game::wallHeading() const {
  // Ahead: the way the courier moves, or faces when still.
  const float v=glm::length(player_.locomotion);
  return v>.8F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw));
}
void Game::wallAction() {
  // The wall action (left mouse): fire now if the wall is within reach, else wait for the courier
  // to get there (as long as it is heading in fast enough to arrive within wallBuffer).
  glm::vec2 n; glm::vec3 point; float distance=0;
  if(!findWall(wallHeading(),wallTrigger,n,point,distance)) return;
  if(distance<=wallReach) { wallActionAt(n,point); return; }
  const float approach=-glm::dot(player_.locomotion,n);
  if(approach<1.F) return;
  wallQueue_=std::min(wallBuffer,(distance-wallReach)/approach+.12F); wallQueueNormal_=n;
}
void Game::wallActionAt(glm::vec2 n,glm::vec3 point) {
  const float v=glm::length(player_.locomotion);
  wallQueue_=0;
  const bool flying=player_.boost==Boost::Flight,sliding=player_.sliding,airborne=!player_.grounded&&!flying;
  const bool running=player_.grounded&&!sliding&&(player_.sprinting||v>sprintSpeed*.8F);
  wallActionWait_=wallActionCooldown;
  auto& m=player_.motion;
  if(!flying&&!sliding&&!airborne&&!running) {
    // Walking or standing: both palms shove off the wall and the courier is pushed back, still
    // facing it.
    player_.locomotion=n*wallShove; player_.yaw=std::atan2(n.x,n.y); m.yawVelocity=0; m.pushAge=0;
    wallKick_=.2F; hitStop_=std::max(hitStop_,.03F);
    dust(point-glm::vec3(0,.5F,0),6,1.2F,.12F);
    for(float side:{-1.F,1.F}) flash(point-glm::vec3(n.x,0,n.y)*.02F+glm::vec3(-n.y,0,n.x)*(.22F*side)+glm::vec3(0,.2F,0),{.7F,.9F,1},.35F,.1F);
    shockwave(point,{.6F,.85F,1},.7F,.2F);
    return;
  }
  if(airborne&&wallJumps_>=wallJumpsPerAir) return;
  // Kick: mirror the motion off the wall at the same speed (at least a kick's worth), making sure
  // it heads well away from the wall, and turn to face the new way.
  // wallRebound times the arrival speed (from at least a kick's worth), capped.
  const float speed=std::min(wallReboundMax,wallRebound*std::max(v,flying?flightTakeoff*.5F:running?sprintSpeed:sliding?slideStartSpeed:wallKickSpeed*.6F));
  glm::vec2 out=v>.1F?player_.locomotion-2*glm::dot(player_.locomotion,n)*n:n;
  if(glm::dot(out,n)<.55F*glm::length(out)) { const glm::vec2 along=out-glm::dot(out,n)*n; out=n*.8F+along*.6F/std::max(.001F,glm::length(out)); }
  out=glm::normalize(out)*speed;
  player_.locomotion=out; player_.yaw=std::atan2(-out.x/speed,-out.y/speed); m.yawVelocity=0;
  carrying_=true; wallKick_=wallKickLock; ++wallJumpsDone_; bounceTime_=0; bounceCooldown_=.3F;
  // Snap off the wall at once (swept, so it stops at anything behind).
  if(std::abs(n.x)>.5F) moveAxis(0,n.x*wallSnap); else moveAxis(2,n.y*wallSnap);
  // Each state leaves the wall its own way: a jump kicks into the back flip with a hop; a sprint
  // runs a step up and lays out a high back flip; a slide springs up off the wall into a low
  // aerial; a flight stays a flight and flip-turns.
  int trick=8; float hop=1.25F;
  if(flying) { trick=11; hop=0; }
  else if(sliding) { trick=10; hop=.95F; player_.sliding=false; crouchToggle_=false; standUp(); }
  else if(running) { trick=9; hop=1.85F; }
  if(hop>0) {
    player_.velocity.y=std::max(player_.velocity.y,std::sqrt(2*gravity*hop)); player_.grounded=false;
    player_.jumpActive=false; player_.jumpBuffer=player_.coyoteTime=0; m.launchAge=0; ++wallJumps_;
  }
  startTrick(trick);
  hitStop_=std::max(hitStop_,running?.065F:.05F);
  wallImpact(n,point);
}
void Game::wallImpact(glm::vec2 out,glm::vec3 point) {
  // Impact on the wall: a ring of light motes flung out across the wall face, a spray of
  // sparks and chips off it, a flash and a small shell, boot flames stamped into the wall and
  // dust shaken down from the contact.
  const glm::vec3 n(out.x,0,out.y),up(0,1,0),across=glm::cross(up,n);
  for(int i=0;i<14;++i) {
    const float a=float(i)*.4488F;
    const glm::vec3 radial=up*std::cos(a)+across*std::sin(a);
    if(auto* p=spawn(ParticleKind::Glow,point+radial*.15F+n*.05F,radial*4.5F+n*1.2F,i%2?glm::vec3(.85F,.95F,1):glm::vec3(.45F,.85F,1),.35F,.08F)) p->drag=4;
  }
  sparks(point,n+up*.35F,{.75F,.92F,1},14,7.F,1.2F);
  sparks(point,n+up*.2F,{1,.8F,.5F},6,5.F,1.F);
  burst(point,{.55F,.57F,.58F},5,1.6F);
  dust(point-up*.35F,8,1.6F,.14F);
  flash(point+n*.1F,{.6F,.85F,1},1.1F,.12F);
  if(auto* p=spawn(ParticleKind::Shell,point+n*.08F,{},{.55F,.88F,1},.2F,.1F)) p->growth=.9F;
  for(bool right:{false,true}) {
    const glm::vec3 foot=footPosition(right)+glm::vec3(0,.08F,0)-n*.15F;
    if(auto* p=spawn(ParticleKind::Fireball,foot,-n*2.5F,{.35F,.75F,1},.22F,.1F)) { p->growth=4.F; p->drag=5; }
    if(auto* p=spawn(ParticleKind::Glow,foot,-n*1.5F,{.5F,.88F,1},.24F,.26F)) p->drag=6;
  }
  for(int i=0;i<6;++i) { const float a=float(i)*1.0472F+random01()*.5F; if(auto* p=spawn(ParticleKind::Ray,point,{},{.75F,.92F,1},.16F,.04F)) p->extent=glm::normalize(n*.3F+up*std::cos(a)+across*std::sin(a))*1.2F; }
}
void Game::startFlight(glm::vec2 direction,float speed,bool refresh) {
  // Take-off: a crouch-and-spring into the hover, boot jets lighting, a bright double ring.
  const bool fresh=player_.boost!=Boost::Flight;
  player_.boost=Boost::Flight; carrying_=true;
  if(refresh) boostTime_=boostDuration;
  player_.locomotion=direction*std::max(speed,glm::length(player_.locomotion));
  if(!fresh) return;
  player_.motion.takeoffAge=0;
  const auto feet=player_.position+glm::vec3(0,.05F,0);
  const glm::vec3 back(-direction.x,0,-direction.y);
  shockwave(feet,{.55F,.6F,1},1.6F,.3F); shockwave(feet,{.45F,1,.95F},1.1F,.22F);
  flash(feet+glm::vec3(0,.5F,0),{.65F,.75F,1},1.4F,.16F);
  for(bool right:{false,true}) {
    const auto foot=footPosition(right)+glm::vec3(0,.05F,0);
    if(auto* p=spawn(ParticleKind::Fireball,foot,glm::vec3(0,-1.5F,0)+back*2.F,{.55F,.45F,1},.3F,.12F)) { p->growth=4.F; p->drag=4; }
  }
  sparks(feet,back+glm::vec3(0,.6F,0),{.75F,.6F,1},16,7.F,1.2F);
  dust(feet,10,2.4F,.14F);
}
void Game::endBoost(bool touchdown) {
  const bool flying=player_.boost==Boost::Flight;
  player_.boost=Boost::None;
  if(!flying||!touchdown) return;
  // Touchdown: the boots come down into a landing dip with a dust skirt and a fading ring.
  auto& m=player_.motion; m.touchAge=0; m.landAge=0; m.landStrength=.9F;
  const auto feet=player_.position+glm::vec3(0,.04F,0);
  dust(feet,12,2.F,.14F); shockwave(feet,{.55F,.75F,1},1.2F,.25F); shockwave(feet,{.75F,.55F,1},2.F,.32F);
  for(int i=0;i<10;++i) { const float a=float(i)*.628F; sparks(feet+glm::vec3(0,.05F,0),{std::cos(a),.25F,std::sin(a)},i%2?glm::vec3(.72F,.5F,1):glm::vec3(.5F,.9F,1),1,5.F,.4F); }
  for(bool right:{false,true}) if(auto* p=spawn(ParticleKind::Glow,footPosition(right)+glm::vec3(0,.05F,0),{0,.4F,0},{.55F,.6F,1},.25F,.22F)) p->drag=5;
}
float Game::boostMeter() const {
  if(boostTime_>0) return std::clamp(boostTime_/boostDuration,0.F,1.F);
  const float v=glm::length(player_.locomotion);
  if(player_.boost==Boost::Stride) return std::clamp((v-speed)/(strideSpeed-speed),0.F,1.F);
  if((player_.sliding||carrying_||!player_.grounded)&&v>sprintSpeed+.3F) return std::clamp((v-sprintSpeed)/(padSpeed+slidePadBonus-sprintSpeed),0.F,1.F);
  return 0;
}
void Game::boostEffects(float dt) {
  // Continuous boost trails. While any pad boost is left, twin backpack thrusters burn on the
  // courier itself (a white core, blue flame, sparks), scaled by the meter. On top of that a
  // stride streaks light from the boots, and a flight burns two vivid boot jets (white core,
  // cyan and violet flame), throws rays past the shell and blows floor dust.
  const float meter=state_==RunState::Playing?boostMeter():0;
  if(meter<=0&&player_.boost==Boost::None) { jetClock_=0; return; }
  jetClock_+=dt;
  if(jetClock_<.025F) return;
  jetClock_=0;
  const float v=glm::length(player_.locomotion);
  const glm::vec3 behind=v>.1F?-glm::vec3(player_.locomotion.x,0,player_.locomotion.y)/v:glm::vec3(std::sin(player_.yaw),0,std::cos(player_.yaw));
  if(meter>0) {
    const glm::vec3 back(std::sin(player_.yaw),0,std::cos(player_.yaw)),side(std::cos(player_.yaw),0,-std::sin(player_.yaw));
    const float hover=.38F*player_.flightBlend,pack=(1.F-.45F*player_.crouchBlend-.25F*player_.slideBlend)*modelScale+hover;
    // Grey exhaust rolls out behind the flame, thicker while sliding.
    for(float s:{-1.F,1.F}) if(random01()<(player_.sliding?.6F:.3F)) smokePuff(player_.position+glm::vec3(0,pack,0)+back*.45F+side*(s*.14F),back*(1.5F+2.F*meter)+glm::vec3(0,.3F,0),.09F+.05F*meter,.6F);
    for(float s:{-1.F,1.F}) {
      const glm::vec3 nozzle=player_.position+glm::vec3(0,pack,0)+back*.36F+side*(s*.14F);
      const glm::vec3 thrust=back*(3.F+3.F*meter)+glm::vec3(0,-.8F,0);
      if(auto* p=spawn(ParticleKind::Glow,nozzle,thrust*.6F,{.9F,.97F,1},.14F+.06F*meter,.07F+.06F*meter)) p->drag=6;
      if(auto* p=spawn(ParticleKind::Fireball,nozzle+back*.08F,thrust,s<0?glm::vec3(.45F,.75F,1):glm::vec3(.6F,.85F,1),.16F+.08F*meter,.05F+.03F*meter)) { p->growth=2.5F+2.F*meter; p->drag=4; }
    }
    if(random01()<.5F*meter) sparks(player_.position+glm::vec3(0,pack,0)+back*.45F,back+glm::vec3(0,-.3F,0),{.75F,.9F,1},1,3.F+2.F*meter,.6F);
    if(random01()<.25F*meter) embers(player_.position+glm::vec3(0,pack-.1F,0)+back*.5F,{.6F,.8F,1},1,1.2F);
  }
  if(player_.boost==Boost::None) return;
  if(player_.boost==Boost::Stride) {
    for(bool right:{false,true}) if(auto* p=spawn(ParticleKind::Glow,footPosition(right)+glm::vec3(0,.12F,0),behind*1.2F,{.40F,.82F,1},.22F,.12F)) p->drag=4;
    if(random01()<.5F) if(auto* p=spawn(ParticleKind::Ray,player_.position+glm::vec3(0,.2F+.6F*random01(),0),{},{.75F,.92F,1},.12F,.025F)) p->extent=behind*1.2F;
    return;
  }
  const float hover=.38F*player_.flightBlend;
  for(bool right:{false,true}) {
    const auto boot=footPosition(right)+glm::vec3(0,hover,0)+behind*.25F;
    if(auto* p=spawn(ParticleKind::Glow,boot,behind*3.F+glm::vec3(0,-.6F,0),{.92F,.97F,1},.16F,.14F)) p->drag=6;
    if(auto* p=spawn(ParticleKind::Fireball,boot,behind*4.F,right?glm::vec3(.35F,.85F,1):glm::vec3(.65F,.45F,1),.2F,.07F)) { p->growth=3.5F; p->drag=4; }
  }
  if(random01()<.6F) {
    const glm::vec3 side(-behind.z,0,behind.x);
    if(auto* p=spawn(ParticleKind::Ray,player_.position+glm::vec3(0,.3F+.9F*random01(),0)+side*(random01()-.5F)*1.1F,{},random01()<.5F?glm::vec3(.75F,.92F,1):glm::vec3(.8F,.65F,1),.14F,.03F)) p->extent=behind*2.2F;
  }
  if(random01()<.35F) embers(player_.position+glm::vec3(0,hover+.3F,0)+behind*.4F,{.75F,.6F,1},1,1.F);
  {
    // Twin helix ribbons (cyan and violet) wind around the line of flight behind the body.
    const glm::vec3 side(-behind.z,0,behind.x),centre=player_.position+glm::vec3(0,hover+.55F,0)+behind*.5F;
    const float spin=elapsed_*16.F;
    for(int strand=0;strand<2;++strand) {
      const float a=spin+float(strand)*3.14159F;
      const glm::vec3 at=centre+side*std::cos(a)*.42F+glm::vec3(0,std::sin(a)*.42F,0);
      if(auto* p=spawn(ParticleKind::Glow,at,behind*.8F,strand?glm::vec3(.75F,.5F,1):glm::vec3(.45F,.9F,1),.4F,.1F)) p->drag=2;
    }
    // Speed lines: streaks appear ahead and to the sides and rush past.
    for(int i=0;i<2;++i) {
      const glm::vec3 at=player_.position-behind*(1.F+1.5F*random01())+side*((random01()<.5F?-1.F:1.F)*(.6F+1.F*random01()))+glm::vec3(0,.2F+1.6F*random01(),0);
      if(auto* p=spawn(ParticleKind::Ray,at,behind*6.F,glm::vec3(.85F,.95F,1),.16F,.02F)) p->extent=behind*(1.5F+1.5F*random01());
    }
  }
  // Jet wash: the boot jets blast the floor below, throwing heavy dust out sideways and back and
  // rolling a low cloud behind the flight.
  {
    const glm::vec3 side(-behind.z,0,behind.x),floor=player_.position+glm::vec3(0,.05F,0)+behind*.35F;
    for(int i=0;i<5;++i) {
      const float a=random01()*6.2831853F;
      const glm::vec3 out=glm::normalize(glm::vec3(std::cos(a),0,std::sin(a))+behind*1.2F);
      if(auto* p=spawn(ParticleKind::Dust,floor+out*.2F,out*(3.5F+3.F*random01())+glm::vec3(0,.4F+.6F*random01(),0),glm::mix(glm::vec3(.24F,.23F,.22F),glm::vec3(.38F,.37F,.35F),random01()),.75F+.3F*random01(),.3F+.14F*random01())) {
        p->drag=3.5F; p->gravity=-.2F; p->growth=2.6F; p->turbulence=2.F; p->extent.x=.9F;  // heavy
      }
    }
    smokePuff(floor+behind*.6F+side*(random01()-.5F)*.6F,behind*(2.5F+2.F*random01())+glm::vec3(0,.3F,0),.2F+.08F*random01(),.9F);
    if(random01()<.5F) dust(floor,3,3.F,.2F,{.32F,.31F,.30F});
  }
}
void Game::updatePickups(float dt) {
  player_.spring=std::max(0.F,player_.spring-dt);
  bonusAge_=std::min(10.F,bonusAge_+dt);
  for(auto& item:pickups_) {
    if(item.taken) continue;
    if(item.respawn>0) {
      item.respawn=std::max(0.F,item.respawn-dt);
      // Coming back: a ring and a puff of motes as it rematerialises.
      if(item.respawn<=0) { shockwave(item.position+glm::vec3(0,.05F,0),pickupColour(item.kind),1.F,.3F); }
      continue;
    }
    const glm::vec2 offset(player_.position.x-item.position.x,player_.position.z-item.position.z);
    if(glm::length(offset)>pickupRadius||std::abs(player_.position.y-item.position.y)>1.6F) continue;
    player_.motion.pickupAge=0; player_.motion.pickupKind=int(item.kind);
    const glm::vec3 colour=pickupColour(item.kind),at=item.position+glm::vec3(0,.9F,0);
    if(item.kind==PowerUp::Time) {
      // The clock: +30 s on the countdown, once. A gold ring of motes climbs around the courier
      // like a clock face winding up, with a double ring at the feet.
      if(practice()) item.respawn=pickupRespawnTime(); else item.taken=true;  // the practice field's comes back too
      bonusAge_=0;
      if(timed()) remaining_+=timeBonus;
      notify(timed() ? "Clock: +30 seconds on the countdown." : "Clock: in a mission this adds 30 seconds.");
      for(int i=0;i<12;++i) {
        const float a=float(i)*.5236F;
        if(auto* p=spawn(ParticleKind::Glow,player_.position+glm::vec3(std::cos(a)*.7F,.2F,std::sin(a)*.7F),{-std::sin(a)*2.F,3.F,std::cos(a)*2.F},i%3==0?glm::vec3(1,.95F,.8F):colour,.8F,.09F)) { p->drag=1.5F; p->gravity=-.5F; }
      }
      shockwave(player_.position+glm::vec3(0,.05F,0),{1,.95F,.75F},1.1F,.25F);
    } else {
      item.respawn=pickupRespawnTime();
      if(item.kind==PowerUp::Shield) { player_.shielded=true; notify("Shield: blocks the next hit."); }
      else if(item.kind==PowerUp::Jet) { player_.jets=1; notify("Jetpack: double-tap sprint, or tap it mid-slide."); }
      else { player_.spring=springTime; notify("Spring boots: jumps go much higher for 12 seconds."); }
    }
    // Take: a flash, a pillar, a ring and a spiral of motes pulled into the courier.
    flash(at,colour,1.4F,.16F);
    pillar(item.position,colour,.5F);
    shockwave(item.position+glm::vec3(0,.05F,0),colour,1.6F,.3F);
    for(int i=0;i<12;++i) {
      const float a=float(i)*.5236F;
      const glm::vec3 from=at+glm::vec3(std::cos(a)*.8F,(float(i%3)-1.F)*.3F,std::sin(a)*.8F);
      if(auto* p=spawn(ParticleKind::Glow,from,(player_.position+glm::vec3(0,.8F,0)-from)*2.5F,colour,.4F,.1F)) p->drag=2;
    }
    sparks(at,{0,1,0},colour,10,4.F,1.2F);
  }
}
void Game::standUp() {
  player_.crouching = false;
  if (blockedPlayer(player_.position)) player_.crouching = true;
}
void Game::warpPlayer(glm::vec3 feet) { player_.position = feet; player_.motion=CourierMotion{}; player_.velocity = glm::vec3(0); player_.locomotion={0,0}; player_.coyoteTime=player_.jumpBuffer=0; player_.jumpActive=player_.jumpCutQueued=false; player_.moveBlend=player_.sprintBlend=player_.airBlend=player_.landingBlend=player_.hitBlend=player_.turnLean=0; player_.knockback={0,0}; player_.hitStun=0; player_.interactionTime=0; player_.interaction=Interaction::None; player_.grounded = false; player_.crouching=false; player_.crouchBlend=0; bounceTime_=0; player_.boost=Boost::None; player_.strideBlend=player_.flightBlend=0; boostTime_=0; resumeFlight_=false; wallTouch_=wallKick_=0; wallJumps_=0; wallQueue_=0; }
void Game::setTimeForDebug(float seconds) { remaining_ = std::max(0.0F, seconds); }
void Game::notify(std::string message) { message_ = std::move(message); messageTime_ = 3.5F; }
void Game::applyDamage(glm::vec3 source,int sourceId,bool launch,DamageKind kind) {
  if (state_!=RunState::Playing || graceTime_>0) return;
  if (sourceId==lastHitSource_ && sourceImmunity_>0) return;
  lastHitSource_=sourceId; sourceImmunity_=sameSourceImmunity;
  if (player_.shielded) {
    // The shield takes the blow: it bursts into cyan hex shards with a ring, the courier is only
    // nudged away, and a short grace stops the same hazard landing straight after.
    player_.shielded = false; player_.motion.shieldBreakAge = 0; graceTime_ = std::max(graceTime_, .5F);
    glm::vec3 away(player_.position.x - source.x, 0, player_.position.z - source.z);
    away = glm::length(away) > .01F ? glm::normalize(away) : glm::vec3(std::sin(player_.yaw), 0, std::cos(player_.yaw));
    if (launch) player_.knockback = glm::vec2(away.x, away.z) * 3.5F;
    const auto chest = player_.position + glm::vec3(0, .8F, 0);
    flash(chest, {.55F, .95F, 1}, 1.8F, .14F);
    if (auto* p = spawn(ParticleKind::Shell, chest, {}, {.5F, .95F, 1}, .25F, .9F)) p->growth = 1.8F;
    for (int i = 0; i < 16; ++i) {
      const float u = random01() * 2 - 1, a = random01() * 6.2831853F, r = std::sqrt(1 - u * u);
      const glm::vec3 out(std::cos(a) * r, std::abs(u), std::sin(a) * r);
      if (auto* p = spawn(ParticleKind::Debris, chest + out * .8F, out * (3.F + 3.F * random01()), {.55F, .92F, 1}, .7F, .09F)) p->growth = .1F;
    }
    // Shards of the bubble: glowing glass plates flung off its surface (harder away from the
    // blow), tumbling down to bounce and lie scattered on the floor for a moment before fading.
    for (int i = 0; i < 24; ++i) {
      const float u = random01() * 1.6F - .6F, a = random01() * 6.2831853F, r = std::sqrt(std::max(0.F, 1 - u * u));
      const glm::vec3 out(std::cos(a) * r, u, std::sin(a) * r);
      const glm::vec3 velocity = out * (2.5F + 3.5F * random01()) + away * 2.F + glm::vec3(0, 2.5F + 2.F * random01(), 0);
      if (auto* p = spawn(ParticleKind::Debris, chest + out * .85F, velocity, i % 4 == 0 ? glm::vec3(.85F, .97F, 1) : glm::vec3(.45F, .9F, 1), 1.6F + .8F * random01(), .07F + .07F * random01())) {
        p->extent = {1, 0, 0}; p->gravity = 9.F; p->drag = .6F;
      }
    }
    sparks(chest, {0, .5F, 0}, {.6F, .95F, 1}, 22, 7.F, 1.4F);
    shockwave(player_.position + glm::vec3(0, .04F, 0), {.5F, .95F, 1}, 2.F, .3F);
    shockwave(player_.position + glm::vec3(0, .06F, 0), {.85F, .97F, 1}, 3.F, .4F);
    flash(chest, {.8F, .97F, 1}, 2.6F, .1F);
    hitStop_ = std::max(hitStop_, .08F);
    notify("Shield absorbed the hit.");
    return;
  }
  if (timed()) remaining_ = std::max(0.0F, remaining_ - hazardPenalty);
  player_.health=std::max(0,player_.health-1);
  player_.hurtCooldown=1.1F;
  player_.hitStun=.32F;
  player_.locomotion={0,0}; player_.jumpActive=false; player_.jumpBuffer=player_.coyoteTime=0;
  player_.interaction=Interaction::None; player_.interactionTime=0; interactionIndex_=-1;
  player_.sprinting=false;
  player_.motion.celebrateAge=10;
  // Impact direction in the courier's frame drives the directional recoil.
  glm::vec3 away(player_.position.x-source.x,0,player_.position.z-source.z);
  away=glm::length(away)>.01F?glm::normalize(away):glm::vec3(std::sin(player_.yaw),0,std::cos(player_.yaw));
  auto& m=player_.motion; const bool electric=kind==DamageKind::Electric;
  m.hitSide=glm::dot(away,glm::vec3(std::cos(player_.yaw),0,-std::sin(player_.yaw)));
  m.hitForward=glm::dot(away,glm::vec3(-std::sin(player_.yaw),0,-std::cos(player_.yaw)));
  m.hitAge=0; m.hitKind=electric?1:0; if(electric) m.shockAge=0;
  // Blunt blows shove hard and low; a shock jolts the courier up off the plate.
  if(launch) { player_.knockback=glm::vec2(away.x,away.z)*(electric?6.5F:9.5F); player_.velocity.y=electric?7.5F:5.2F; player_.grounded=false; }
  const auto chest=player_.position+glm::vec3(0,.9F,0);
  if(electric) {
    // Electric: blue-white flash and burst, arcs leaping off the shell and down to the floor.
    flash(chest,{.6F,.9F,1},1.5F,.14F);
    impactBurst(chest,{.45F,.85F,1},2.2F,14,26);
    for(int i=0;i<8;++i) {
      const float a=random01()*6.2831853F,h=random01();
      arc(chest+glm::vec3(0,(h-.5F)*.8F,0),player_.position+glm::vec3(std::cos(a)*.9F,i%2?.05F:.9F+h*.6F,std::sin(a)*.9F),{.45F,.8F,1},.14F);
    }
    embers(chest,{.5F,.85F,1},10,2.F);
    shockwave(player_.position+glm::vec3(0,.04F,0),{.45F,.85F,1},1.9F,.32F);
    hitStop_=.06F;
  } else {
    // Blunt: warm-white flash, chunky shell fragments, a dust cloud and a heavy ground ring.
    flash(chest-away*.2F,{1,.9F,.75F},1.6F,.14F);
    impactBurst(chest-away*.3F,{1,.88F,.7F},2.6F,10,14);
    sparks(chest,away+glm::vec3(0,.6F,0),{1,.62F,.25F},14,7.F,.8F);
    const glm::vec3 shards[]{{.90F,.90F,.86F},{.20F,.22F,.24F},{.12F,.39F,.57F}};
    for(int i=0;i<12;++i) {
      const float x=random01(),y=random01(),z=random01(),s=random01(),size=random01();
      const glm::vec3 v=away*(2.F+3.F*s)+glm::vec3((x-.5F)*3.F,2.F+2.5F*y,(z-.5F)*3.F);
      if(auto* p=spawn(ParticleKind::Debris,chest,v,shards[i%3],.7F+.3F*s,.06F+.06F*size)) { p->gravity=13; p->drag=.6F; }
    }
    dust(player_.position+glm::vec3(0,.05F,0),14,3.F,.2F,{.62F,.64F,.65F});
    smoke(chest,3,.24F,{.5F,.52F,.53F});
    shockwave(player_.position+glm::vec3(0,.04F,0),{.95F,.92F,.85F},2.4F,.36F);
    scorch(player_.position+glm::vec3(0,.04F,0),.8F,{.12F,.12F,.12F});
    hitStop_=.12F;
  }
  notify(electric?"Shock! -1 armour / -5 seconds.":"Impact! -1 armour / -5 seconds.");
  if(player_.health==0 || remaining_<=0) beginDeath();
}
void Game::beginDeath() {
  if(state_==RunState::Lost) return;
  state_=RunState::Lost; player_.deathTime=0; player_.sprinting=false; player_.sliding=false; player_.slideBlend=0; player_.boost=Boost::None; boostTime_=0; resumeFlight_=false;
  player_.spring=0; player_.shielded=false;  // power-ups are lost with the courier
  // Each destroyed courier costs a life. With lives and time to spare it reboots at a checkpoint;
  // the last life restarts the level (see outOfLives), and running out of time ends the run.
  // Training never runs out of lives or time.
  if(player_.health<=0&&timed()) lives_=std::max(0,lives_-1);
  respawnPending_=player_.health<=0&&(!timed()||(remaining_>respawnPenalty&&lives_>0));
  auto& fall=player_.motion; fall.fallAngle=0; fall.slapAge=10;
  if(fall.hitAge>1) { fall.hitForward=1; fall.hitSide=0; fall.fallSpin=.8F; }
  else {
    // A lethal blow launches the body away from the source with a tumble.
    const float shove=glm::length(player_.knockback);
    if(shove>.1F) player_.knockback*=std::max(1.F,8.5F/shove);
    player_.velocity.y=std::max(player_.velocity.y,6.8F); player_.grounded=false;
    fall.fallSpin=2.7F;
  }
  player_.interaction=Interaction::None; player_.interactionTime=0; interactionIndex_=-1;
  // Shutdown blast: white-hot flash, layered fireballs, a storm of sparks and debris,
  // two blast rings, a scorch mark and a column of smoke.
  const auto core=player_.position+glm::vec3(0,1,0);
  flash(core,{1,.9F,.75F},2.8F,.24F);
  impactBurst(core,{1,.65F,.3F},4.5F,22,30);
  for(int i=0;i<4;++i) {
    const float a=random01()*6.2831853F,r=random01()*.35F;
    fireball(core+glm::vec3(std::cos(a)*r,(random01()-.3F)*.5F,std::sin(a)*r),i==0?glm::vec3(1,.75F,.35F):glm::vec3(1,.45F,.12F),.45F+.2F*random01(),5.F,.55F+.25F*random01());
  }
  sparks(core,{0,1,0},{1,.60F,.22F},64,11.F,1.3F);
  embers(core,{1,.5F,.18F},24,3.F);
  burst(core,{.20F,.22F,.24F},24,4.6F);
  dust(player_.position+glm::vec3(0,.05F,0),18,3.4F,.24F,{.45F,.47F,.48F});
  smoke(core,10,.38F,{.14F,.15F,.16F});
  shockwave(player_.position+glm::vec3(0,.04F,0),{1,.6F,.25F},3.6F,.5F);
  shockwave(player_.position+glm::vec3(0,.05F,0),{1,1,1},2.F,.3F);
  scorch(player_.position+glm::vec3(0,.04F,0),1.7F,{.07F,.06F,.06F});
  hitStop_=std::max(hitStop_,.12F);
  notify(player_.health<=0?"Courier destroyed. Five impacts exhausted the armour.":"Power depleted. Courier shutting down.");
}
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
  const glm::vec3 at=from?link.from:link.to,colour=link.linked()?glm::vec3(.35F,1,.85F):glm::vec3(1,.7F,.25F);
  // Waking: a ring and a column of light, sparks spiralling up; linking also lights the far end.
  shockwave(at+glm::vec3(0,.05F,0),colour,1.6F,.35F); pillar(at,colour,.6F);
  flash(at+glm::vec3(0,.4F,0),colour,1.2F,.16F);
  for(int i=0;i<14;++i) { const float a=float(i)*.4488F; if(auto* p=spawn(ParticleKind::Glow,at+glm::vec3(std::cos(a)*.6F,.1F,std::sin(a)*.6F),{-std::sin(a)*2.F,2.6F,std::cos(a)*2.F},colour,.7F,.08F)) { p->drag=2; p->gravity=-.6F; } }
  if(link.linked()) {
    const glm::vec3 other=from?link.to:link.from;
    shockwave(other+glm::vec3(0,.05F,0),colour,1.6F,.35F); pillar(other,colour,.6F);
    notify("Portals linked. Step into either one to cross to the other.");
  } else notify("Portal online. Activate its pair to link them.");
  hitStop_=std::max(hitStop_,.05F);
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
    // Jump: the courier is pulled into the disc in a swirl of light and pops out of its pair.
    for(const auto& [at,colour]:{std::pair{departure,glm::vec3(.35F,1,.85F)},std::pair{arrival,glm::vec3(.5F,.95F,1)}}) {
      shockwave(at+glm::vec3(0,.05F,0),colour,1.8F,.35F); pillar(at,colour,.5F);
      flash(at+glm::vec3(0,.8F,0),colour,1.6F,.18F);
      for(int i=0;i<16;++i) { const float a=float(i)*.3927F; if(auto* p=spawn(ParticleKind::Glow,at+glm::vec3(std::cos(a)*.9F,.2F+.1F*float(i%4),std::sin(a)*.9F),{-std::cos(a)*1.5F-std::sin(a)*2.F,1.8F,-std::sin(a)*1.5F+std::cos(a)*2.F},colour,.6F,.08F)) { p->drag=2.5F; p->gravity=-.4F; } }
    }
    if(player_.boost!=Boost::None) endBoost(false);
    player_.position=arrival; player_.velocity=glm::vec3(0); player_.locomotion*=.2F; player_.knockback={0,0};
    player_.grounded=true; player_.jumpActive=false; player_.coyoteTime=player_.jumpBuffer=0;
    player_.motion.teleportAge=0; link.pulse=1; portalWait_=portalCooldown; portalHold_=int(i)*2+(1-end); ++teleports_;
    ++revision_;  // Snaps the follow camera to the new position.
    hitStop_=std::max(hitStop_,.06F);
    break;
  }
}
void Game::jetBurst() {
  // Jetpack: one burst, then the charge is spent. A run is thrown forward into a boosted stride at
  // jetSpeed or more; a slide becomes a turbo slide. The boost clock then runs as after a pad.
  player_.jets=std::max(0,player_.jets-1); ++jetsUsed_;
  const float v=glm::length(player_.locomotion);
  const glm::vec2 dir=v>.1F?player_.locomotion/v:glm::vec2(-std::sin(player_.yaw),-std::cos(player_.yaw));
  auto& m=player_.motion; m.jetAge=0; m.boostAge=0; carrying_=true; boostTime_=boostDuration; jetting_=true;
  if(player_.sliding) { player_.locomotion=dir*std::max(v+5.F,padSpeed+slidePadBonus); slideSurge_=slideSurgeLength_=padSurgeTime; m.boostKind=2; m.surge=1; }
  else { player_.locomotion=dir*std::max(v+5.F,jetSpeed); player_.boost=Boost::Stride; m.boostKind=0; m.popAge=0; }
  hitStop_=std::max(hitStop_,.06F);
  // Ignition: two flame tongues and sparks blasting back from the pack, a smoke wake, a ring at
  // the feet and a flash.
  const glm::vec3 ahead(dir.x,0,dir.y),across(-dir.y,0,dir.x),pack=player_.position+glm::vec3(0,1.F,0)-ahead*.35F,feet=player_.position+glm::vec3(0,.05F,0);
  for(float side:{-1.F,1.F}) {
    const glm::vec3 nozzle=pack+across*(.13F*side)-glm::vec3(0,.2F,0);
    if(auto* p=spawn(ParticleKind::Fireball,nozzle,-ahead*5.F-glm::vec3(0,1.F,0),{1,.55F,.2F},.32F,.14F)) { p->growth=5.F; p->drag=4; }
    if(auto* p=spawn(ParticleKind::Fireball,nozzle,-ahead*3.F,{.5F,.85F,1},.2F,.08F)) { p->growth=3.F; p->drag=5; }
    sparks(nozzle,-ahead+glm::vec3(0,-.2F,0),{1,.7F,.35F},10,9.F,.7F);
  }
  for(int i=0;i<8;++i) smokePuff(pack-ahead*.3F,-ahead*(2.F+2.F*random01())+across*(random01()-.5F)*2.F+glm::vec3(0,.4F,0),.18F,.9F);
  shockwave(feet,{1,.65F,.3F},1.8F,.3F); dust(feet,10,2.4F,.14F);
  flash(pack,{1,.75F,.4F},1.4F,.14F);
  // A huge yellow ring blasting out across the floor (two, the outer one slower), a yellow shell
  // round the courier, a big yellow flash and yellow rays.
  shockwave(feet,{1,.86F,.22F},7.F,.55F); shockwave(feet,{1,.95F,.6F},4.2F,.35F);
  if(auto* p=spawn(ParticleKind::Shell,player_.position+glm::vec3(0,.8F,0),{},{1,.88F,.3F},.3F,.2F)) p->growth=2.6F;
  flash(player_.position+glm::vec3(0,.8F,0),{1,.9F,.35F},3.2F,.16F);
  for(int i=0;i<10;++i) { const float a=float(i)*.628F; if(auto* p=spawn(ParticleKind::Ray,feet+glm::vec3(0,.3F,0),{},{1,.9F,.35F},.2F,.05F)) p->extent=glm::vec3(std::cos(a),.12F,std::sin(a))*2.4F; }
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
  // The nearest of a core, a relay or an inactive checkpoint wins.
  const auto p=player_.position+glm::vec3(0,.9F,0);
  Interaction best=Interaction::None; float closest=1e9F;
  const auto consider=[&](Interaction kind,int index,glm::vec3 at) { if(index>=0&&glm::distance(p,at)<closest) { closest=glm::distance(p,at); best=kind; } };
  const int core=nearestCore(),sw=nearestSwitch(),cp=nearestCheckpoint();
  if(core>=0) consider(Interaction::Core,core,cores_[std::size_t(core)].position);
  if(sw>=0) consider(Interaction::Switch,sw,switches_[std::size_t(sw)].position+glm::vec3(0,.9F,0));
  if(cp>=0) consider(Interaction::Checkpoint,cp,checkpoints_[std::size_t(cp)].position+glm::vec3(0,.9F,0));
  if(const int portal=nearestPortal();portal>=0) consider(Interaction::Portal,portal,portalEnd(portal)+glm::vec3(0,.9F,0));
  return best;
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
          core.collected=true; player_.motion.celebrateAge=0;
          // Flash, a pillar of light, two rings, an upward spark fountain and spiralling motes.
          flash(core.position,{.65F,1,.95F},1.6F,.24F);
          pillar(core.position-glm::vec3(0,.6F,0),{.35F,1,.88F},.8F);
          impactBurst(core.position,{.35F,1,.88F},2.8F,16,26);
          shockwave(core.position,{.30F,1,.85F},2.2F,.5F);
          shockwave(core.position-glm::vec3(0,.6F,0),{.85F,1,.97F},1.3F,.32F);
          sparks(core.position,{0,1,0},{.35F,1,.85F},30,7.5F,1.1F);
          embers(core.position,{.55F,1,.9F},16,1.8F);
          for(int i=0;i<18;++i) {
            const float a=float(i)*.349F;
            const glm::vec3 v(std::cos(a)*1.9F-std::sin(a)*1.2F,1.2F+.4F*float(i%3),std::sin(a)*1.9F+std::cos(a)*1.2F);
            if(auto* p=spawn(ParticleKind::Glow,core.position,v,i%3?glm::vec3(.35F,1,.85F):glm::vec3(.95F,1,.98F),.75F+.02F*float(i),.085F)) { p->drag=2.2F; p->gravity=-1.4F; }
          }
          hitStop_=std::max(hitStop_,.075F);
          notify(gateUnlocked()?"All cores recovered. Exit gate opening!":"Energy core recovered.");
        }
      } else if(player_.interaction==Interaction::Switch && interactionIndex_>=0) {
        auto& sw=switches_[std::size_t(interactionIndex_)];
        traps_[std::size_t(sw.trap)].disabledFor=switchDuration; sw.animation=1;
        shockwave(sw.position+glm::vec3(0,1.04F,0),{.25F,.95F,.85F},1.7F,.4F);
        sparks(sw.position+glm::vec3(0,1.05F,0),{0,1,0},{.30F,.95F,.85F},18,5.F,.9F);
        flash(sw.position+glm::vec3(0,1.05F,0),{.35F,1,.9F},.9F,.18F);
        impactBurst(sw.position+glm::vec3(0,1.05F,0),{.3F,.95F,.85F},1.5F,8,10);
        for(int i=0;i<3;++i) { const float a=random01()*6.2831853F; arc(sw.position+glm::vec3(0,1.05F,0),sw.position+glm::vec3(std::cos(a)*.9F,.05F,std::sin(a)*.9F),{.30F,.95F,.85F},.14F); }
        notify("Relay active: linked floor trap safe for 5 seconds.");
      } else if(player_.interaction==Interaction::Checkpoint && interactionIndex_>=0) {
        activateCheckpoint(std::size_t(interactionIndex_));
      } else if(player_.interaction==Interaction::Portal && interactionIndex_>=0) {
        activatePortal(interactionIndex_);
      }
    }
    if(player_.interactionTime<=0) player_.interaction=Interaction::None;
  }
  if(!pressed || player_.interactionTime>0 || player_.hitStun>0) return;
  const auto kind=availableInteraction(); if(kind==Interaction::None) return;
  interactionIndex_=kind==Interaction::Core?nearestCore():kind==Interaction::Switch?nearestSwitch():kind==Interaction::Portal?nearestPortal():nearestCheckpoint();
  player_.interactionTarget=kind==Interaction::Core?cores_[std::size_t(interactionIndex_)].position
    :kind==Interaction::Switch?switches_[std::size_t(interactionIndex_)].position+glm::vec3(0,1,0)
    :kind==Interaction::Portal?portalEnd(interactionIndex_)+glm::vec3(0,.3F,0)
    :checkpoints_[std::size_t(interactionIndex_)].position+glm::vec3(0,1.1F,0);
  player_.interaction=kind; player_.interactionTime=duration; interactionCommitted_=false;
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
float Game::random01() {
  random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
  return float(random_ & 0x00ffffff) / float(0x01000000);
}
Particle* Game::spawn(ParticleKind kind, glm::vec3 position, glm::vec3 velocity, glm::vec3 colour, float lifetime, float size) {
  for (auto& p : particles_) if (p.remaining <= 0) {
    // Lifetimes never exceed one second, so a quiet level empties quickly.
    p = Particle{}; p.kind = kind; p.position = position; p.velocity = velocity; p.colour = colour;
    p.lifetime = p.remaining = std::min(lifetime, 1.0F); p.size = size; p.seed = random01();
    if (kind != ParticleKind::Debris) p.gravity = 0;
    return &p;
  }
  return nullptr;
}
void Game::burst(glm::vec3 position, glm::vec3 colour, int count, float energy) {
  for (int i = 0; i < count; ++i) {
    const float x = random01(), y = random01(), z = random01(), life = random01(), size = random01();
    auto* p = spawn(ParticleKind::Debris, position, glm::vec3((x - .5F) * 2, .4F + y, (z - .5F) * 2) * energy * 1.6F, colour, .35F + life * .65F, .035F + size * .045F);
    if (!p) break;
    p->gravity = 22; p->drag = .4F;
  }
}
void Game::dust(glm::vec3 position, int count, float speed, float size, glm::vec3 colour) {
  for (int i = 0; i < count; ++i) {
    const float a = random01() * 6.2831853F, s = speed * (.55F + .45F * random01()), up = random01(), life = random01(), scale = random01();
    const glm::vec3 out(std::cos(a), 0, std::sin(a));
    auto* p = spawn(ParticleKind::Dust, position + out * .14F, out * s + glm::vec3(0, .25F + .45F * up, 0), colour, .42F + .3F * life, size * (.7F + .6F * scale));
    if (!p) break;
    p->drag = 5.5F; p->gravity = -.35F; p->growth = 1.9F; p->turbulence = 1.2F;
  }
}
void Game::sparks(glm::vec3 position, glm::vec3 direction, glm::vec3 colour, int count, float speed, float spread, float drag) {
  for (int i = 0; i < count; ++i) {
    const float x = random01(), y = random01(), z = random01(), r = random01(), life = random01();
    const float s = speed * (.45F + .55F * r);
    glm::vec3 v = direction + glm::vec3(x - .5F, y - .5F, z - .5F) * 2.F * spread;
    v = glm::length(v) > .001F ? glm::normalize(v) * s : glm::vec3(0, s, 0);
    auto* p = spawn(ParticleKind::Spark, position, v, colour, .22F + .3F * life, .028F);
    if (!p) break;
    p->drag = drag; p->gravity = 18;
  }
}
void Game::shockwave(glm::vec3 position, glm::vec3 colour, float radius, float lifetime) {
  if (auto* p = spawn(ParticleKind::Ring, position, {}, colour, lifetime, .18F)) p->growth = radius;
}
void Game::flash(glm::vec3 position, glm::vec3 colour, float size, float lifetime) {
  spawn(ParticleKind::Glow, position, {}, colour, lifetime, size);
}
void Game::arc(glm::vec3 from, glm::vec3 to, glm::vec3 colour, float lifetime) {
  if (auto* p = spawn(ParticleKind::Arc, from, {}, colour, lifetime, .02F)) p->extent = to - from;
}
void Game::shockEffects(float dt) {
  // While electrocuted, small arcs keep crawling over the shell.
  if(player_.motion.shockAge>.75F) return;
  shockClock_+=dt; if(shockClock_<.04F) return; shockClock_=0;
  const auto centre=player_.position+glm::vec3(0,.9F,0);
  const float a=random01()*6.2831853F,h=random01(),r=random01(),v=random01();
  arc(centre+glm::vec3(std::cos(a)*.3F,(h-.5F)*.9F,std::sin(a)*.3F),centre+glm::vec3(std::cos(a+1.2F)*(.45F+.3F*r),(v-.5F)*1.1F,std::sin(a+1.2F)*(.45F+.3F*r)),{.45F,.8F,1},.07F);
  if(random01()<.5F) sparks(centre,{0,.5F,0},{.55F,.9F,1},2,3.F,1.4F);
}
void Game::impactBurst(glm::vec3 position, glm::vec3 colour, float radius, int rays, int sparkCount) {
  if (auto* p = spawn(ParticleKind::Shell, position, {}, colour, .3F, .15F)) p->growth = radius;
  for (int i = 0; i < rays; ++i) {
    // Spread over the upper hemisphere with a jittered spiral so rays never bunch.
    const float u = .15F + .85F * (float(i) + random01()) / float(rays), a = float(i) * 2.39996F + random01() * .4F, r = std::sqrt(1 - u * u * .6F);
    const glm::vec3 direction = glm::normalize(glm::vec3(std::cos(a) * r, u * .8F - .1F, std::sin(a) * r));
    if (auto* p = spawn(ParticleKind::Ray, position, {}, glm::mix(colour, glm::vec3(1), .35F + .3F * random01()), .2F + .1F * random01(), .05F + .03F * random01())) p->extent = direction * radius * (.8F + .5F * random01());
  }
  sparks(position, {0, .3F, 0}, colour, sparkCount, 15.F, 1.6F, 5.5F);
}
void Game::fireball(glm::vec3 position, glm::vec3 colour, float size, float growth, float lifetime) {
  if (auto* p = spawn(ParticleKind::Fireball, position, {0, .8F, 0}, colour, lifetime, size)) { p->growth = growth; p->drag = 2; p->gravity = -1.5F; p->turbulence = 1.5F; }
}
void Game::scorch(glm::vec3 position, float size, glm::vec3 colour) {
  if (auto* p = spawn(ParticleKind::Scorch, {position.x, 1.f, position.z}, {}, colour, 1.F, size)) p->growth = .25F;
}
void Game::pillar(glm::vec3 base, glm::vec3 colour, float lifetime) {
  spawn(ParticleKind::Pillar, base, {}, colour, lifetime, .9F);
}
void Game::smoke(glm::vec3 position, int count, float size, glm::vec3 colour) {
  // Slow, rising, swirling puffs that swell to several times their size.
  for (int i = 0; i < count; ++i) {
    const float x = random01(), z = random01(), up = random01(), life = random01(), scale = random01();
    auto* p = spawn(ParticleKind::Dust, position + glm::vec3(x - .5F, 0, z - .5F) * .4F, glm::vec3((x - .5F) * 1.2F, .9F + 1.2F * up, (z - .5F) * 1.2F), colour, .75F + .25F * life, size * (.7F + .6F * scale));
    if (!p) break;
    p->drag = 1.6F; p->gravity = -.5F; p->growth = 2.6F; p->turbulence = 3.F;
  }
}
void Game::smokePuff(glm::vec3 position, glm::vec3 velocity, float size, float lifetime) {
  const float tint = random01();
  if (auto* p = spawn(ParticleKind::Dust, position, velocity, glm::mix(glm::vec3(.74F, .77F, .80F), glm::vec3(.86F, .88F, .90F), tint), lifetime * (.8F + .4F * random01()), size * (.8F + .4F * random01()))) {
    p->drag = 2.4F; p->gravity = -.45F; p->growth = 3.F; p->turbulence = 2.5F;
  }
}
void Game::firework(glm::vec3 position, glm::vec3 colour) {
  // A shell bursts into a sphere of light streaks that droop as they fade.
  flash(position, glm::mix(colour, glm::vec3(1), .5F), 1.4F, .2F);
  for (int i = 0; i < 34; ++i) {
    const float u = random01() * 2 - 1, a = random01() * 6.2831853F, s = 5.F + 1.5F * random01(), r = std::sqrt(1 - u * u);
    if (auto* p = spawn(ParticleKind::Spark, position, glm::vec3(std::cos(a) * r, u, std::sin(a) * r) * s, colour, .7F + .25F * random01(), .032F)) { p->drag = 2.2F; p->gravity = 3.5F; }
  }
  embers(position, colour, 6, 1.5F);
}
void Game::embers(glm::vec3 position, glm::vec3 colour, int count, float speed) {
  // Kept short-lived: an ember from a late spark still dies within the one-second budget.
  for (int i = 0; i < count; ++i) {
    const float a = random01() * 6.2831853F, up = random01(), s = speed * (.4F + .6F * random01()), life = random01();
    auto* p = spawn(ParticleKind::Ember, position, glm::vec3(std::cos(a) * s, .6F + 1.4F * up, std::sin(a) * s), colour, .35F + .45F * life, .03F);
    if (!p) break;
    p->gravity = 1.2F; p->drag = 2.2F; p->turbulence = 2.5F;
  }
}
glm::vec3 Game::footPosition(bool right) const {
  // Local +X is the courier's right side; hips sit 0.17 units from the centre line.
  return player_.position + glm::vec3(std::cos(player_.yaw), 0, -std::sin(player_.yaw)) * (right ? .17F : -.17F) * modelScale;
}

}  // namespace relay
