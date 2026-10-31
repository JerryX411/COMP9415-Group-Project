#include "app/Audio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "core/AssetPath.h"
#include "core/RelayGame.h"
#include "miniaudio.h"

namespace craft::app {
namespace {
// Sample groups (assets/sounds; see LICENSE.txt there). Events below layer several of them: a
// crisp transient, a low body pitched down for weight, and a tail.
enum Sound {
  Step, StepMetal, Thud, ThudHeavy, Clank, ClankMid, Slam, SlamBig, Punch, Hit, Plate, PlateMid,
  SwishLight, SwishHeavy, Boom, Thrust, JetPick, Field, Zap, Glass, Shards, Glitch, Noise, Core,
  Chime, Spring, ShieldPick, Clock, Checkpoint, Relay, DoorOpen, DoorClose, Click, PortalWake,
  PortalJump, Tick, Buzz, Crunch, Reboot, Win, Ring, MenuMove, MenuConfirm, MenuBack,
  StepRun, SlideStart, SlideEnd, BoostPulse, BluntBody, ElectricArc, CoreAbsorb, EngineIgnite,
  JumpLift, LandContact, LandHeavy, WallContact, WallRelease,
  RobotBreak, PowerDown, BootUp, Servo, ArcBlast, MetalBlast, BoostLaser, BoostArc,
  Transit, PortalOpen, SaveCharge, SaveReady, SoundCount
};
struct Spec { int copies; std::vector<const char*> files; };
const std::array<Spec, SoundCount>& specs() {
  static const std::array<Spec, SoundCount> table{{
    {2, {"footstep_concrete_000.ogg", "footstep_concrete_001.ogg", "footstep_concrete_002.ogg", "footstep_concrete_003.ogg", "footstep_concrete_004.ogg"}},
    {2, {"impactMetal_light_000.ogg", "impactMetal_light_001.ogg", "impactMetal_light_002.ogg"}},
    {2, {"impactSoft_medium_000.ogg", "impactSoft_medium_001.ogg", "impactSoft_medium_002.ogg"}},
    {1, {"impactSoft_heavy_000.ogg", "impactSoft_heavy_001.ogg"}},
    {1, {"impactMetal_heavy_000.ogg", "impactMetal_heavy_001.ogg", "impactMetal_heavy_002.ogg"}},
    {1, {"impactMetal_medium_000.ogg", "impactMetal_medium_001.ogg"}},
    {2, {"oga_slam_01.ogg", "oga_slam_04.ogg", "oga_slam_05.ogg", "oga_slam_06.ogg"}},
    {1, {"oga_slam_02.ogg", "oga_slam_03.ogg"}},
    {1, {"impactPunch_heavy_000.ogg", "impactPunch_heavy_001.ogg", "impactPunch_heavy_002.ogg"}},
    {1, {"oga_hit_01.ogg", "oga_hit_02.ogg", "oga_hit_03.ogg"}},
    {1, {"impactPlate_heavy_000.ogg", "impactPlate_heavy_001.ogg", "impactPlate_heavy_002.ogg"}},
    {1, {"impactPlate_medium_000.ogg", "impactPlate_medium_001.ogg", "impactPlate_medium_002.ogg"}},
    {2, {"oga_swish-10.wav", "oga_swish-11.wav", "oga_swish-12.wav", "oga_swish-13.wav"}},
    {2, {"oga_swish-3.wav", "oga_swish-4.wav", "oga_swish-5.wav", "oga_swish-7.wav", "oga_swish-9.wav"}},
    {1, {"lowFrequency_explosion_000.ogg", "lowFrequency_explosion_001.ogg"}},
    {1, {"thrusterFire_001.ogg"}},
    {1, {"thrusterFire_002.ogg"}},
    {2, {"forceField_000.ogg", "forceField_001.ogg", "forceField_002.ogg"}},
    {1, {"forceField_003.ogg", "forceField_004.ogg"}},
    {1, {"impactGlass_heavy_000.ogg", "impactGlass_heavy_001.ogg", "impactGlass_heavy_002.ogg"}},
    {1, {"oga_glass_01.ogg", "oga_glass_02.ogg", "oga_glass_03.ogg"}},
    {1, {"glitch_001.ogg", "glitch_002.ogg", "glitch_003.ogg", "glitch_004.ogg"}},
    {1, {"computerNoise_000.ogg", "computerNoise_001.ogg"}},
    {1, {"confirmation_001.ogg", "confirmation_002.ogg"}},
    {2, {"maximize_001.ogg", "maximize_002.ogg", "maximize_003.ogg"}},
    {1, {"pluck_001.ogg", "pluck_002.ogg"}},
    {1, {"glass_001.ogg", "glass_002.ogg"}},
    {1, {"bong_001.ogg"}},
    {1, {"confirmation_003.ogg"}},
    {1, {"switch_001.ogg", "switch_002.ogg", "switch_003.ogg", "switch_004.ogg"}},
    {2, {"doorOpen_000.ogg", "doorOpen_001.ogg", "doorOpen_002.ogg"}},
    {2, {"doorClose_000.ogg", "doorClose_001.ogg", "doorClose_002.ogg"}},
    {1, {"click_001.ogg", "click_002.ogg", "click_003.ogg"}},
    {1, {"maximize_004.ogg", "maximize_005.ogg"}},
    {1, {"laserLarge_000.ogg", "laserLarge_001.ogg"}},
    {2, {"tick_001.ogg", "tick_002.ogg"}},
    {2, {"laserSmall_000.ogg", "laserSmall_001.ogg", "laserSmall_002.ogg"}},
    {1, {"explosionCrunch_000.ogg", "explosionCrunch_001.ogg"}},
    {1, {"laserLarge_002.ogg"}},
    {1, {"confirmation_004.ogg"}},
    {1, {"oga_metal_08.ogg", "oga_metal_09.ogg", "oga_metal_10.ogg"}},
    {2, {"select_001.ogg", "select_002.ogg", "select_003.ogg", "select_004.ogg"}},
    {1, {"click_004.ogg", "click_005.ogg"}},
    {1, {"back_001.ogg", "back_002.ogg"}},
    {2, {"footstep_concrete_000.ogg", "footstep_concrete_001.ogg", "footstep_concrete_002.ogg"}},
    {2, {"oga_scrape-4.ogg", "oga_scrape-5.ogg"}},
    {2, {"oga_scrape-6.ogg", "oga_scrape-8.ogg"}},
    {2, {"thrusterFire_001.ogg"}},
    {2, {"feedback/blunt_1.wav", "feedback/blunt_2.wav", "feedback/blunt_3.wav"}},
    {2, {"feedback/electric_1.wav", "feedback/electric_2.wav", "feedback/electric_3.wav"}},
    {2, {"feedback/core_1.wav", "feedback/core_2.wav", "feedback/core_3.wav"}},
    {2, {"feedback/ignition_1.wav", "feedback/ignition_2.wav", "feedback/ignition_3.wav"}},
    {2, {"impactSoft_medium_000.ogg", "impactSoft_medium_001.ogg"}},
    {2, {"footstep_concrete_000.ogg", "footstep_concrete_001.ogg", "footstep_concrete_002.ogg"}},
    {2, {"impactSoft_heavy_000.ogg", "impactSoft_heavy_001.ogg"}},
    {2, {"impactPlate_medium_000.ogg", "impactPlate_medium_001.ogg"}},
    {2, {"oga_swish-10.wav", "oga_swish-11.wav"}},
    {1, {"feedback/shutdown_1.wav", "feedback/shutdown_2.wav", "feedback/shutdown_3.wav"}},
    {1, {"feedback/powerdown_1.wav", "feedback/powerdown_2.wav", "feedback/powerdown_3.wav"}},
    {1, {"feedback/boot_1.wav", "feedback/boot_2.wav", "feedback/boot_3.wav"}},
    {2, {"feedback/servo_1.wav", "feedback/servo_2.wav", "feedback/servo_3.wav", "feedback/servo_4.wav", "feedback/servo_5.wav", "feedback/servo_6.wav"}},
    {2, {"forceField_003.ogg", "forceField_004.ogg"}},
    {2, {"oga_slam_01.ogg", "oga_slam_04.ogg", "oga_slam_05.ogg"}},
    {2, {"laserSmall_000.ogg", "laserSmall_001.ogg", "laserSmall_002.ogg"}},
    {2, {"glitch_001.ogg", "glitch_002.ogg"}},
    {2, {"feedback/transit_1.wav", "feedback/transit_2.wav", "feedback/transit_3.wav"}},
    {2, {"feedback/portal_open_1.wav", "feedback/portal_open_2.wav", "feedback/portal_open_3.wav"}},
    {1, {"feedback/checkpoint_charge_1.wav"}},
    {2, {"feedback/checkpoint_ready_1.wav"}},
  }};
  return table;
}
}  // namespace

struct Audio::Impl {
  ma_engine engine{};
  bool ready = false;
  struct Group { std::vector<std::unique_ptr<ma_sound>> voices; std::vector<int> next; int copies = 1, last = -1; double lastTime = -10; };
  std::array<Group, SoundCount> groups;
  // Loops: the thruster under flights and jet boosts, the scrape under slides. Big impacts duck
  // them for a moment so the hit cuts through.
  std::unique_ptr<ma_sound> thrust, scrape, motor;
  float thrustVolume = 0, scrapeVolume = 0, motorVolume = 0, motorPitch = .8F, duck = 1;
  bool wasPlaying = false, wasSliding = false, offline = false, localPack = false;
  double clock = 0, lastStep = -10, lastBoost = -10;
  double lastIgnition = -10, wallReleaseDue = -1;
  int boostCombo = 0;
  float releasePitch = 1;
  double lastServo = -10;
  bool crouched = false;
  bool wasCheckpointInteraction = false;
  glm::vec3 listener{0}, listenerRight{1,0,0};
  bool hasListener = false;
  float actorGain = 1, actorPan = 0;
  static float distanceGain(float distance) { return 1.F / (1.F + .065F * std::max(0.F, distance - 3.F) * std::max(0.F, distance - 3.F)); }
  float sourcePan(glm::vec3 source) const {
    glm::vec3 delta = source - listener; delta.y = 0;
    const float d = glm::length(delta);
    return d > .01F ? .8F * std::clamp(glm::dot(delta/d, listenerRight), -1.F, 1.F) : 0.F;
  }
  unsigned seed = 0x9e3779b9u;
  // What the game looked like last frame (to spot what just happened).
  bool synced = false;
  int level = -1, revision = -1, collected = 0, wallJumps = 0, teleports = 0;
  float elapsed = 0;
  relay::CourierMotion motion{};
  relay::RunState state = relay::RunState::Playing;
  bool gateOpen = false;
  std::vector<bool> checkpoints, plates, portalEnds;
  std::vector<float> switches, doors;
  std::vector<relay::TrapPhase> traps;

  float random01() { seed = seed * 1664525u + 1013904223u; return float(seed >> 8) / 16777216.F; }

  explicit Impl(bool renderOffline) : offline(renderOffline) {
    auto config = ma_engine_config_init();
    config.noDevice = renderOffline; config.channels = 2; config.sampleRate = 48000;
    if (ma_engine_init(&config, &engine) != MA_SUCCESS) return;
    ready = true;
    ma_engine_set_volume(&engine, .65F);
    std::string folder;
    bool reported = false;
    // assetPath finds files, so locate the folder through its licence note.
    try { folder = craft::core::assetPath("sounds/LICENSE.txt").parent_path().string(); } catch (...) { return; }
    const auto load = [&](const std::string& file, bool loop) -> std::unique_ptr<ma_sound> {
      auto sound = std::make_unique<ma_sound>();
      const std::string path = folder + "/" + file;
      const ma_result result = ma_sound_init_from_file(&engine, path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, sound.get());
      if (result != MA_SUCCESS) { if (!reported) { std::fprintf(stderr, "audio: cannot load %s (%s)\n", path.c_str(), ma_result_description(result)); reported = true; } return nullptr; }
      ma_sound_set_looping(sound.get(), loop ? MA_TRUE : MA_FALSE);
      return sound;
    };
    // Optional local sample pack; the CC0 pack remains a complete portable fallback.
    localPack = !std::getenv("CRAFT_AUDIO_CC0_ONLY") && std::filesystem::exists(folder + "/local-apex/SOURCE.json");
    for (int s = 0; s < SoundCount; ++s) {
      auto& g = groups[std::size_t(s)]; const auto& spec = specs()[std::size_t(s)];
      g.copies = spec.copies;
      std::vector<std::string> files(spec.files.begin(), spec.files.end());
      const char* prefix = nullptr; int count = 0;
      if (s == Step) { prefix = "walk"; count = 8; }
      if (s == StepRun) { prefix = "run"; count = 8; }
      if (s == StepMetal) { prefix = "metal"; count = 6; }
      if (s == SlideStart) { prefix = "slide_start"; count = 4; }
      if (s == SlideEnd) { prefix = "slide_end"; count = 3; }
      if (s == BoostPulse) { prefix = "boost"; count = 2; }
      if (s == BluntBody) { prefix = "blunt"; count = 3; }
      if (s == ElectricArc) { prefix = "electric"; count = 3; }
      if (s == CoreAbsorb) { prefix = "core"; count = 3; }
      if (s == JumpLift) { prefix = "jump"; count = 4; }
      if (s == LandContact) { prefix = "land"; count = 4; }
      if (s == LandHeavy) { prefix = "heavy_land"; count = 3; }
      if (s == WallContact) { prefix = "wall"; count = 4; }
      if (s == WallRelease) { prefix = "wall_release"; count = 3; }
      if (s == RobotBreak) { prefix = "shutdown"; count = 3; }
      if (s == PowerDown) { prefix = "powerdown"; count = 3; }
      if (s == BootUp) { prefix = "boot"; count = 3; }
      if (s == Servo) { prefix = "servo"; count = 6; }
      if (s == ArcBlast) { prefix = "arc_blast"; count = 3; }
      if (s == MetalBlast) { prefix = "metal_blast"; count = 3; }
      if (s == BoostLaser) { prefix = "boost_laser"; count = 3; }
      if (s == BoostArc) { prefix = "boost_arc"; count = 2; }
      if (s == Transit) { prefix = "transit"; count = 3; }
      if (s == PortalOpen) { prefix = "portal_open"; count = 3; }
      if (s == SaveCharge) { prefix = "checkpoint_charge"; count = 1; }
      if (s == SaveReady) { prefix = "checkpoint_ready"; count = 1; }
      if (localPack && prefix) {
        std::vector<std::string> candidates;
        for (int n = 1; n <= count; ++n) candidates.push_back(std::string("local-apex/") + prefix + "_0" + std::to_string(n) + ".wav");
        if (std::all_of(candidates.begin(), candidates.end(), [&](const auto& f) { return std::filesystem::exists(folder + "/" + f); })) files = candidates;
      }
      for (const auto& file : files)
        for (int c = 0; c < spec.copies; ++c) if (auto sound = load(file, false)) g.voices.push_back(std::move(sound));
      g.next.assign(files.size(), 0);
      if (g.voices.size() != files.size() * std::size_t(spec.copies)) {
        for (auto& v : g.voices) ma_sound_uninit(v.get());
        g.voices.clear(); g.next.clear();
      }
    }
    for (auto* loop : {&thrust, &scrape}) {
      const std::string custom = loop == &thrust ? "local-apex/thrust_loop.wav" : "local-apex/slide_loop.wav";
      *loop = load(localPack && std::filesystem::exists(folder + "/" + custom) ? custom : loop == &thrust ? "thrusterFire_000.ogg" : "oga_scrape-7.ogg", true);
      if (*loop) { ma_sound_set_volume(loop->get(), 0); ma_sound_start(loop->get()); }
    }
    motor = load(localPack && std::filesystem::exists(folder + "/local-apex/engine_loop.wav") ? "local-apex/engine_loop.wav" : "feedback/engine_loop.wav", true);
    if (motor) { ma_sound_set_volume(motor.get(), 0); ma_sound_start(motor.get()); }
  }
  ~Impl() {
    if (!ready) return;
    for (auto& g : groups) for (auto& v : g.voices) ma_sound_uninit(v.get());
    for (auto* loop : {&thrust, &scrape, &motor}) if (*loop) ma_sound_uninit(loop->get());
    ma_engine_uninit(&engine);
  }

  // Play one variant at volume (gain above 1 lifts quiet sources) and pitch, with a touch of
  // random pitch; maxSeconds > 0 cuts it short with a quick fade.
  void play(Sound id, float volume, float pitch = 1, float maxSeconds = 0, float pan = 0, bool positioned = false) {
    if (!positioned && id != MenuMove && id != MenuConfirm && id != MenuBack) { volume *= actorGain; pan += actorPan; }
    if (!ready || volume <= .01F) return;
    auto& g = groups[std::size_t(id)];
    if (g.voices.empty()) return;
    const double now = double(ma_engine_get_time_in_milliseconds(&engine)) / 1000;
    if (now - g.lastTime < .035) return;
    g.lastTime = now;
    const int variants = int(g.next.size());
    int variant = std::min(variants - 1, int(random01() * float(variants)));
    if (variants > 1 && variant == g.last) variant = (variant + 1 + int(random01() * float(variants - 1))) % variants;
    g.last = variant;
    int& slot = g.next[std::size_t(variant)];
    ma_sound* s = g.voices[std::size_t(variant * g.copies + slot)].get(); slot = (slot + 1) % g.copies;
    ma_sound_stop(s); ma_sound_seek_to_pcm_frame(s, 0);
    ma_sound_set_fade_in_pcm_frames(s, 1, 1, 0);
    ma_sound_set_stop_time_in_pcm_frames(s, ~ma_uint64(0));
    ma_sound_set_volume(s, std::min(.9F, volume));
    ma_sound_set_pan(s, std::clamp(pan, -.85F, .85F));
    ma_sound_set_pitch(s, pitch * (.98F + .04F * random01()));
    if (maxSeconds > 0) ma_sound_set_stop_time_with_fade_in_milliseconds(s, ma_engine_get_time_in_milliseconds(&engine) + ma_uint64(maxSeconds * 1000), 120);
    ma_sound_start(s);
  }
  void hitDuck(float depth) { duck = std::min(duck, 1 - depth); }
  void stopGroup(Sound id) { for (auto& v : groups[std::size_t(id)].voices) ma_sound_stop(v.get()); }
  void transit() {
    for (Sound s : {BoostLaser, BoostArc, BoostPulse, EngineIgnite, JumpLift, WallRelease}) stopGroup(s);
    wallReleaseDue = -1;
    play(Transit, .82F); hitDuck(.7F);
  }
  void playAt(const relay::Game& game, Sound id, glm::vec3 source, float volume, float pitch = 1) {
    if (!hasListener) { play(id, volume, pitch); return; }
    float gain = distanceGain(glm::distance(source, listener));
    // Cheap obstruction approximation for environmental cues, not a room-acoustics simulation.
    for (int i=1; i<16; ++i) {
      const auto at = glm::mix(source, listener, float(i)/16.F);
      if (game.world().cell(int(std::floor(at.x)), int(std::floor(at.y)), int(std::floor(at.z)))) { gain *= .45F; break; }
    }
    play(id, volume * gain, pitch, 0, sourcePan(source), true);
  }

  void impact(bool electric, float pan = 0) {
    if (electric) { play(ArcBlast, .78F, 1.12F, .55F, pan); play(ElectricArc, .48F, 1.F, 0, pan); }
    else { play(MetalBlast, .65F, .82F, .65F, pan); play(BluntBody, .76F, .88F, 0, pan); }
    hitDuck(electric ? .65F : .75F);
  }
  void collectCore() { play(CoreAbsorb, .9F); play(Core, .22F, 1.1F); mechanical(.12F, 1.05F); hitDuck(.5F); }
  void ignite(bool jet) {
    const double now = double(ma_engine_get_time_in_milliseconds(&engine)) / 1000;
    boostCombo = now - lastIgnition < 1.4 ? (boostCombo + 1) % 3 : 0;
    lastIgnition = now;
    const float pitch = (jet ? .91F : .98F) + .09F * float(boostCombo);
    play(EngineIgnite, .72F, pitch);
    play(BoostPulse, .23F, pitch, .5F);
    play(BoostArc, .33F, pitch, .32F);
    play(BoostLaser, .42F, pitch + .12F, .38F);
  }
  void mechanical(float volume, float pitch = 1) {
    const double now = double(ma_engine_get_time_in_milliseconds(&engine)) / 1000;
    if (now - lastServo < .16) return;
    lastServo = now; play(Servo, volume, pitch, .28F);
  }
  void shutdown(bool destroyed) { silence(); play(destroyed ? RobotBreak : PowerDown, destroyed ? .87F : .62F); }
  void boot() { silence(); play(BootUp, .72F); }
  void jump() { play(JumpLift, .48F, 1.08F); play(SwishLight, .16F, 1.15F, .2F); mechanical(.18F, 1.15F); }
  void land(float strength) {
    const float weight = std::clamp((strength - .25F) / 1.3F, 0.F, 1.F);
    play(LandContact, .28F + .43F * weight, 1.08F - .2F * weight);
    if (weight > .2F) play(LandHeavy, .15F + .5F * weight, 1.F - .22F * weight);
    if (weight > .55F) {
      const float heavy = (weight - .55F) / .45F;
      play(BluntBody, .24F + .40F * heavy, .76F);
      play(ClankMid, .10F + .12F * heavy, .85F, .3F);
      hitDuck(.3F + .35F * heavy);
    }
    lastStep = clock;  // Landing replaces the first foot contact, rather than doubling it.
    mechanical(.10F + .08F * weight, .87F);
  }
  void wallKick(int kind) {
    releasePitch = kind == 10 ? 1.12F : kind == 11 ? .93F : 1.F;
    play(WallContact, .72F, releasePitch);
    play(Thud, .22F, .87F);
    wallReleaseDue = clock + .045;
    hitDuck(.25F);
    mechanical(.22F, 1.08F);
  }

  void silence() {
    for (auto& group : groups) for (auto& voice : group.voices) ma_sound_stop(voice.get());
    for (auto* loop : {&thrust, &scrape, &motor}) if (*loop) ma_sound_set_volume(loop->get(), 0);
    thrustVolume = scrapeVolume = motorVolume = 0; motorPitch = .8F; duck = 1; wasSliding = false;
    lastStep = lastBoost = -10;
    lastIgnition = -10; boostCombo = 0; wallReleaseDue = -1;
    lastServo = -10;
  }

  void sync(const relay::Game& g) {
    const auto& p = g.player();
    level = g.level(); revision = g.revision(); elapsed = g.elapsed(); collected = g.collectedCount(); wallJumps = g.wallJumpsDone(); teleports = g.teleports();
    motion = p.motion; state = g.state(); gateOpen = g.gateUnlocked(); crouched = p.crouching;
    wasCheckpointInteraction = p.interaction == relay::Interaction::Checkpoint && p.interactionTime > 0;
    checkpoints.clear(); for (const auto& c : g.checkpoints()) checkpoints.push_back(c.active);
    plates.clear(); for (const auto& pl : g.plates()) plates.push_back(pl.down);
    portalEnds.clear(); for (const auto& l : g.portals()) { portalEnds.push_back(l.fromOn); portalEnds.push_back(l.toOn); }
    switches.clear(); for (const auto& s : g.switches()) switches.push_back(s.animation);
    doors.clear(); for (const auto& d : g.doors()) doors.push_back(d.open);
    traps.clear(); for (std::size_t i = 0; i < g.traps().size(); ++i) traps.push_back(g.trapPhase(i));
    synced = true;
  }

  void update(const relay::Game& g, float dt, bool playing) {
    if (!ready) return;
    dt = std::clamp(dt, 0.F, .1F); clock += dt;
    if (!playing) { if (wasPlaying) silence(); sync(g); wasPlaying = false; return; }
    // A new level (or a restart): take stock quietly.
    if (!synced || !wasPlaying || g.revision() != revision || g.level() != level || g.elapsed() < elapsed - .001F) { silence(); sync(g); wasPlaying = true; return; }
    const auto& p = g.player(); const auto& a = p.motion;
    if (hasListener) {
      const float gain = std::clamp(distanceGain(glm::distance(listener,p.position+glm::vec3(0,1,0))),.65F,1.F);
      actorGain += (gain-actorGain)*(1-std::exp(-8.F*dt));
      actorPan += (.35F*sourcePan(p.position)-actorPan)*(1-std::exp(-8.F*dt));
    }
    const auto fresh = [&](float now, float before) { return now < before && now < .1F; };
    // Death and reboot are exclusive transitions: no stale foot/impact/boost layers survive.
    if (g.state() == relay::RunState::Lost) {
      if (state != relay::RunState::Lost) shutdown(p.health <= 0);
      sync(g); return;
    }
    if ((state == relay::RunState::Lost && g.state() == relay::RunState::Playing) || fresh(a.spawnAge, motion.spawnAge)) {
      boot(); sync(g); return;
    }
    const auto near = [&](glm::vec3 at) { return std::clamp(1.F - glm::distance(at, p.position) / 16.F, 0.F, 1.F); };
    const float speed = glm::length(p.locomotion);
    const bool teleportEvent = g.teleports() > teleports;
    const bool saving = p.interaction == relay::Interaction::Checkpoint && p.interactionTime > 0;
    if (saving && !wasCheckpointInteraction) {
      const int cp = g.nearestCheckpoint();
      if (cp >= 0 && !g.checkpoints()[std::size_t(cp)].active) playAt(g, SaveCharge, g.checkpoints()[std::size_t(cp)].position+glm::vec3(0,.9F,0), .7F);
    }
    if (!saving || fresh(a.hitAge,motion.hitAge)) stopGroup(SaveCharge);
    const bool wallEvent = g.wallJumpsDone() > wallJumps;
    const bool landingEvent = fresh(a.landAge, motion.landAge);
    if (wallReleaseDue >= 0 && clock >= wallReleaseDue) { play(WallRelease, .32F, releasePitch, .25F); wallReleaseDue = -1; }
    // Feet: a concrete step with a faint metal tick on top (the courier is a robot), harder at a sprint.
    if (!landingEvent && fresh(a.stepAge, motion.stepAge) && p.grounded && !p.sliding && speed > .4F && clock - lastStep > .14) {
      lastStep = clock;
      const float k = std::clamp(a.stepStrength, .1F, 1.F);
      const bool metal = std::any_of(g.traps().begin(), g.traps().end(), [&](const auto& t) {
        return p.position.x >= t.area.min.x && p.position.x <= t.area.max.x &&
               p.position.z >= t.area.min.z && p.position.z <= t.area.max.z &&
               std::abs(p.position.y - t.area.max.y) < .35F;
      });
      play(localPack && metal ? StepMetal : p.sprinting ? StepRun : Step, (.24F + .35F * k) * (p.crouching ? .45F : 1.F), 1.F, 0, a.stepSide * .08F);
      if (!localPack) play(StepMetal, .025F + .035F * k, 1.1F);
      mechanical(p.crouching ? .045F : p.sprinting ? .12F : .085F, p.sprinting ? 1.12F : .96F);
    }
    if (p.crouching != crouched && !wallEvent) mechanical(.13F, p.crouching ? .82F : 1.1F);
    if (!wallEvent && (fresh(a.startAge, motion.startAge) || fresh(a.stopAge, motion.stopAge) || fresh(a.pivotAge, motion.pivotAge))) mechanical(.10F, .94F);
    if (!teleportEvent && !wallEvent && fresh(a.launchAge, motion.launchAge)) jump();
    if (landingEvent) land(a.landStrength);
    // Slide: a drop-in slam and thud with a whoosh (a boosted one adds a field burst and a low
    // boom); the scrape loop below follows the slide's speed.
    if (fresh(a.slideAge, motion.slideAge)) {
      play(SlideStart, .6F); play(Thud, .16F, .95F);
      mechanical(.16F, .82F);
    }
    if (fresh(a.popAge, motion.popAge)) { play(SwishHeavy, .5F, 1.05F); play(Thud, .5F, .9F); }
    if (!wallEvent && fresh(a.trickAge, motion.trickAge)) play(SwishHeavy, .38F, .85F + .06F * float(a.trickKind % 4));
    if (!wallEvent && fresh(a.bounceAge, motion.bounceAge)) { const float k = std::min(a.bounceStrength, 1.2F); play(PlateMid, .45F + .35F * k); play(Slam, .3F * k, 1.2F); }
    if (wallEvent) wallKick(a.trickKind);
    if (!wallEvent && fresh(a.pushAge, motion.pushAge)) { play(Thud, .4F, .85F); play(PlateMid, .25F, 1.1F); }
    // Boosts: the jetpack (a deep boom, a big slam pitched down, a heavy whoosh, a thruster
    // burst), speed pads, flight.
    const bool jet = fresh(a.jetAge, motion.jetAge);
    if (!teleportEvent && (jet || fresh(a.boostAge, motion.boostAge) || fresh(a.takeoffAge, motion.takeoffAge)) && clock - lastBoost > .18) {
      lastBoost = clock; ignite(jet);
    }
    if (!landingEvent && fresh(a.touchAge, motion.touchAge)) land(.9F);
    // Hits and hazards.
    if (fresh(a.hitAge, motion.hitAge)) {
      const auto away = a.hitSide*glm::vec3(std::cos(p.yaw),0,-std::sin(p.yaw)) + a.hitForward*glm::vec3(-std::sin(p.yaw),0,-std::cos(p.yaw));
      impact(a.hitKind == 1, hasListener ? -.35F*glm::dot(away,listenerRight) : 0.F);
    }
    if (fresh(a.shieldBreakAge, motion.shieldBreakAge)) { play(Glass, .95F); play(Shards, .75F); play(Slam, .45F, 1.2F); hitDuck(.5F); }
    if (fresh(a.hackAge, motion.hackAge)) { play(Glitch, .75F); play(Noise, .3F, 1.F, .55F); play(Hit, .35F, .7F); }
    for (std::size_t i = 0; i < g.traps().size() && i < traps.size(); ++i) {
      const auto phase = g.trapPhase(i);
      if (phase == traps[i]) continue;
      const auto at = (g.traps()[i].area.min + g.traps()[i].area.max) * .5F + glm::vec3(0,.2F,0);
      if (phase == relay::TrapPhase::Warning) playAt(g, Tick, at, .3F);
      if (phase == relay::TrapPhase::Active) playAt(g, Buzz, at, .35F, .85F);
    }
    // Pickups, cores and devices.
    if (fresh(a.pickupAge, motion.pickupAge)) {
      const auto kind = relay::PowerUp(a.pickupKind);
      play(Chime, .45F);
      play(kind == relay::PowerUp::Spring ? Spring : kind == relay::PowerUp::Shield ? ShieldPick : kind == relay::PowerUp::Time ? Clock : JetPick, .7F, 1.F, kind == relay::PowerUp::Jet ? .6F : 0.F);
    }
    if (g.collectedCount() > collected) collectCore();
    for (std::size_t i = 0; i < g.checkpoints().size() && i < checkpoints.size(); ++i) if (g.checkpoints()[i].active && !checkpoints[i]) {
      stopGroup(SaveCharge);
      playAt(g, SaveReady, g.checkpoints()[i].position+glm::vec3(0,.9F,0), .82F);
      hitDuck(.3F);
    }
    for (std::size_t i = 0; i < g.switches().size() && i < switches.size(); ++i) if (g.switches()[i].animation > switches[i] + .5F) play(Relay, .7F);
    for (std::size_t i = 0; i < g.plates().size() && i < plates.size(); ++i) if (g.plates()[i].down && !plates[i]) play(Click, .6F * std::max(.3F, near(g.plates()[i].position)));
    for (std::size_t i = 0; i < g.doors().size() && i < doors.size(); ++i) {
      const auto& d = g.doors()[i];
      if (d.open > .02F && doors[i] <= .02F) playAt(g, DoorOpen, d.centre, .75F);
      if (d.open < .98F && doors[i] >= .98F) playAt(g, DoorClose, d.centre, .75F);
    }
    for (std::size_t i = 0; i < g.portals().size() && 2 * i + 1 < portalEnds.size(); ++i) {
      const auto& l = g.portals()[i];
      if (l.fromOn && !portalEnds[2 * i]) playAt(g, PortalOpen, l.from+glm::vec3(0,1,0), .65F, l.linked() ? 1.12F : .98F);
      if (l.toOn && !portalEnds[2 * i + 1]) playAt(g, PortalOpen, l.to+glm::vec3(0,1,0), .65F, l.linked() ? 1.12F : .98F);
    }
    if (teleportEvent) transit();
    if (g.gateUnlocked() && !gateOpen) { play(DoorOpen, .9F, .7F); play(Ring, .5F, .8F); }
    // Shutdown, reboot, victory.
    if (g.state() == relay::RunState::Won && state != relay::RunState::Won) { play(Win, .9F); play(Ring, .5F); }
    // Loops: the thruster follows a flight or a jet boost; the scrape follows a slide, louder and
    // higher the faster it goes. Both come back up after a duck.
    duck += (1 - duck) * (1 - std::exp(-4.F * dt));
    const bool alive = g.state() == relay::RunState::Playing && p.health > 0;
    const bool sliding = alive && p.sliding && p.grounded && speed > .5F;
    if (wasSliding && !sliding && alive && p.grounded) play(SlideEnd, .38F, 1.F, .45F);
    wasSliding = sliding;
    const bool powered = alive && (g.jetting() || g.boostTimeLeft() > 0 || p.flightBlend > .1F || p.strideBlend > .1F);
    const float load = std::clamp(speed / 18.F, 0.F, 1.F);
    const float motorWant = powered ? .38F + .4F * load : 0.F;
    motorVolume += (motorWant - motorVolume) * (1 - std::exp(-(motorWant > motorVolume ? 16.F : 7.F) * dt));
    motorPitch += ((.72F + .62F * load + (g.jetting() ? .12F : 0.F)) - motorPitch) * (1 - std::exp(-6.F * dt));
    if (motor) { ma_sound_set_volume(motor.get(), motorVolume * duck * actorGain); ma_sound_set_pan(motor.get(),actorPan); ma_sound_set_pitch(motor.get(), motorPitch); }
    const float thrustWant = powered ? (.2F + .15F * load) : 0.F;
    const float scrapeWant = sliding ? std::clamp(speed / 14.F, .1F, 1.F) * .65F : 0.F;
    thrustVolume += (thrustWant - thrustVolume) * (1 - std::exp(-8.F * dt));
    scrapeVolume += (scrapeWant - scrapeVolume) * (1 - std::exp(-(scrapeWant > scrapeVolume ? 25.F : 10.F) * dt));
    if (thrust) { ma_sound_set_volume(thrust.get(), thrustVolume * duck * actorGain); ma_sound_set_pan(thrust.get(),actorPan); ma_sound_set_pitch(thrust.get(), .9F + .25F * (g.jetting() ? 1.F : 0.F)); }
    if (scrape) { ma_sound_set_volume(scrape.get(), scrapeVolume * duck * actorGain); ma_sound_set_pan(scrape.get(),actorPan); ma_sound_set_pitch(scrape.get(), .75F + .4F * std::min(speed / 14.F, 1.F)); }
    sync(g);
  }
};

Audio::Audio(bool offline) : impl_(std::make_unique<Impl>(offline)) {}
Audio::~Audio() = default;
bool Audio::renderDemo(const char* path) {
  if (!impl_->ready || !impl_->offline || loadedSounds() != soundKinds()) return false;
  auto& a = *impl_;
  ma_encoder encoder{};
  auto config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 2, 48000);
  if (ma_encoder_init_file(path, &config, &encoder) != MA_SUCCESS) return false;
  std::array<float, 960> samples{};
  std::array<ma_int16, 960> pcm{};
  float peak = 0, pausePeak = 0;
  double leftL=0,leftR=0,rightL=0,rightR=0,nearEnergy=0,farEnergy=0;
  bool ok = true;
  relay::Game probe;
  a.update(probe, .01F, true);
  for (int block = 0; block < 6400 && ok; ++block) {
    // Walk/run/slide, engine spool, blunt, electrical, core, then overlap and pause checks.
    if (block < 300 && block % 40 == 0) a.play(Step, .5F, 1.F, 0, (block / 40 % 2 ? .08F : -.08F));
    if (block >= 300 && block < 500 && block % 24 == 0) a.play(StepRun, .65F);
    if (block == 500) a.play(SlideStart, .6F);
    if (a.scrape) ma_sound_set_volume(a.scrape.get(), block >= 510 && block < 790 ? .5F : 0.F);
    if (block == 790) a.play(SlideEnd, .38F, 1.F, .45F);
    if (block == 820) a.ignite(true);
    if (a.motor) {
      const float ramp = std::clamp(float(block - 820) / 90.F, 0.F, 1.F);
      const float release = std::clamp(float(1110 - block) / 30.F, 0.F, 1.F);
      ma_sound_set_volume(a.motor.get(), block >= 820 && block < 1110 ? .78F * ramp * release : 0.F);
      ma_sound_set_pitch(a.motor.get(), .72F + .74F * ramp);
    }
    if (a.thrust) ma_sound_set_volume(a.thrust.get(), block >= 840 && block < 1090 ? .5F : 0.F);
    if (block == 1150) a.impact(false);
    if (block == 1350) a.impact(true);
    if (block == 1550) a.collectCore();
    if (block == 1750) { a.ignite(true); a.impact(true); a.collectCore(); }
    if (block == 1900) a.jump();
    if (block == 1980) a.land(.4F);
    if (block == 2150) a.land(1.55F);
    if (block == 2350) a.wallKick(9);
    a.clock += .01;
    if (a.wallReleaseDue >= 0 && a.clock >= a.wallReleaseDue) { a.play(WallRelease, .32F, a.releasePitch, .25F); a.wallReleaseDue = -1; }
    if (block == 2600 || block == 2670 || block == 2740 || block == 2950) {
      a.ignite(true);
      const int expected = block == 2670 ? 1 : block == 2740 ? 2 : 0;
      if (a.boostCombo != expected) ok = false;
    }
    if (block >= 3250 && block < 3450 && block % 35 == 0) { a.play(Step, .45F); a.mechanical(.11F); }
    if (block == 3500) a.shutdown(true);
    if (block == 3800) a.boot();
    if (block == 4000) a.shutdown(false);
    if (block == 4200) a.boot();
    if (block == 4400) { a.wallKick(9); a.update(probe, .01F, false); if (a.wallReleaseDue >= 0) ok = false; }
    if (block == 4800) { setListener({0,10,0},{1,0,0}); a.playAt(probe,BoostLaser,{-4,10,0},.5F); }
    if (block == 5000) a.playAt(probe,BoostLaser,{4,10,0},.5F);
    if (block == 5200) a.playAt(probe,BoostLaser,{20,10,0},.5F);
    if (block == 5400) { a.silence(); a.hasListener=false; a.actorGain=1; a.actorPan=0; }
    if (block == 5500) a.play(PortalOpen,.65F);
    if (block == 5620) a.transit();
    if (block == 5800) a.play(SaveCharge,.7F);
    if (block == 5823) { a.stopGroup(SaveCharge); a.play(SaveReady,.82F); }
    if (block == 6000) a.play(SaveCharge,.7F);
    if (block == 6008) a.stopGroup(SaveCharge);
    if (block == 6200) a.silence();
    ma_uint64 frames = 0;
    if (ma_engine_read_pcm_frames(&a.engine, samples.data(), 480, &frames) != MA_SUCCESS || frames != 480) { ok = false; break; }
    for (std::size_t i = 0; i < samples.size(); ++i) {
      if (!std::isfinite(samples[i])) { ok = false; break; }
      peak = std::max(peak, std::abs(samples[i]));
      const double energy = double(samples[i])*samples[i];
      if (block >= 4800 && block < 4900) { (i%2 ? leftR : leftL) += energy; nearEnergy += energy; }
      if (block >= 5000 && block < 5100) (i%2 ? rightR : rightL) += energy;
      if (block >= 5200 && block < 5300) farEnergy += energy;
      if (block >= 6250 || (block >= 6030 && block < 6150)) pausePeak = std::max(pausePeak, std::abs(samples[i]));
      pcm[i] = static_cast<ma_int16>(std::clamp(samples[i], -1.F, 1.F) * 32767.F);
    }
    ma_uint64 written = 0;
    if (ma_encoder_write_pcm_frames(&encoder, pcm.data(), 480, &written) != MA_SUCCESS || written != 480) ok = false;
  }
  ma_encoder_uninit(&encoder);
  std::printf("audio demo: pack=%s groups=%d/%d peak=%.4f pausePeak=%.6f\n", a.localPack ? "local-apex" : "CC0", loadedSounds(), soundKinds(), peak, pausePeak);
  const bool fieldOk = leftL > leftR*2 && rightR > rightL*2 && farEnergy < nearEnergy*.15 && farEnergy > 0;
  std::printf("audio field: left=%d right=%d far/near=%.4f\n",leftL>leftR*2,rightR>rightL*2,farEnergy/std::max(nearEnergy,1e-12));
  return ok && fieldOk && peak > .001F && peak < .99F && pausePeak < .0001F;
}
bool Audio::ready() const { return impl_->ready; }
int Audio::loadedSounds() const { return int(std::count_if(impl_->groups.begin(), impl_->groups.end(), [](const auto& g) { return !g.voices.empty(); })) + (impl_->thrust ? 1 : 0) + (impl_->scrape ? 1 : 0) + (impl_->motor ? 1 : 0); }
int Audio::soundKinds() { return SoundCount + 3; }
void Audio::update(const relay::Game& game, float dt, bool playing) { impl_->update(game, dt, playing); }
void Audio::setListener(glm::vec3 eye, glm::vec3 right) {
  impl_->listener = eye;
  right.y = 0;
  impl_->listenerRight = glm::length(right) > .001F ? glm::normalize(right) : glm::vec3(1,0,0);
  impl_->hasListener = true;
}
void Audio::menuMove() { impl_->play(MenuMove, .4F); }
void Audio::menuConfirm() { impl_->play(MenuConfirm, .55F); }
void Audio::menuBack() { impl_->play(MenuBack, .5F); }

}  // namespace craft::app
