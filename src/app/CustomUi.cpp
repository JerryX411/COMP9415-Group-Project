#include "app/CustomUi.h"

#include "imgui.h"

namespace craft::app {

void CustomUi::drawContents(bool interactionDisabled) {
  ImGui::SeparatorText("Custom controls");
  ImGui::BeginDisabled(interactionDisabled);
  ImGui::TextUnformatted("Rotation X / Y");
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::SliderFloat2("##ObjectRotation",
                      objectControls_.rotationDegrees.data(), -180.0F, 180.0F,
                      "%.0f deg");
  ImGui::TextUnformatted("Colour tint");
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::ColorEdit3("##ObjectColour", objectControls_.colourTint.data());
  ImGui::EndDisabled();
}

const CustomObjectControls& CustomUi::objectControls() const noexcept {
  return objectControls_;
}

}  // namespace craft::app
