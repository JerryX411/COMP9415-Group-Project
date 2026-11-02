#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "core/CourierMotion.h"


namespace relay {

struct Bounds {
  glm::vec3 min{}, max{};
  bool intersects(const Bounds& other) const;
};

class VoxelWorld {
 public:
  static constexpr int size = 64;
  std::uint8_t cell(int x, int y, int z) const;
  void set(int x, int y, int z, std::uint8_t material);
  void fill(int x0, int y0, int z0, int x1, int y1, int z1,
            std::uint8_t material);
  void clear();
  bool collides(const Bounds& box) const;
  bool collidesCylinder(glm::vec3 feet, float radius, float height) const;
  int solidCount() const;
 private:
  std::vector<std::uint8_t> cells_ = std::vector<std::uint8_t>(std::size_t(size) * size * size);
};

enum class RunState { Playing, Won, Lost };
enum class TrapPhase { Safe, Warning, Active, Disabled };
enum class Interaction { None, Core, Switch, Checkpoint, Portal };
// Moving hazards strike bluntly; live floor plates electrocute.
enum class DamageKind { Blunt, Electric };
struct Input {
  glm::vec2 movement{0.0F};  // World X/Z, normalized by the simulation.
  bool jumpPressed = false;
  bool interactPressed = false;
  bool sprintHeld = false;
  bool jumpHeld = false;
  bool crouchHeld = false;     // Crouch while held (scripts and tests).
  bool crouchPressed = false;  // Toggle crouch on this edge (the C key).
  bool sprintPressed = false;  // Sprint edge: cancels a slide straight into a run.
  bool wallPressed = false;    // Wall action (left mouse button): kick off or shove off a wall.
};
// Speed-pad states: a walk becomes a fast stride, a sprint becomes a low hover-flight.
enum class Boost : std::uint8_t { None, Stride, Flight };
struct Player {
  glm::vec3 position{7.0F, 1.0F, 24.0F};  // Centre of the feet.
  glm::vec3 velocity{0.0F};
  bool grounded = true;
  float yaw = 0.0F;
  float walkPhase = 0.0F;
  float hurtCooldown = 0.0F;
  float landingPulse = 0.0F;
  glm::vec2 knockback{0};
  glm::vec2 locomotion{0};
  float coyoteTime = 0, jumpBuffer = 0, jumpAge = 0;
  bool jumpActive = false, jumpCutQueued = false;
  // Simulation-driven pose weights keep transitions independent of render FPS.
  float moveBlend = 0, sprintBlend = 0, airBlend = 0, landingBlend = 0;
  float turnLean = 0, hitBlend = 0, idlePhase = 0;
  float hitStun = 0, interactionTime = 0, deathTime = 0;
  // Power-ups: seconds of spring boots left (higher jumps), a shield that absorbs one hit, and
  // jetpack charges (one burst each: sprint double-tapped while moving on the ground, or tapped
  // once mid-slide).
  float spring = 0;
  bool shielded = false;
  int jets = 0;
  int health = 5;
  bool sprinting = false;
  // Crouching lowers the collider; crouchBlend eases the pose in and out.
  bool crouching = false;
  float crouchBlend = 0;
  // Sliding is a crouch carried at speed; slideBlend eases the pose in and out.
  bool sliding = false;
  float slideBlend = 0;
  Boost boost = Boost::None;
  float strideBlend = 0, flightBlend = 0;  // Pose weights for the boost states.
  // Backpack glow, 0..1: soft while sprint is held (standing, running or sliding), full for the
  // whole of a speed-pad boost, off otherwise.
  float backGlow = 0;
  Interaction interaction = Interaction::None;
  glm::vec3 interactionTarget{};
  CourierMotion motion;
};
struct Core { glm::vec3 position{}; bool collected = false; };
// Beacon a courier activates with E; after a destroying blow it reboots at the last one.
struct Checkpoint {
  glm::vec3 position{};  // Pylon centre on the floor.
  glm::vec3 respawn{};   // Clear floor beside the pylon.
  bool active = false;
  float activeAge = 10;  // Seconds since activation (drives the boot-up animation).
};
// Floor strip that launches the courier along its arrow when stepped on.
// Floating power-ups: walk into one to take it; it comes back after Game::pickupRespawn. The
// clock (Time) adds Game::timeBonus seconds and is taken once per run of the level. The jetpack
// (Jet) gives one burst of speed.
enum class PowerUp : std::uint8_t { Spring, Shield, Time, Jet };
struct Pickup {
  glm::vec3 position{};  // On the floor; the item floats above it.
  PowerUp kind = PowerUp::Spring;
  float respawn = 0;  // Seconds until it is back (0: available).
  bool taken = false;  // A spent one-shot item (the clock): only its dark dock is left.
};
// Each power-up's colour: spring green, shield cyan, clock gold, jetpack orange.
inline glm::vec3 pickupColour(PowerUp kind) {
  return kind == PowerUp::Shield ? glm::vec3(.5F, .95F, 1) : kind == PowerUp::Time ? glm::vec3(1, .8F, .35F) : kind == PowerUp::Jet ? glm::vec3(1, .55F, .2F) : glm::vec3(.5F, 1, .6F);
}
// A pair of small round floor portals. Each end is activated with E; once both are, stepping into
// either carries the courier to the other. A pair is a shortcut that has to be opened from both
// ends (so usually from the far side first).
struct PortalLink {
  glm::vec3 from{}, to{};  // Centres, on the surface they stand on.
  bool fromOn = false, toOn = false;
  float pulse = 0;  // Fades after an activation or a jump (drives the flare).
  bool linked() const { return fromOn && toOn; }
};
struct SpeedPad {
  Bounds area{};
  glm::vec2 direction{1, 0};
  bool occupied = false;
  float flash = 0;  // Fades after a launch (drives the chevron burst).
};
// Crimson floor tile: standing on it costs Game::drainPenalty seconds of the countdown, and again
// every Game::drainInterval seconds while the courier keeps standing on crimson.
struct DrainTile {
  Bounds area{};
  float flash = 0;  // Fades after it bites (drives the red flare).
};
// Presentation: a crimson tile's dark-red flicker, 0..1 (about .75 most of the time), like a failing
// light: a stepped jitter, brief dips toward dark and the odd bright pop. seed keeps neighbouring
// cells (or tiles) out of step.
inline float crimsonFlicker(float time, float seed) {
  const auto hash = [](float n) { const float h = std::sin(n * 12.9898F) * 43758.5453F; return h - std::floor(h); };
  const float step = std::floor(time * 14.F), slow = std::floor(time * 3.F);
  const float jitter = hash(step * 1.7F + seed * 9.1F), dip = hash(slow * 3.3F + seed * 5.7F);
  float k = .68F + .14F * jitter;
  if (dip > .82F && jitter > .35F) k = .12F + .1F * jitter;  // a dropout toward dark
  else if (jitter > .93F) k = 1.F;                          // a pop
  return k;
}
// Floor button: standing on it opens its door, which stays open for doorHold seconds after stepping off.
struct PressurePlate {
  glm::vec3 position{};
  int door = 0;
  bool down = false;
  float press = 0;  // Eased 0..1 sink.
};
// Split blast door filling a wall opening; the two halves slide apart into the wall.
struct BlastDoor {
  glm::vec3 centre{}, half{};  // Closed panel volume.
  bool alongZ = false;         // The door spans z (else x).
  float open = 0, hold = 0;    // 0 sealed .. 1 open; seconds left before it closes.
  Bounds bounds() const { return {centre - half, centre + half}; }
  std::array<Bounds, 2> panels() const;
};
struct FloorTrap {
  Bounds area{};
  float cycleOffset = 0.0F;
  float disabledFor = 0.0F;
  static constexpr float tileWidth = .92F, surfaceHeight = .70F;
  Bounds tileBounds(float x, float z) const;
  bool touchesFeet(glm::vec3 feet, float radius) const;
};
struct RelaySwitch {
  glm::vec3 position{};
  int trap = 0;
  float animation = 0.0F;
};
struct MovingHazard {
  glm::vec3 start{}, end{}, position{}, halfSize{0.5F};
  float period = 5.0F;
  float offset = 0.0F;
  std::array<Bounds,3> collisionParts() const;
  Bounds bounds() const;
  bool intersectsPlayer(glm::vec3 feet, float radius, float height) const;
};
// Debris is lit and solid; the rest are soft effects. Embers are what is left
// of a spark after it bounces; arcs are crackling bolts from position to position + extent.
// Fireballs swell and cool, scorch marks are floor decals and pillars are columns of light.
// Shells are expanding shockwave spheres; rays are comic-style light streaks fired outward.
enum class ParticleKind : std::uint8_t { Debris, Dust, Spark, Glow, Ring, Ember, Arc, Fireball, Scorch, Pillar, Shell, Ray };
struct Particle {
  glm::vec3 position{}, velocity{}, colour{};
  glm::vec3 extent{};  // Arcs and rays: their span. Dust: extent.x > 0 makes it heavy (denser).
                       // Debris: extent.x > 0 makes it a glowing glass shard (a shield's).
  float remaining = 0.0F;
  float lifetime = 1.0F;
  float size = 0.06F;
  // Drag is per second, gravity may be negative (rising smoke); growth widens
  // dust over its life and is the final radius of a ring.
  // Debris with growth > 0 is confetti: flat, fluttering paper.
  float drag = 0.0F, gravity = 5.0F, growth = 0.0F;
  float turbulence = 0.0F, seed = 0.0F;
  ParticleKind kind = ParticleKind::Debris;
  bool bounced = false;
};


// Reduced simulation: basic locomotion, switches and two hazards only.
class Game {
 public:
 static constexpr float playerRadius=.70F, playerHeight=1.70F, modelScale=.8F;
 static constexpr float crouchHeight=.95F,crouchSpeed=2.6F,speed=5.2F;
 static constexpr float gravity=26,fallGravity=36,jumpSpeed=9.6F,footRadius=.30F;
 static constexpr float coyoteDuration=.10F,jumpBufferDuration=.12F;
 // Retained pose calibration constants; no sprint or pad gameplay in this edition.
 static constexpr float sprintSpeed=8.4F,padSpeed=14;
 static constexpr bool missionsUnlocked=true;
 Game(); void reset(int level); void update(float seconds,const Input& input);
 Bounds playerBounds() const;
 bool collidesWithScene(const Bounds& box) const{return blocked(box);}
 bool playerIntersectsScene() const{return blockedPlayer(player_.position);}
 TrapPhase trapPhase(std::size_t index) const; float trapPhaseRemaining(std::size_t index) const;
 int nearestCore() const;
 glm::vec3 gatePosition() const{return {32,1,7};}
 float gateOpen() const{return gateOpen_;}
 const char* tutorialHint() const;
 int nearestSwitch() const; Interaction availableInteraction() const;
 float shadowHeight() const; const char* levelName() const{return nameOf(level_);} static const char* nameOf(int level);
 float bodyHeight() const{return player_.crouching?crouchHeight:playerHeight;}
 const VoxelWorld& world() const{return world_;} const Player& player() const{return player_;}
 const std::vector<FloorTrap>& traps() const{return traps_;}
 const std::vector<RelaySwitch>& switches() const{return switches_;}
 const std::vector<MovingHazard>& movers() const{return movers_;}
 RunState state() const{return state_;} int level() const{return level_;} int revision() const{return revision_;}
 float elapsed() const{return elapsed_;} Bounds room() const{return room_;} glm::vec3 spawnPoint() const{return spawn_;}
 const std::string& message() const{return message_;} float messageTime() const{return messageTime_;}
 void warpPlayer(glm::vec3 feet);
 // Empty legacy collections let structural regression tests assert removal.
 const std::vector<Core>& cores() const{return cores_;} const std::vector<Checkpoint>& checkpoints() const{return checkpoints_;}
 const std::vector<PortalLink>& portals() const{return portals_;} const std::vector<Pickup>& pickups() const{return pickups_;}
 const std::vector<SpeedPad>& speedPads() const{return pads_;} const std::vector<BlastDoor>& doors() const{return doors_;}
 int collectedCount() const; int liveParticleCount() const; bool gateUnlocked() const; float boostMeter() const;
 bool jetting() const{return false;}
 private:
 void step(float dt,const Input& input); void moveAxis(int axis,float distance); void buildShell();
 bool blocked(const Bounds& box,int ignoreMover=-1) const; bool blockedPlayer(glm::vec3 feet,int ignoreMover=-1) const;
 const std::vector<Bounds>& fixtures() const; mutable std::vector<Bounds> fixtures_;
 void updateAnimation(float dt,glm::vec3 previous,float turn);
 void updateMovers(float dt); bool separateFromMover(std::size_t index);
 void updateInteraction(float dt,bool pressed);
 void applyDamage(glm::vec3 source,int sourceId,bool launch=true,DamageKind kind=DamageKind::Blunt);
 VoxelWorld world_; Player player_; RunState state_=RunState::Playing;
 int level_=0,revision_=0,wallHeight_=3,interactionIndex_=-1;
 bool crouchToggle_=false,interactionCommitted_=false;
 Bounds room_{{4,1,4},{36,4,36}}; glm::vec3 spawn_{8,1,25};
 float elapsed_=0,remaining_=0,gateOpen_=0,hitStop_=0,messageTime_=0;
 std::string message_;
 std::vector<FloorTrap> traps_; std::vector<RelaySwitch> switches_; std::vector<MovingHazard> movers_;
 std::vector<Core> cores_; std::vector<Checkpoint> checkpoints_; std::vector<PortalLink> portals_;
 std::vector<Pickup> pickups_; std::vector<SpeedPad> pads_; std::vector<BlastDoor> doors_;
 std::array<Particle,1> particles_{};
};
} // namespace relay
