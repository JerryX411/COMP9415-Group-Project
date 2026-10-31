#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace relay {
class Game;
}

namespace craft::app {

// Sound effects: portable CC0 samples plus an optional, git-ignored local movement pack, played through
// miniaudio. update() watches the game's presentation signals each frame (the same ages and
// counters the camera reacts to) and plays what just happened; the menus report their own moves.
// If no audio device opens, everything stays silent.
class Audio {
 public:
  explicit Audio(bool offline = false);
  ~Audio();
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  bool ready() const;
  // Sounds that loaded (all of them when assets/sounds is complete), out of soundKinds().
  int loadedSounds() const;
  static int soundKinds();
  // Offline mixer smoke test and listening reference; no audio device required.
  bool renderDemo(const char* path);
  // playing: gameplay is on screen; pausing stops gameplay voices and syncs event history.
  void update(const relay::Game& game, float dt, bool playing);
  void setListener(glm::vec3 eye, glm::vec3 right);
  void menuMove();
  void menuConfirm();
  void menuBack();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace craft::app
