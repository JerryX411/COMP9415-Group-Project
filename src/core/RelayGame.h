#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "core/CourierMotion.h"
#include "core/LevelData.h"

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
  static constexpr float tileWidth = .92F, surfaceHeight = .1075F;
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

class Game {
 public:
  // Rounded body collision; floor damage uses a separate foot contact patch.
  static constexpr float playerRadius = 0.70F;
  // The courier model is drawn at 80% (about 1.6 units tall, above one voxel); the
  // collider and step cadence follow it.
  static constexpr float modelScale = .8F;
  static constexpr float playerHeight = 1.70F;
  // Crouched, the courier fits under a single voxel (1 unit) with headroom to spare.
  static constexpr float crouchHeight = .95F, crouchSpeed = 2.6F;
  static constexpr float footRadius = .30F;
  static constexpr float speed = 5.2F;
  static constexpr float sprintSpeed = 8.4F;
  // Slide (Apex-style): crouch at sprint speed. The first slide after a cool-down adds a boost,
  // then friction and drag bleed the speed off over about a second; input only bends the line.
  static constexpr float slideStartSpeed = speed + .6F, slideEndSpeed = crouchSpeed + .6F;
  static constexpr float slideBoost = 4.5F, slideBoostCooldown = 2.2F;  // longer than a boosted slide, so boosts cannot chain
  // Surge: right after a boost the slide barely slows (friction x surgeFriction) for a moment,
  // so the burst is felt; a slide onto a speed pad gets a longer turbo surge.
  static constexpr float slideSurgeTime = .3F, padSurgeTime = .65F, surgeFriction = .3F;
  static constexpr float slideFriction = 3.6F, slideDrag = .42F, slideSteer = 1.7F;
  // In the air, speed above the run speed (a slide jump) bleeds off this slowly.
  static constexpr float airCarryDrag = 1.5F;
  static constexpr float gravity = 26.0F, fallGravity = 36.0F;
  static constexpr float jumpSpeed = 9.6F;
  static constexpr float coyoteDuration = .10F, jumpBufferDuration = .12F;
  static constexpr float switchDuration = 5.0F;
  static constexpr float hazardPenalty = 5.0F;
  static constexpr std::size_t particleCapacity = 1024;

  Game();
  void reset(int level);
  void update(float seconds, const Input& input);
  Bounds playerBounds() const;
  bool collidesWithScene(const Bounds& box) const { return blocked(box); }
  bool playerIntersectsScene() const { return blockedPlayer(player_.position); }
  TrapPhase trapPhase(std::size_t index) const;
  float trapPhaseRemaining(std::size_t index) const;
  int nearestSwitch() const;
  int nearestCheckpoint() const;
  int nearestCore() const;
  Interaction availableInteraction() const;
  int collectedCount() const;
  int liveParticleCount() const;
  float shadowHeight() const;
  bool gateUnlocked() const;
  const char* levelName() const { return nameOf(level_); }
  static const char* nameOf(int level);
  // Collider height: standing or crouched.
  float bodyHeight() const { return player_.crouching ? crouchHeight : playerHeight; }

  const VoxelWorld& world() const { return world_; }
  const Player& player() const { return player_; }
  const std::vector<Core>& cores() const { return cores_; }
  const std::vector<FloorTrap>& traps() const { return traps_; }
  const std::vector<RelaySwitch>& switches() const { return switches_; }
  const std::vector<Checkpoint>& checkpoints() const { return checkpoints_; }
  const std::vector<SpeedPad>& speedPads() const { return pads_; }
  const std::vector<Pickup>& pickups() const { return pickups_; }
  const std::vector<PressurePlate>& plates() const { return plates_; }
  const std::vector<BlastDoor>& doors() const { return doors_; }
  const std::vector<PortalLink>& portals() const { return portals_; }
  // The nearest inactive portal end within reach, as link * 2 + end (0 from, 1 to), or -1.
  int nearestPortal() const;
  glm::vec3 portalEnd(int code) const;
  // Portal jumps and jetpack bursts so far this level.
  int teleports() const { return teleports_; }
  int jetsUsed() const { return jetsUsed_; }
  // A jetpack burst's boost is still running (the yellow shake, flames and streaks last as long).
  bool jetting() const { return jetting_; }
  // The top of whatever is built at a floor spot: the lowest empty layer of its column (1 on open
  // floor, on top of a raised platform, still 1 inside a tunnel).
  float surfaceAt(float x, float z) const;
  int lives() const { return lives_; }
  // Seconds left on the speed-pad flight clock. It runs whether or not the courier is flying:
  // a flight interrupted by an interaction resumes after it, and sprint takes off again.
  float boostTimeLeft() const { return boostTime_; }
  // How much speed-pad boost is left, 0..1: a flight's clock, a stride's extra speed, or the pad
  // speed a slide (or a jump or run out of one) still carries. Drives the HUD meter and jets.
  float boostMeter() const;
  // The last life is gone: the shutdown plays out, then the level restarts.
  bool outOfLives() const { return state_ == RunState::Lost && player_.health <= 0 && lives_ == 0; }
  // Destroyed with time to spare: the courier reboots at the last checkpoint (or the start).
  static constexpr float respawnDelay = 2.6F, respawnPenalty = 10.F;
  static constexpr float sameSourceImmunity = .5F;
  // Each level gives three lives; losing the last restarts the level.
  static constexpr int livesPerLevel = 3;
  // Spring boots raise jump speed by springJump (about 2.5 units high instead of 1.7: over a
  // two-block wall, still short of the three-block outer walls) for springTime seconds.
  // Pickups come back after pickupRespawn.
  static constexpr float springTime = 12.F, springJump = 1.2F, pickupRespawn = 20.F, pickupRadius = .9F;
  // Seconds the clock pickup adds to a mission's countdown.
  static constexpr float timeBonus = 30.F;
  // Crimson tiles: seconds taken per bite, and the least time between two bites.
  static constexpr float drainPenalty = 5.F, drainInterval = 1.F;
  // Level 2 is the training ground: no clock, unlimited reboots, guided steps. Missions
  // (levels 0 and 1) stay locked in the menu until they are reopened.
  static constexpr int tutorialLevel = 2, tutorialSteps = 25;
  // Level 3 is the practice field (from the main menu): a big open floor with every power-up in
  // one place (each back after practiceRespawn) and a speed-pad runway. No clock, no exit, unlimited
  // reboots.
  static constexpr int practiceLevel = 3;
  static constexpr float practiceRespawn = 1.F;
  static constexpr bool missionsUnlocked = false;
  // Running into a wall faster than bounceSpeed rebounds; steering waits bounceLock seconds.
  static constexpr float bounceSpeed = 4.5F, bounceRestitution = .5F, bounceLock = .22F;
  // Boosted wall jump: jump within wallGrace of hitting a wall to kick off it at wallKickSpeed
  // or more. Steering waits wallKickLock; the hop never climbs above a floor jump's apex (plus
  // a little), and only wallJumpsPerAir are allowed before landing, so walls cannot be climbed.
  static constexpr float wallGrace = .16F, wallKickSpeed = 9.F, wallKickLock = .24F;
  static constexpr int wallJumpsPerAir = 2;
  // Wall action (left mouse): pressed while heading at a wall at wallAngle (45 degrees) or more
  // and within wallTrigger of it (beyond the body), it fires on reaching wallReach of it (at once
  // if already there; otherwise it waits for the courier to arrive, up to wallBuffer). Flying,
  // sliding, airborne or sprinting kicks off it straight back at the same speed with a flip (a
  // kind for each); walking or standing just shoves away from it at wallShove.
  // wallActionCooldown between uses.
  static constexpr float wallReach = .6F, wallTrigger = 2.F, wallAngle = .7071F, wallBuffer = .6F;
  static constexpr float wallShove = 4.2F, wallActionCooldown = .25F;
  // A kick rebounds at wallRebound times the arrival speed (capped at wallReboundMax) and snaps
  // wallSnap off the wall at once; the extra speed then bleeds off as carried speed does.
  static constexpr float wallRebound = 1.2F, wallReboundMax = 15.6F, wallSnap = .24F;
  // Speed pads launch to at least padSpeed. Speed above the run speed then bleeds off at
  // groundCarryDrag while steering ahead (coastDrag with no input).
  static constexpr float padSpeed = 14.F, groundCarryDrag = 5.F, coastDrag = 10.F;
  // Pad states: a stride launches to strideSpeed and bleeds slowly back to a walk. A flight
  // launches to padSpeed, hovers over floor traps and lasts boostDuration seconds whatever the
  // stick does: steering (even straight back) only turns it, at up to flightTurn rad/s. A slide
  // across a pad gets padSpeed + slidePadBonus.
  static constexpr float strideSpeed = 9.F, strideDrag = 1.6F, flightDrag = 2.F;
  // boostDuration is the speed-pad boost clock: it runs through every move (flight, stride,
  // run, slide, jumps), holding the speed and the boost effects until it runs out.
  static constexpr float boostDuration = 2.4F, flightTurn = 3.2F, boostHoldDrag = .6F;
  static constexpr float flightTakeoff = 12.5F, slidePadBonus = 3.5F;
  static constexpr float doorHold = 4.F, plateRadius = .75F;
  // A jetpack burst throws a run forward to at least jetSpeed (a slide gets the speed-pad turbo);
  // the boost clock then runs as after a pad. A portal catches the courier within portalRadius of
  // its centre; after a jump the pair waits portalCooldown, and the arrival end stays quiet until
  // the courier has stepped off it.
  static constexpr float jetSpeed = 15.F, portalCooldown = 1.F, portalRadius = .7F;
  // Two sprint presses within doubleTap seconds fire a jetpack charge.
  static constexpr float doubleTap = .3F;
  bool respawning() const { return state_ == RunState::Lost && respawnPending_; }
  int activeCheckpoint() const { return respawnIndex_; }
  // Quick-menu teleport to the last checkpoint (or the entry point): keeps armour and time.
  void returnToCheckpoint();
  const std::vector<MovingHazard>& movers() const { return movers_; }
  const std::array<Particle, particleCapacity>& particles() const { return particles_; }
  RunState state() const { return state_; }
  int level() const { return level_; }
  int revision() const { return revision_; }
  float elapsed() const { return elapsed_; }
  float timeRemaining() const { return remaining_; }
  // Seconds since a clock pickup last added time (the HUD shows +30 S while it is fresh).
  float timeBonusAge() const { return bonusAge_; }
  // Seconds since a crimson tile last took time (the HUD shows -5 S while it is fresh).
  float timePenaltyAge() const { return penaltyAge_; }
  const std::vector<DrainTile>& drainTiles() const { return drains_; }
  float gateOpen() const { return gateOpen_; }
  glm::vec3 gatePosition() const { return gate_; }
  glm::vec3 spawnPoint() const { return spawn_; }
  // Floor area inside the outer walls.
  Bounds room() const { return room_; }
  // Edited maps: when set (the game sets it, tests do not), reset() loads
  // assets/levels/levelN.txt, written by RelayEditor, in place of the built-in layout.
  static inline bool useLevelFiles = false;
  // The current level's layout (right after reset) as the editor and level files see it.
  LevelData captureLevel() const;
  // Replace the layout with a level file's (walls, items, spawn, gate, clock).
  void applyLevel(const LevelData& level);
  bool tutorial() const { return level_ == tutorialLevel; }
  bool practice() const { return level_ == practiceLevel; }
  bool timed() const { return level_ < tutorialLevel; }
  // How long a taken power-up stays away (the clock included, on the practice field).
  float pickupRespawnTime() const { return practice() ? practiceRespawn : pickupRespawn; }
  // Training: the current step (0..tutorialSteps-1), how long it has been shown, and where it points.
  int tutorialStep() const { return tutorialStep_; }
  float tutorialStepAge() const { return tutorialAge_; }
  glm::vec3 tutorialTarget() const;
  // What the current training step asks for, in a few words (shown at its beacon).
  const char* tutorialHint() const;
  // Boosted wall jumps made since the level started (training teaches one).
  int wallJumpsDone() const { return wallJumpsDone_; }
  const std::string& message() const { return message_; }
  float messageTime() const { return messageTime_; }

  // Explicit whitebox controls, also used to reproduce collision regressions.
  void warpPlayer(glm::vec3 feet);
  void setTimeForDebug(float seconds);
  void setBoostForDebug(float seconds) { boostTime_ = seconds; }
  void setJetsForDebug(int charges) { player_.jets = charges; }
  void linkPortalsForDebug() { for (auto& p : portals_) p.fromOn = p.toOn = true; }
  // True just after touching a wall while moving into it (the wall-jump window).
  bool wallTouching() const { return wallTouch_ > 0; }
  void setVoxelForDebug(int x, int y, int z, std::uint8_t material) { world_.set(x, y, z, material); ++revision_; }
  // Arrival-scene clock: advances effects while gameplay stays frozen.
  void present(float seconds, float wakeTime);
  // Impact freeze requested by the last hit or pickup; reading clears it.
  float takeHitStop() { const float t = hitStop_; hitStop_ = 0; return t; }

 private:
  void step(float dt, const Input& input);
  void moveAxis(int axis, float distance);
  bool blocked(const Bounds& bounds, int ignoreMover = -1) const;
  bool blockedPlayer(glm::vec3 feet, int ignoreMover = -1) const;
  const std::vector<Bounds>& fixtures() const;
  mutable std::vector<Bounds> fixtures_;
  void updateAnimation(float dt, glm::vec3 previousPosition, float turn);
  // sourceId names the hazard (trap index, 100 + mover index, 200 for a fall). Only the
  // same source is held off, for sameSourceImmunity; a different hazard hits at once.
  void applyDamage(glm::vec3 source, int sourceId, bool launch = true, DamageKind kind = DamageKind::Blunt);
  void shockEffects(float dt);
  void updateMovers(float dt);
  bool separateFromMover(std::size_t index);
  void updateInteraction(float dt, bool pressed);
  void updateParticles(float dt);
  void beginDeath();
  void burst(glm::vec3 position, glm::vec3 colour, int count, float energy);
  Particle* spawn(ParticleKind kind, glm::vec3 position, glm::vec3 velocity,
                  glm::vec3 colour, float lifetime, float size);
  void dust(glm::vec3 position, int count, float speed, float size, glm::vec3 colour = {.80F, .84F, .85F});
  void sparks(glm::vec3 position, glm::vec3 direction, glm::vec3 colour, int count, float speed, float spread, float drag = 1.6F);
  // Impact burst: shockwave shell, radiating rays and a fast spark blast that hangs in the air.
  void impactBurst(glm::vec3 position, glm::vec3 colour, float radius, int rays, int sparkCount);
  void shockwave(glm::vec3 position, glm::vec3 colour, float radius, float lifetime);
  void flash(glm::vec3 position, glm::vec3 colour, float size, float lifetime);
  void arc(glm::vec3 from, glm::vec3 to, glm::vec3 colour, float lifetime);
  void embers(glm::vec3 position, glm::vec3 colour, int count, float speed);
  void fireball(glm::vec3 position, glm::vec3 colour, float size, float growth, float lifetime);
  void scorch(glm::vec3 position, float size, glm::vec3 colour);
  void pillar(glm::vec3 base, glm::vec3 colour, float lifetime);
  void smoke(glm::vec3 position, int count, float size, glm::vec3 colour);
  // One soft grey puff thrown along velocity that swells, swirls and rises (slide smoke).
  void smokePuff(glm::vec3 position, glm::vec3 velocity, float size, float lifetime);
  void firework(glm::vec3 position, glm::vec3 colour);
  glm::vec3 footPosition(bool right) const;
  void notify(std::string message);
  void standUp();
  void startSlide();
  void startTrick(int kind);
  void wallJump();
  // Wall action: find a wall ahead (its outward normal and contact point), then kick or shove.
  bool findWall(glm::vec2 direction, float reach, glm::vec2& normal, glm::vec3& point, float& distance) const;
  glm::vec2 wallHeading() const;
  void wallAction();
  void wallActionAt(glm::vec2 normal, glm::vec3 point);
  void wallImpact(glm::vec2 out, glm::vec3 point);
  // refresh restarts the flight clock (a pad, a take-off from a stride); a resume keeps it.
  void startFlight(glm::vec2 direction, float speed, bool refresh = true);
  void endBoost(bool touchdown);
  void boostEffects(float dt);
  void updateTutorial(float dt);
  bool tutorialStepDone(int step) const;
  void popUp(bool sprintCancel);
  void drainBite();
  void jetBurst();
  void activatePortal(int code);
  void updatePortals(float dt);
  void updateDevices(float dt);
  void updatePickups(float dt);
  void speedTrail(float dt);
  void slideEffects(float dt);
  void activateCheckpoint(std::size_t index);
  void respawn();
  void beamIn();
  glm::vec3 rebootPosition() const;
  float random01();

  VoxelWorld world_;
  Player player_;
  std::vector<Core> cores_;
  std::vector<FloorTrap> traps_;
  std::vector<RelaySwitch> switches_;
  std::vector<MovingHazard> movers_;
  std::array<Particle, particleCapacity> particles_{};
  RunState state_ = RunState::Playing;
  int level_ = 0;
  int revision_ = 0;
  // Layout anchors: floor inside the outer walls, spawn (and facing), exit gate, starting clock.
  Bounds room_{{4, 1, 4}, {28, 4, 28}};
  glm::vec3 spawn_{7, 1, 24}, gate_{25, 1, 7};
  float spawnYaw_ = 0, startTime_ = 150;
  int wallHeight_ = 3;  // Outer walls around the room.
  void buildShell();
  float elapsed_ = 0.0F, remaining_ = 150.0F, gateOpen_ = 0.0F, bonusAge_ = 10.0F;
  float sparkClock_ = 0.0F, messageTime_ = 0.0F;
  float hitStop_ = 0.0F, exhaustClock_ = 0.0F, lastWake_ = -1.0F;
  bool crouchToggle_ = false;
  std::vector<Checkpoint> checkpoints_;
  int respawnIndex_ = -1;
  bool respawnPending_ = false;
  int lastHitSource_ = -1;
  int lives_ = livesPerLevel;
  int tutorialStep_ = 0;
  float tutorialAge_ = 0.0F, bounceTime_ = 0.0F, bounceCooldown_ = 0.0F, jetClock_ = 0.0F, boostTime_ = 0.0F;
  bool resumeFlight_ = false;
  float slideSurge_ = 0.0F, slideSurgeLength_ = 1.0F;  // An interaction paused the flight; it lifts off again afterwards.
  float wallTouch_ = 0, wallSpeed_ = 0, wallKick_ = 0, groundY_ = 1;
  int wallJumpsDone_ = 0;
  glm::vec2 wallNormal_{0};
  glm::vec3 wallPoint_{0};
  int wallJumps_ = 0;
  bool carrying_ = false;  // Boosted speed is bleeding off (see groundCarryDrag).
  float trailClock_ = 0.0F;
  std::vector<SpeedPad> pads_;
  std::vector<DrainTile> drains_;
  float drainCooldown_ = 0, penaltyAge_ = 10;
  std::vector<Pickup> pickups_;
  std::vector<PressurePlate> plates_;
  std::vector<BlastDoor> doors_;
  std::vector<PortalLink> portals_;
  int teleports_ = 0, jetsUsed_ = 0;
  float portalWait_ = 0;
  int portalHold_ = -1;  // The end just arrived at (link * 2 + end), quiet until stepped off.
  bool jetting_ = false;
  float wallActionWait_ = 0, wallQueue_ = 0;
  glm::vec2 wallQueueNormal_{0};  // A pressed wall action waiting to reach its wall.
  float lastSprintTap_ = -10;  // elapsed_ at the last sprint press (for the double tap).
  float jetTrailClock_ = 0;
  float sourceImmunity_ = 0.0F, graceTime_ = 0.0F;
  float skidClock_ = 0.0F, shockClock_ = 0.0F, slideClock_ = 0.0F, slideCooldown_ = 0.0F, runDustClock_ = 0.0F;
  std::uint32_t random_ = 0x52454c41;
  std::string message_;
  int interactionIndex_ = -1;
  bool interactionCommitted_ = false;
};

}  // namespace relay
