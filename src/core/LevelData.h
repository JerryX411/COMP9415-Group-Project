#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace relay {
// A level as the map editor (RelayEditor) edits it and assets/levels/levelN.txt stores it.
// Positions are floor coordinates (x, z); heights are fixed by each object type. The voxels
// are `height` layers above the floor (y = 1..height, 3 to 8) inside the room, so a map can have
// platforms, bridges and overhangs; the floor and the ring of outer walls (3 high) around the room
// are generated from the room bounds.
struct LevelData {
  static constexpr int minHeight = 3, maxHeight = 8;
  // Voxel materials: 0 empty, 2 wall, 3 low block (other values are kept as they are).
  enum : std::uint8_t { Empty = 0, Wall = 2, Low = 3 };
  struct Trap { glm::vec2 min{}, max{}; float offset = 0; };
  struct Switch { glm::vec2 at{}; int trap = 0; };
  struct Checkpoint { glm::vec2 at{}, respawn{}; };
  struct Mover { glm::vec2 a{}, b{}; float period = 4, offset = 0; };
  // A blast door across a wall opening: halfWidth along x (or z when alongZ).
  struct Door { glm::vec2 at{}; float halfWidth = 1; bool alongZ = false; };
  struct Plate { glm::vec2 at{}; int door = 0; };
  struct Pad { glm::vec2 min{}, max{}, direction{1, 0}; };
  // kind: 0 spring boots, 1 shield, 2 clock (same order as PowerUp).
  struct Pickup { glm::vec2 at{}; int kind = 0; };

  std::string name, message;
  int minX = 4, minZ = 4, maxX = 28, maxZ = 28;  // Floor cells minX..maxX-1, minZ..maxZ-1.
  float time = 150;                              // Countdown seconds (ignored in training).
  glm::vec2 spawn{7, 24};
  float spawnYaw = 0;
  glm::vec2 gate{25, 7};
  int height = 3;                   // Voxel layers above the floor.
  std::vector<std::uint8_t> cells;  // height * depth * width, see index().

  std::vector<glm::vec2> cores;
  std::vector<Trap> traps;
  std::vector<Switch> switches;
  std::vector<Checkpoint> checkpoints;
  std::vector<Mover> movers;
  std::vector<Door> doors;
  std::vector<Plate> plates;
  std::vector<Pad> pads;
  std::vector<Pickup> pickups;

  int width() const { return maxX - minX; }
  int depth() const { return maxZ - minZ; }
  bool inside(int x, int z) const { return x >= minX && x < maxX && z >= minZ && z < maxZ; }
  std::size_t index(int x, int y, int z) const { return (std::size_t(y - 1) * depth() + std::size_t(z - minZ)) * width() + std::size_t(x - minX); }
  // Voxel at world (x, y, z), y in 1..height; 0 outside the room.
  std::uint8_t cell(int x, int y, int z) const;
  void setCell(int x, int y, int z, std::uint8_t material);
  // Change the room bounds, keeping the voxels that still fit.
  void resize(int newMinX, int newMinZ, int newMaxX, int newMaxZ);
  // Change the number of layers (minHeight..maxHeight), keeping the voxels that still fit.
  void setHeight(int layers);
  // The highest layer with any voxel in it (0 when empty).
  int topLayer() const;

  bool save(const std::filesystem::path& path, std::string* error = nullptr) const;
  static bool load(const std::filesystem::path& path, LevelData& out, std::string* error = nullptr);
  // The file name for a level index (level0.txt ...).
  static std::string fileName(int level) { return "level" + std::to_string(level) + ".txt"; }
};
}  // namespace relay
