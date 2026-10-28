#include "app/SceneBenchmark.h"

#include <cstdlib>
#include <string_view>

#include "graphics/OpenGL.h"

namespace craft::app {

void SceneBenchmark::start() {
  running_ = true;
  complete_ = false;
  timingThisFrame_ = false;
  warmupFrames_ = 0;
  measuredFrames_ = 0;
  totalMilliseconds_ = 0.0;
}

void SceneBenchmark::beginSceneFrame() {
  timingThisFrame_ = running_ && warmupFrames_ >= kWarmupFrames;
  if (!timingThisFrame_) {
    return;
  }

  // Drain earlier GPU work so it cannot be charged to this scene frame.
  glFinish();
  frameStart_ = Clock::now();
}

bool SceneBenchmark::endSceneFrame() {
  if (!running_) {
    return false;
  }

  if (warmupFrames_ < kWarmupFrames) {
    ++warmupFrames_;
    return false;
  }

  if (!timingThisFrame_) {
    return false;
  }

  // Wait until the scene draw submitted between begin/end is complete.
  glFinish();
  const std::chrono::duration<double, std::milli> elapsed =
      Clock::now() - frameStart_;
  totalMilliseconds_ += elapsed.count();
  ++measuredFrames_;
  timingThisFrame_ = false;

  if (measuredFrames_ == kMeasuredFrames) {
    running_ = false;
    complete_ = true;
    return true;
  }
  return false;
}

BenchmarkSnapshot SceneBenchmark::snapshot() const noexcept {
  return {
      .running = running_,
      .complete = complete_,
      .warmupFrames = warmupFrames_,
      .requiredWarmupFrames = kWarmupFrames,
      .measuredFrames = measuredFrames_,
      .requiredMeasuredFrames = kMeasuredFrames,
      .averageMilliseconds =
          measuredFrames_ > 0
              ? totalMilliseconds_ / static_cast<double>(measuredFrames_)
              : 0.0,
      .averageFramesPerSecond = totalMilliseconds_ > 0.0
                                    ? static_cast<double>(measuredFrames_) *
                                          1000.0 / totalMilliseconds_
                                    : 0.0,
  };
}

bool benchmarkRequestedFromEnvironment() {
  const char* value = std::getenv("CRAFT_BENCHMARK");
  if (value == nullptr) {
    return false;
  }
  const std::string_view setting(value);
  return setting == "1" || setting == "true" || setting == "TRUE" ||
         setting == "on" || setting == "ON";
}

}  // namespace craft::app
