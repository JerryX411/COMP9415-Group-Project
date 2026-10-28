#pragma once

#include "graphics/Shader.h"
#include "graphics/TextRenderer.h"
#include "graphics/Texture.h"

namespace craft::graphics {
class Window;
}

namespace craft::app {

// Start screen shown before the game: the title fades in over a background
// image (assets/textures/title_background.png), then "PRESS THE ENTER KEY"
// pulses until the player confirms.
// Confirming hides the prompt, holds briefly, then fades to black before the
// game starts.
class TitleScreen {
 public:
  explicit TitleScreen(graphics::Window& window);
  ~TitleScreen();

  TitleScreen(const TitleScreen&) = delete;
  TitleScreen& operator=(const TitleScreen&) = delete;

  // Reads input and advances the animation. Call once per frame.
  void update();

  // Draws the whole frame, including clearing the framebuffer.
  void draw(int framebufferWidth, int framebufferHeight);

  // Sets the font cell height of the title and the prompt, as fractions of
  // the window height (defaults 0.15 and 0.042). Either line is still shrunk
  // if it would not fit the window width.
  void setTextSizes(float titleSize, float promptSize);

  // True once the confirm animation has finished and the game should start.
  [[nodiscard]] bool finished() const noexcept;

 private:
  graphics::Window& window_;
  graphics::Shader backgroundShader_;
  graphics::Texture background_;
  graphics::TextRenderer text_;
  unsigned int backgroundVertexArray_ = 0;

  float titleSize_;
  float promptSize_;

  double startTime_ = 0.0;
  double now_ = 0.0;
  double confirmTime_ = -1.0;  // Negative until Enter is pressed.
  bool enterWasDown_ = true;   // Ignore an Enter key already held at launch.
};

}  // namespace craft::app
