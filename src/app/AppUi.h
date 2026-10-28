#pragma once

#include "app/SceneBenchmark.h"
#include "graphics/RelayRenderer.h"

namespace craft::graphics {
class Window;
}

namespace craft::app {

// Owns the Dear ImGui lifecycle and composes one compact control window.
// Assignment-specific controls remain isolated in CustomUi.
class AppUi {
 public:
  explicit AppUi(graphics::Window& window);
  ~AppUi();

  AppUi(const AppUi&) = delete;
  AppUi& operator=(const AppUi&) = delete;

  void beginFrame();
  // Returns true when the course-provided benchmark section requests a run.
  [[nodiscard]] bool draw(const BenchmarkSnapshot& benchmark, relay::Game& game,
                          relay::RenderOptions& options, const relay::Renderer& renderer,
                          bool focused);
  void render();


 private:
  [[nodiscard]] bool drawBenchmarkSection(
      const BenchmarkSnapshot& benchmark) const;

};

}  // namespace craft::app
