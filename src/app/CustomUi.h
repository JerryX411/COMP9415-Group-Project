#pragma once

#include <array>

namespace craft::app {

struct CustomObjectControls {
  std::array<float, 2> rotationDegrees{0.0F, 0.0F};
  std::array<float, 3> colourTint{1.0F, 1.0F, 1.0F};
};

// Student-facing extension point for assignment-specific Dear ImGui controls.
// It deliberately has no access to SceneBenchmark internals.
class CustomUi {
 public:
  // Draws only the custom component inside an existing ImGui window.
  void drawContents(bool interactionDisabled);

  [[nodiscard]] const CustomObjectControls& objectControls() const noexcept;

 private:
  CustomObjectControls objectControls_;
};

}  // namespace craft::app
