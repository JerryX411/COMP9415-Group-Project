#include "app/TitleScreen.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "core/AssetPath.h"
#include "graphics/OpenGL.h"
#include "graphics/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace craft::app {

namespace {

constexpr std::string_view kTitle = "VOXEL RELAY";
constexpr std::string_view kPrompt = "PRESS THE ENTER KEY";

// Replace this image to change the cover. Any aspect ratio works: it is
// scaled to fill the window without stretching, and the overflow is cropped.
constexpr const char* kBackgroundImage = "textures/title_background.png";

// Vertical centres of the title and prompt, as fractions of window height.
constexpr float kTitleCentreY = 0.45F;
constexpr float kPromptCentreY = 0.8F;

// Default text sizes: the height of one font cell as a fraction of window
// height (capital letters fill roughly 40% of a cell). Change these, or call
// TitleScreen::setTextSizes, to make the title or prompt larger or smaller.
constexpr float kDefaultTitleSize = 0.15F;
constexpr float kDefaultPromptSize = 0.042F;

// Text is shrunk if needed so it never exceeds this fraction of window width.
constexpr float kTitleMaxWidth = 0.82F;
constexpr float kPromptMaxWidth = 0.9F;

// Fits a line of text: the cell width for a cell height of size * height,
// reduced if the line would be wider than maxWidth * width.
float fittedGlyphWidth(std::string_view text, float size, float maxWidth,
                       float spacing, float width, float height) {
  const auto length = static_cast<float>(text.size());
  return std::min(0.5F * size * height,
                  maxWidth * width / (length + (length - 1.0F) * spacing));
}

// Timeline, in seconds.
constexpr double kTitleFadeIn = 1.4;
constexpr double kPromptDelay = 1.6;
constexpr double kPromptFadeIn = 0.4;
constexpr double kPromptPulsePeriod = 1.8;
constexpr double kConfirmHoldDuration = 0.3;
constexpr double kFadeOutDuration = 0.6;

// The title's letters start spread out and settle into place as it fades in.
constexpr float kTitleSpacingStart = 0.7F;
constexpr float kTitleSpacingEnd = 0.12F;
constexpr float kPromptSpacing = 0.45F;

constexpr double kPi = 3.14159265358979323846;

float clamp01(double value) {
  return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

float easeOutCubic(float t) {
  const float inverse = 1.0F - t;
  return 1.0F - inverse * inverse * inverse;
}

}  // namespace

TitleScreen::TitleScreen(graphics::Window& window)
    : window_(window),
      backgroundShader_(core::assetPath("shaders/title_background.vert"),
                        core::assetPath("shaders/title_background.frag")),
      background_(core::assetPath(kBackgroundImage),
                  graphics::Texture::Magnification::Nearest),
      text_(core::assetPath("textures/font.png"),
            core::assetPath("shaders/text.vert"),
            core::assetPath("shaders/text.frag")),
      titleSize_(kDefaultTitleSize),
      promptSize_(kDefaultPromptSize),
      startTime_(glfwGetTime()),
      now_(startTime_) {
  // Core profile needs a bound VAO even though the background quad has no vertex attributes.
  glGenVertexArrays(1, &backgroundVertexArray_);
}

TitleScreen::~TitleScreen() {
  if (backgroundVertexArray_ != 0) {
    glDeleteVertexArrays(1, &backgroundVertexArray_);
  }
}

void TitleScreen::update() {
  now_ = glfwGetTime();

  GLFWwindow* handle = window_.nativeHandle();
  const bool enterDown = glfwGetKey(handle, GLFW_KEY_ENTER) == GLFW_PRESS ||
                         glfwGetKey(handle, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
  if (enterDown && !enterWasDown_ && confirmTime_ < 0.0) {
    confirmTime_ = now_;
    // Pressing early skips straight to the fully revealed title.
    startTime_ = std::min(startTime_, now_ - kPromptDelay);
  }
  enterWasDown_ = enterDown;
}

void TitleScreen::draw(int framebufferWidth, int framebufferHeight) {
  const double elapsed = now_ - startTime_;
  const bool confirmed = confirmTime_ >= 0.0;
  const double sinceConfirm = confirmed ? now_ - confirmTime_ : 0.0;
  const float fade =
      confirmed
          ? clamp01((sinceConfirm - kConfirmHoldDuration) / kFadeOutDuration)
          : 0.0F;

  glViewport(0, 0, framebufferWidth, framebufferHeight);
  glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);

  const auto width = static_cast<float>(framebufferWidth);
  const auto height = static_cast<float>(framebufferHeight);

  // Cover the window: show the whole image along the tighter axis and crop
  // the middle along the other, so the image is never stretched.
  const float imageAspect = static_cast<float>(background_.width()) /
                            static_cast<float>(background_.height());
  const float screenAspect = width / height;
  const glm::vec2 uvScale = screenAspect > imageAspect
                                ? glm::vec2(1.0F, imageAspect / screenAspect)
                                : glm::vec2(screenAspect / imageAspect, 1.0F);
  backgroundShader_.use();
  backgroundShader_.setInt("uImage", 0);
  backgroundShader_.setVec2("uUvScale", uvScale);
  background_.bind(0);
  glBindVertexArray(backgroundVertexArray_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);

  text_.begin(framebufferWidth, framebufferHeight);

  // Title: sized from the window height, but once settled never wider than
  // kTitleMaxWidth of the window.
  const float titleReveal = easeOutCubic(clamp01(elapsed / kTitleFadeIn));
  const float titleAlpha = titleReveal;
  const float titleGlyph =
      fittedGlyphWidth(kTitle, titleSize_, kTitleMaxWidth, kTitleSpacingEnd,
                       width, height);
  const float titleSpacing =
      kTitleSpacingStart +
      (kTitleSpacingEnd - kTitleSpacingStart) * titleReveal;
  const float titleWidth =
      graphics::TextRenderer::measure(kTitle, titleGlyph, titleSpacing);
  const float titleX = (width - titleWidth) * 0.5F;
  const float titleY = height * kTitleCentreY - titleGlyph;
  const float titleShadow = std::max(2.0F, titleGlyph * 0.06F);
  text_.drawText(kTitle, titleX + titleShadow, titleY + titleShadow,
                 titleGlyph, glm::vec4(0.0F, 0.0F, 0.0F, 0.5F * titleAlpha),
                 titleSpacing);
  // The atlas font is thin, so overdraw it with small offsets for weight.
  const float titleWeight = std::max(1.0F, std::round(titleGlyph * 0.035F));
  for (float offset = 0.0F; offset <= titleWeight; offset += 1.0F) {
    text_.drawText(kTitle, titleX + offset, titleY, titleGlyph,
                   glm::vec4(1.0F, 1.0F, 1.0F, titleAlpha), titleSpacing);
  }

  // A rule under the title grows outwards from the centre.
  const float ruleWidth = graphics::TextRenderer::measure(
                              kTitle, titleGlyph, kTitleSpacingEnd) *
                          titleReveal;
  const float ruleHeight = std::max(1.0F, std::round(height * 0.0025F));
  text_.drawRect((width - ruleWidth) * 0.5F, titleY + 1.75F * titleGlyph,
                 ruleWidth, ruleHeight,
                 glm::vec4(1.0F, 1.0F, 1.0F, 0.75F * titleAlpha));

  // Prompt: slow pulse while waiting, hidden as soon as Enter is pressed.
  float promptAlpha = 0.0F;
  if (!confirmed && elapsed >= kPromptDelay) {
    const double sincePrompt = elapsed - kPromptDelay;
    const double pulse =
        0.5 + 0.5 * std::cos(2.0 * kPi * sincePrompt / kPromptPulsePeriod);
    promptAlpha = clamp01(sincePrompt / kPromptFadeIn) *
                  static_cast<float>(0.3 + 0.7 * pulse);
  }

  const float promptGlyph = std::max(
      4.0F, fittedGlyphWidth(kPrompt, promptSize_, kPromptMaxWidth,
                             kPromptSpacing, width, height));
  const float promptWidth =
      graphics::TextRenderer::measure(kPrompt, promptGlyph, kPromptSpacing);
  const float promptX = (width - promptWidth) * 0.5F;
  const float promptY = height * kPromptCentreY - promptGlyph;
  const float promptShadow = std::max(1.0F, promptGlyph * 0.1F);
  text_.drawText(kPrompt, promptX + promptShadow, promptY + promptShadow,
                 promptGlyph, glm::vec4(0.0F, 0.0F, 0.0F, 0.7F * promptAlpha),
                 kPromptSpacing);
  text_.drawText(kPrompt, promptX, promptY, promptGlyph,
                 glm::vec4(1.0F, 1.0F, 1.0F, promptAlpha), kPromptSpacing);

  // Fade to black over everything once Enter has been pressed.
  text_.drawRect(0.0F, 0.0F, width, height, glm::vec4(0.0F, 0.0F, 0.0F, fade));

  text_.end();
}

void TitleScreen::setTextSizes(float titleSize, float promptSize) {
  titleSize_ = std::max(0.0F, titleSize);
  promptSize_ = std::max(0.0F, promptSize);
}

bool TitleScreen::finished() const noexcept {
  return confirmTime_ >= 0.0 &&
         now_ - confirmTime_ >= kConfirmHoldDuration + kFadeOutDuration;
}

}  // namespace craft::app
