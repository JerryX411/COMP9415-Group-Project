// Corona Run (Lite level 3): structure checks and a full run with ordinary controls, side branches
// (jump pad perch, stepping stones, the long beam, the loop) included.
// The route in tests/corona_route.txt follows the authored Lite map.
// The bot only presses keys: it runs, sprints, crouches and jumps at lips and steps on its own, and
// waits for shock strips and sweeping blocks the way a careful player would. It must finish the
// run without falling into the void, inside the time limit.
#include "core/RelayGame.h"
#include "graphics/RelayModels.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr float dt = 1.F / 120;
constexpr float pi = 3.14159265F;
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }

struct Bot {
  relay::Game& g;
  int frames = 0;
  std::string where() const {
    const auto p = g.player().position; std::ostringstream s; s << " at " << p.x << ',' << p.y << ',' << p.z << " t=" << g.elapsed();
    return s.str();
  }
  void tick(relay::Input in) {
    const int falls = g.falls(), health = g.player().health;
    g.update(dt, in); ++frames;
    check(g.falls() == falls, "fell into the void" + where());
    check(g.player().health == health, "took damage" + where());
    check(g.state() != relay::RunState::Lost, "lost the run" + where());
  }
  void idle(float seconds) { for (int i = 0; i < int(seconds / dt); ++i) tick({}); }
  bool solid(float x, int y, float z) const { return g.world().cell(int(std::floor(x)), y, int(std::floor(z))) != 0; }
  // Jump at the last moment over a gap (the cell under the centre is gone but the 0.7 radius
  // still stands on the lip), or onto a one-block step just ahead.
  bool wantsJump(glm::vec2 dir) const {
    const auto& p = g.player();
    if (!p.grounded) return false;
    const int fy = int(std::floor(p.position.y + .01F));
    if (!solid(p.position.x, fy - 1, p.position.z)) return true;
    const float ax = p.position.x + dir.x * .95F, az = p.position.z + dir.y * .95F;
    return solid(ax, fy, az) && !solid(ax, fy + 1, az) && !solid(ax, fy + 2, az);
  }
  void go(glm::vec2 target, char mode, std::optional<float> takeoffZ = {}) {
    bool jumpRequested = false;
    float best = 1e9F; int stall = 0;
    for (int i = 0; i < 120 * 30; ++i) {
      const auto p = g.player().position;
      const glm::vec2 delta = target - glm::vec2(p.x, p.z);
      const float distance = glm::length(delta);
      if (distance < .22F && g.player().grounded) return;
      const glm::vec2 dir = delta / std::max(distance, 1e-4F);
      relay::Input in;
      in.movement = dir * std::min(1.F, distance * 2.5F);
      in.sprintHeld = mode == 's' && distance > 1.2F;
      in.crouchHeld = mode == 'c';
      in.jumpHeld = mode != 'c';
      // For the low beam, delay the jump until just past the lip. The normal
      // coyote-time window lets the courier take off without hitting the beam early.
      in.jumpPressed = takeoffZ ? !jumpRequested && p.z <= *takeoffZ
                                : mode != 'c' && wantsJump(dir);
      jumpRequested = jumpRequested || in.jumpPressed;
      tick(in);
      if (distance < best - .02F) { best = distance; stall = 0; } else if (++stall > 360) break;
    }
    throw std::runtime_error("stuck on the way to " + std::to_string(target.x) + "," + std::to_string(target.y) + where());
  }
  void waitTrap(int i, float need) {
    for (int n = 0; n < 120 * 20; ++n) {
      if (g.trapPhase(std::size_t(i)) == relay::TrapPhase::Safe && g.trapPhaseRemaining(std::size_t(i)) >= need) return;
      tick({});
    }
    throw std::runtime_error("trap never safe" + where());
  }
  glm::vec3 moverAt(const relay::MovingHazard& m, float t) const {
    return glm::mix(m.start, m.end, (1 - std::cos((t + m.offset) * 2 * pi / m.period)) * .5F);
  }
  // A sweeper crossing the path: wait until it stays clear of the path's centre line (on the
  // axis it moves along) for the crossing.
  void waitMover(int j, char axis, float lane, float clearance) {
    const auto& m = g.movers().at(std::size_t(j));
    for (int n = 0; n < 120 * 20; ++n) {
      bool clear = true;
      for (float tau = 0; tau <= .7F && clear; tau += .02F) {
        const auto at = moverAt(m, g.elapsed() + tau);
        if (std::abs((axis == 'x' ? at.x : at.z) - lane) < clearance) clear = false;
      }
      if (clear) return;
      tick({});
    }
    throw std::runtime_error("sweeper never clear" + where());
  }
  // A block sliding along the path: wait until it comes toward the courier, then run at it and
  // jump over it. Works along either axis.
  void jumpMover(int j) {
    const auto& m = g.movers().at(std::size_t(j));
    const glm::vec2 travel(m.end.x - m.start.x, m.end.z - m.start.z);
    const bool alongX = std::abs(travel.x) > std::abs(travel.y);
    const glm::vec2 d = alongX ? glm::vec2(travel.x > 0 ? 1.F : -1.F, 0) : glm::vec2(0, travel.y > 0 ? 1.F : -1.F);
    const auto along = [&](glm::vec3 q) { return q.x * d.x + q.z * d.y; };
    for (int n = 0; n < 120 * 20; ++n) {
      const float x = along(m.position), next = along(moverAt(m, g.elapsed() + .05F));
      const float gap = x - along(g.player().position);
      if (next - x < -.01F && gap > 3.6F && gap < 6.F) break;   // heading back toward us
      tick({});
    }
    for (int n = 0; n < 120 * 6; ++n) {
      const auto p = g.player().position;
      const float gap = along(m.position) - along(p);
      relay::Input in; in.movement = d; in.sprintHeld = true; in.jumpHeld = true;
      const float side = std::clamp((alongX ? m.start.z - p.z : m.start.x - p.x) * 2.F, -1.F, 1.F);
      if (alongX) in.movement.y = side; else in.movement.x = side;
      // Jump about 0.25 s before contact (the block and the courier close fast): the feet clear the
      // block from about 0.15 s to 0.55 s into a jump, and the overlap lasts under 0.2 s.
      const float closing = std::max(1.F, glm::dot(glm::vec2(g.player().velocity.x, g.player().velocity.z), d) - (along(moverAt(m, g.elapsed() + .01F)) - along(m.position)) / .01F);
      if (g.player().grounded && gap > 0 && (gap - 1.4F) / closing < .25F) in.jumpPressed = true;
      if (!in.jumpPressed) in.jumpPressed = wantsJump(d);
      tick(in);
      if (gap < -1.7F && g.player().grounded) return;
    }
    throw std::runtime_error("could not get past the sliding block" + where());
  }
  void pad(glm::vec2 at, glm::vec2 landing) {
    // Walk onto the pad; once it fires, steer toward the landing in the air.
    // A chain of pads: a second launch (rising fast again after coming down) ends this step and
    // the next pad step steers on from there.
    bool launched = false, fell = false;
    const float startY = g.player().position.y;
    for (int n = 0; n < 120 * 6; ++n) {
      const auto p = g.player().position;
      if (launched && g.player().velocity.y < 0) fell = true;
      if (fell && g.player().velocity.y > 10) return;
      launched = launched || g.player().velocity.y > 10;
      const glm::vec2 d = (launched ? landing : at) - glm::vec2(p.x, p.z);
      relay::Input in; in.movement = glm::length(d) > .05F ? d / std::max(glm::length(d), .35F) : glm::vec2(0);
      if (glm::length(in.movement) > 1) in.movement = glm::normalize(in.movement);
      tick(in);
      if (launched && g.player().grounded && g.player().position.y > startY + 1.5F) return;  // landed up on the next lane
    }
    throw std::runtime_error("jump pad did not land" + where());
  }
  void core() {
    const int before = g.collectedCount();
    check(g.availableInteraction() == relay::Interaction::Core, "no core in reach" + where());
    relay::Input in; in.interactPressed = true; tick(in); idle(.5F);
    check(g.collectedCount() == before + 1, "core not collected" + where());
  }
};
}  // namespace

int main() {
  try {
    relay::Game g; g.reset(relay::Game::runLevel);
    check(g.level() == relay::Game::runLevel && std::string(g.levelName()) == "CORONA RUN", "Corona Run loads as level 3");
    check(g.cores().size() == 9 && g.checkpoints().size() == 7 && g.traps().size() == 15 && g.movers().size() == 5 && g.jumpPads().size() == 4 && g.portals().empty(), "Corona Run pieces");
    check(g.timedLevel() && g.timeRemaining() == 300 && g.wordless(), "Five-minute wordless run");
    check(g.player().position.y > 3.9F, "Spawn stands on the raised start platform");
    // Elevated hazards stand on the lanes, not on the void floor.
    for (const auto& t : g.traps()) check(t.area.min.y >= 5, "Shock strips sit on the lanes");
    for (const auto& m : g.movers()) check(m.start.y > 5, "Sweepers move on the lanes");
    g.update(.2F, {});
    check(g.player().grounded && !g.playerIntersectsScene(), "Clear spawn");

    // The void: stepping off the start platform costs an armour cell and returns to the spawn.
    {
      relay::Game f; f.reset(relay::Game::runLevel); f.update(.1F, {});
      f.warpPlayer({8.5F, 1.2F, 78.5F}); for (int i = 0; i < 30; ++i) f.update(dt, {});
      check(f.falls() == 1 && f.player().health == 4 && glm::distance(f.player().position, f.spawnPoint()) < .3F, "A fall into the void returns to the spawn for one armour cell");
      // The robot beams back in: it pops up from nothing and is fully visible half a second later.
      for (int i = 0; i < 60; ++i) f.update(dt, {});
      check(relay::SceneModels::bodyScale(f.player()).y > .9F, "The robot is visible again after respawning");
      const auto cp = f.checkpoints()[0];
      f.warpPlayer(cp.respawn); for (int i = 0; i < 30; ++i) f.update(dt, {});
      check(f.checkpoints()[0].active && glm::distance(f.respawnPoint(), cp.respawn) < .01F, "Passing a checkpoint saves it");
      f.warpPlayer({40.5F, 1.2F, 62.5F}); for (int i = 0; i < 30; ++i) f.update(dt, {});
      check(f.falls() == 2 && glm::distance(f.player().position, cp.respawn) < .3F, "Later falls return to the saved checkpoint");
      for (int k = 0; k < 3; ++k) { f.update(1.1F, {}); f.warpPlayer({40.5F, 1.2F, 62.5F}); for (int i = 0; i < 30; ++i) f.update(dt, {}); }
      check(f.state() == relay::RunState::Lost && f.respawning() && f.lives() == 2, "Five falls cost a life");
      // Lives: the robot reboots at the saved checkpoint with full armour; the third loss is final.
      for (int i = 0; i < int(relay::Game::respawnDelay / dt) + 2 && f.state() != relay::RunState::Playing; ++i) f.update(dt, {});
      check(f.state() == relay::RunState::Playing && f.player().health == 5 && glm::distance(f.player().position, cp.respawn) < .3F, "Reboot at the checkpoint with full armour");
      check(relay::SceneModels::bodyScale(f.player()).y < .5F, "The reboot beams in from nothing");
      for (int life = 0; life < 2; ++life) {
        for (int k = 0; k < 5 && f.state() == relay::RunState::Playing; ++k) { f.update(1.1F, {}); f.warpPlayer({40.5F, 1.2F, 62.5F}); for (int i = 0; i < 30; ++i) f.update(dt, {}); }
        if (life == 0) { check(f.respawning() && f.lives() == 1, "Second life lost"); for (int i = 0; i < int(relay::Game::respawnDelay / dt) + 2; ++i) f.update(dt, {}); }
      }
      check(f.outOfLives() && f.lives() == 0 && !f.respawning(), "Losing the third life ends the attempt (the level restarts)");
      f.update(5.F, {});
      check(f.state() == relay::RunState::Lost, "No reboot after the last life");
      f.reset(relay::Game::runLevel);
      check(f.falls() == 0 && !f.checkpoints()[0].active && f.player().health == 5 && f.lives() == 3, "Restart clears falls, checkpoints and lives");
    }

    // The full run with ordinary controls.
    std::ifstream file(std::filesystem::path(__FILE__).parent_path() / "corona_route.txt");
    check(bool(file), "route file missing");
    Bot bot{g};
    std::string line; int step = 0;
    while (std::getline(file, line)) {
      if (line.empty()) continue;
      std::istringstream s(line); std::string op; s >> op; ++step;
      if (std::getenv("CORONA_TRACE")) std::cerr << step << ": " << line << bot.where() << '\n';
      if (op == "go") {
        float x, z, takeoffZ;
        char mode;
        check(bool(s >> x >> z >> mode), "invalid go command: " + line);
        const std::optional<float> lateJump = s >> takeoffZ ? std::optional<float>(takeoffZ) : std::nullopt;
        bot.go({x, z}, mode, lateJump);
      }
      else if (op == "wtrap") { int i; float need; s >> i >> need; bot.waitTrap(i, need); }
      else if (op == "wmover") { int j; char axis; float lane, clearance; s >> j >> axis >> lane >> clearance; bot.waitMover(j, axis, lane, clearance); }
      else if (op == "jmover") { int j; s >> j; bot.jumpMover(j); }
      else if (op == "pad") { float px, pz, tx, tz; s >> px >> pz >> tx >> tz; bot.pad({px, pz}, {tx, tz}); }
      else if (op == "core") bot.core();
      else if (op == "exit") {
        for (int n = 0; n < 240 && g.state() == relay::RunState::Playing; ++n) { relay::Input in; in.movement = {0, -1}; bot.tick(in); }
      } else throw std::runtime_error("unknown route step " + op);
    }
    check(g.collectedCount() == int(g.cores().size()), "all cores collected");
    check(g.state() == relay::RunState::Won, "finished the run" + bot.where());
    int active = 0; for (const auto& c : g.checkpoints()) active += c.active;
    check(active == 1, "exactly the last checkpoint holds the respawn point");
    std::cout << "Corona Run: careful bot run " << g.elapsed() << " s of " << g.timeLimit() << " s, no falls, no damage\n";
  } catch (const std::exception& e) { std::cerr << "FAIL corona: " << e.what() << '\n'; return 1; }
}
