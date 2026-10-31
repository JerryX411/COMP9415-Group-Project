#pragma once

#include "app/SceneBenchmark.h"
#include "graphics/RelayRenderer.h"
#include "imgui.h"
#include <vector>

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
  // Draws the UI, first refreshing the acrylic backdrop (a blurred copy of the scene).
  void render();
  // Level the UI asked to load (restart or switch), or -1; reading clears it.
  int takeLevelRequest() { const int level=levelRequest_; levelRequest_=-1; return level; }
  // Training field-manual popup (armour, reboots, lives). While open the game holds and the
  // cursor is free; Esc, the X or the last page's button closes it.
  bool popupOpen() const { return popupPage_>=0; }
  void openPopup(int page) { popupPage_=page; popupShown_=true; popupOpenedAt_=-10; }


 private:
  [[nodiscard]] bool drawBenchmarkSection(
      const BenchmarkSnapshot& benchmark) const;
  int levelRequest_ = -1;
  float prewarmedScale_ = 0;
  // Training field manual: the page on screen (-1 closed), whether it has been shown, and when.
  int popupPage_ = -1;
  bool popupShown_ = false;
  float popupOpenedAt_ = 0;
  // Sparks shed by the boost meter (screen space).
  struct HudSpark { ImVec2 p, v; float age, life, size; };
  std::vector<HudSpark> hudSparks_;
  float sparkClock_ = 0;
  unsigned sparkSeed_ = 12345u;
  // Acrylic backdrop: four successively halved copies of the frame (the last one is sampled).
  void captureBackdrop();
  unsigned backdropFbo_[4]{}, backdropTex_[4]{};
  int backdropWidth_ = 0, backdropHeight_ = 0;

};

}  // namespace craft::app
