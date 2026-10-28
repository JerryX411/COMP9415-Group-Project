#pragma once
#include <algorithm>
namespace relay {
// Blackout between levels: fade out, load at full black behind a title card,
// then fade back in. Gameplay and input wait until the fade-in begins.
struct LevelTransition {
  static constexpr float fadeOut=.6F, hold=1.8F, fadeIn=.8F;
  float elapsed=-1;
  int target=0;
  bool active() const { return elapsed>=0; }
  void start(int level) { if(active()) return; target=level; elapsed=0; }
  // True on the single update that reaches full black: load the level then.
  bool update(float dt) {
    if(!active()) return false;
    const float before=elapsed; elapsed+=std::max(0.F,dt);
    const bool load=before<fadeOut&&elapsed>=fadeOut;
    if(elapsed>=fadeOut+hold+fadeIn) elapsed=-1;
    return load;
  }
  bool blocksGameplay() const { return active()&&elapsed<fadeOut+hold; }
  float black() const {
    if(!active()) return 0;
    if(elapsed<fadeOut) return elapsed/fadeOut;
    if(elapsed<fadeOut+hold) return 1;
    return std::clamp(1-(elapsed-fadeOut-hold)/fadeIn,0.F,1.F);
  }
  // Title card: appears just before full black, leaves early in the fade-in.
  float card() const {
    if(!active()) return 0;
    return std::clamp(std::min((elapsed-fadeOut*.7F)/.25F,(fadeOut+hold+fadeIn*.45F-elapsed)/.3F),0.F,1.F);
  }
  float progress() const { return active()?std::clamp(elapsed/(fadeOut+hold),0.F,1.F):0; }
};
}
