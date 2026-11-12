#pragma once

#include <algorithm>
#include <glm/glm.hpp>

namespace relay {
// Shared dimensions and motion for the exit's collision bodies and model joints.
struct ExitGate {
  static constexpr float openingSeconds = .85F;
  static float openness(float progress) {
    const float t = std::clamp(progress, 0.F, 1.F);
    return t*t*t*(t*(t*6.F-15.F)+10.F); // Zero velocity/acceleration at both ends.
  }
  static glm::vec3 panelCentre(float side, float open) {
    return {side*(.8F+1.6F*std::clamp(open,0.F,1.F)),1.4F,0};
  }
  static glm::vec3 panelSize() { return {1.6F,2.8F,.4F}; }
  static glm::vec3 postCentre(float side) { return {side*1.8F,1.5F,0}; }
  static glm::vec3 postSize() { return {.4F,3.F,.6F}; }
};
} // namespace relay
