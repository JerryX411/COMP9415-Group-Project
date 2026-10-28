#pragma once

#include <chrono>
#include <cstddef>

namespace craft::app {

inline constexpr int kBenchmarkFramebufferWidth = 1920;
inline constexpr int kBenchmarkFramebufferHeight = 1080;

struct BenchmarkSnapshot {
  bool running = false;
  bool complete = false;
  std::size_t warmupFrames = 0;
  std::size_t requiredWarmupFrames = 60;
  std::size_t measuredFrames = 0;
  std::size_t requiredMeasuredFrames = 240;
  double averageMilliseconds = 0.0;
  double averageFramesPerSecond = 0.0;
};

// Times only the scene-rendering boundary selected by the caller.
// It deliberately knows nothing about chunks, meshes, or drawing strategy.
class SceneBenchmark {
 public:
  void start();

  // Call immediately before and after the complete scene render. Dear ImGui
  // rendering and buffer swap must remain outside this pair.
  void beginSceneFrame();
  // Returns true only on the frame that completes the 240th measurement.
  [[nodiscard]] bool endSceneFrame();

  [[nodiscard]] BenchmarkSnapshot snapshot() const noexcept;

 private:
  using Clock = std::chrono::steady_clock;

  static constexpr std::size_t kWarmupFrames = 60;
  static constexpr std::size_t kMeasuredFrames = 240;

  bool running_ = false;
  bool complete_ = false;
  bool timingThisFrame_ = false;
  std::size_t warmupFrames_ = 0;
  std::size_t measuredFrames_ = 0;
  double totalMilliseconds_ = 0.0;
  Clock::time_point frameStart_{};
};

// CRAFT_BENCHMARK=1 starts a reproducible CLI benchmark run.
[[nodiscard]] bool benchmarkRequestedFromEnvironment();

}  // namespace craft::app
