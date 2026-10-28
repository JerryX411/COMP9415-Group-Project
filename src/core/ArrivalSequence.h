#pragma once
#include <algorithm>
namespace relay {
// Cinematic clock is independent of gameplay: countdown, hazards and input wait
// until the camera has returned. Skipping consumes the triggering key edge.
struct ArrivalSequence {
  static constexpr float duration=4.F;
  float elapsed=-1;
  bool active() const { return elapsed>=0&&elapsed<duration; }
  void start() { elapsed=0; }
  void cancel() { elapsed=-1; }
  void update(float dt,bool skip,bool focused) {
    if(!active()||!focused) return;
    elapsed=skip?duration:std::min(duration,elapsed+std::max(0.F,dt));
  }
};
}
