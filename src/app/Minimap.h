#pragma once

#include "imgui.h"

namespace relay { class Game; }

namespace craft::app {

struct MinimapCircle {
  ImVec2 centre{};
  float radius = 0;
  bool overlaps(ImVec2 point, float padding) const;
};

// Tracks 70% of the view turn and settles to the view heading once it stops.
class MinimapHeading {
 public:
  float update(float cameraYaw, float dt, int sceneRevision);

 private:
  int revision_ = -1;
  float yaw_ = 0, previousCameraYaw_ = 0, stillTime_ = 0;
};

// Read-only map HUD. The displayed view heading points up.
MinimapCircle drawMinimap(ImDrawList& draw, const relay::Game& game,
                               ImVec2 display, float scale, float mapYaw);

} // namespace craft::app
