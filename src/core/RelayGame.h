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
  static constexpr int size = 32;
  std::uint8_t cell(int x, int y, int z) const;
  void set(int x, int y, int z, std::uint8_t material);
  void fill(int x0, int y0, int z0, int x1, int y1, int z1,
            std::uint8_t material);
  void clear();
  bool collides(const Bounds& box) const;
  bool collidesCylinder(glm::vec3 feet, float radius, float height) const;
  int solidCount() const;
 private:
  std::array<std::uint8_t, size * size * size> cells_{};
};

enum class RunState { Playing, Won, Lost };
enum class TrapPhase { Safe, Warning, Active, Disabled };
enum class Interaction { None, Core, Switch };
struct Input {
  glm::vec2 movement{0.0F};  // World X/Z, normalized by the simulation.
  bool jumpPressed = false;
  bool interactPressed = false;
  bool sprintHeld = false;
  bool jumpHeld = false;
};
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
  int health = 5;
  bool sprinting = false;
  Interaction interaction = Interaction::None;
  glm::vec3 interactionTarget{};
  CourierMotion motion;
};
struct Core { glm::vec3 position{}; bool collected = false; };
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
struct Particle {
  glm::vec3 position{}, velocity{}, colour{};
  float remaining = 0.0F;
  float lifetime = 1.0F;
  float size = 0.06F;
};

class Game {
 public:
  // Rounded body collision; floor damage uses a separate foot contact patch.
  static constexpr float playerRadius = 0.70F;
  static constexpr float playerHeight = 2.10F;
  static constexpr float footRadius = .30F;
  static constexpr float speed = 5.2F;
  static constexpr float sprintSpeed = 8.4F;
  static constexpr float gravity = 26.0F, fallGravity = 36.0F;
  static constexpr float jumpSpeed = 9.6F;
  static constexpr float coyoteDuration = .10F, jumpBufferDuration = .12F;
  static constexpr float switchDuration = 5.0F;
  static constexpr float hazardPenalty = 5.0F;
  static constexpr std::size_t particleCapacity = 384;

  Game();
  void reset(int level);
  void update(float seconds, const Input& input);
  Bounds playerBounds() const;
  bool collidesWithScene(const Bounds& box) const { return blocked(box); }
  bool playerIntersectsScene() const { return blockedPlayer(player_.position); }
  TrapPhase trapPhase(std::size_t index) const;
  float trapPhaseRemaining(std::size_t index) const;
  int nearestSwitch() const;
  int nearestCore() const;
  Interaction availableInteraction() const;
  int collectedCount() const;
  int liveParticleCount() const;
  float shadowHeight() const;
  bool gateUnlocked() const;
  const char* levelName() const;

  const VoxelWorld& world() const { return world_; }
  const Player& player() const { return player_; }
  const std::vector<Core>& cores() const { return cores_; }
  const std::vector<FloorTrap>& traps() const { return traps_; }
  const std::vector<RelaySwitch>& switches() const { return switches_; }
  const std::vector<MovingHazard>& movers() const { return movers_; }
  const std::array<Particle, particleCapacity>& particles() const { return particles_; }
  RunState state() const { return state_; }
  int level() const { return level_; }
  int revision() const { return revision_; }
  float elapsed() const { return elapsed_; }
  float timeRemaining() const { return remaining_; }
  float gateOpen() const { return gateOpen_; }
  glm::vec3 gatePosition() const { return {25.0F, 1.0F, 7.0F}; }
  const std::string& message() const { return message_; }
  float messageTime() const { return messageTime_; }

  // Explicit whitebox controls, also used to reproduce collision regressions.
  void warpPlayer(glm::vec3 feet);
  void setTimeForDebug(float seconds);

 private:
  void step(float dt, const Input& input);
  void moveAxis(int axis, float distance);
  bool blocked(const Bounds& bounds, int ignoreMover = -1) const;
  bool blockedPlayer(glm::vec3 feet, int ignoreMover = -1) const;
  std::vector<Bounds> fixtures() const;
  void updateAnimation(float dt, glm::vec3 previousPosition, float turn);
  void applyDamage(glm::vec3 source, bool launch = true);
  void updateMovers(float dt);
  bool separateFromMover(std::size_t index);
  void updateInteraction(float dt, bool pressed);
  void updateParticles(float dt);
  void beginDeath();
  void burst(glm::vec3 position, glm::vec3 colour, int count, float energy);
  void notify(std::string message);
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
  float elapsed_ = 0.0F, remaining_ = 150.0F, gateOpen_ = 0.0F;
  float sparkClock_ = 0.0F, messageTime_ = 0.0F;
  std::uint32_t random_ = 0x52454c41;
  std::string message_;
  int interactionIndex_ = -1;
  bool interactionCommitted_ = false;
};

}  // namespace relay
