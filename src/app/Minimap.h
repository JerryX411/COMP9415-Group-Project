#pragma once

#include "imgui.h"

namespace relay { class Game; }

namespace craft::app {

struct MinimapCircle {
  ImVec2 centre{};
  float radius = 0;
  bool overlaps(ImVec2 point, float padding) const;
};

// Read-only HUD for the tutorial showcase and Helios. World +X points right and +Z points down.
MinimapCircle drawYardMinimap(ImDrawList& draw, const relay::Game& game,
                               ImVec2 display, float scale);

} // namespace craft::app
