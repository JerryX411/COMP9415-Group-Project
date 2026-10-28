#include "core/LevelData.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace relay {
std::uint8_t LevelData::cell(int x, int y, int z) const {
  if (y < 1 || y > height || !inside(x, z) || cells.size() != std::size_t(height * width() * depth())) return Empty;
  return cells[index(x, y, z)];
}
void LevelData::setCell(int x, int y, int z, std::uint8_t material) {
  if (y < 1 || y > height || !inside(x, z)) return;
  if (cells.size() != std::size_t(height * width() * depth())) cells.assign(std::size_t(height * width() * depth()), Empty);
  cells[index(x, y, z)] = material;
}
void LevelData::resize(int newMinX, int newMinZ, int newMaxX, int newMaxZ) {
  LevelData old = *this;
  minX = newMinX; minZ = newMinZ; maxX = std::max(newMinX + 1, newMaxX); maxZ = std::max(newMinZ + 1, newMaxZ);
  cells.assign(std::size_t(height * width() * depth()), Empty);
  for (int y = 1; y <= height; ++y)
    for (int z = minZ; z < maxZ; ++z)
      for (int x = minX; x < maxX; ++x) cells[index(x, y, z)] = old.cell(x, y, z);
}
void LevelData::setHeight(int layers) {
  LevelData old = *this;
  height = std::clamp(layers, minHeight, maxHeight);
  cells.assign(std::size_t(height * width() * depth()), Empty);
  for (int y = 1; y <= height; ++y)
    for (int z = minZ; z < maxZ; ++z)
      for (int x = minX; x < maxX; ++x) cells[index(x, y, z)] = old.cell(x, y, z);
}
int LevelData::topLayer() const {
  for (int y = height; y >= 1; --y)
    for (int z = minZ; z < maxZ; ++z)
      for (int x = minX; x < maxX; ++x) if (cell(x, y, z)) return y;
  return 0;
}

namespace {
char glyph(std::uint8_t material) { return material == LevelData::Empty ? '.' : material == LevelData::Wall ? '#' : material == LevelData::Low ? '=' : char('0' + std::min<int>(material, 9)); }
std::uint8_t material(char c) { return c == '#' ? LevelData::Wall : c == '=' ? LevelData::Low : c >= '0' && c <= '9' ? std::uint8_t(c - '0') : LevelData::Empty; }
const char* pickupName(int kind) { return kind == 1 ? "shield" : kind == 2 ? "clock" : "spring"; }
int pickupKind(const std::string& name) { return name == "shield" ? 1 : name == "clock" ? 2 : 0; }
bool fail(std::string* error, const std::string& message) { if (error) *error = message; return false; }
}  // namespace

bool LevelData::save(const std::filesystem::path& path, std::string* error) const {
  std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary);
  if (!out) return fail(error, "cannot write " + path.string());
  out << "# Voxel Relay level. Edit with RelayEditor, or by hand: one item per line, x z on the floor.\n";
  out << "name " << name << "\nmessage " << message << '\n';
  out << "room " << minX << ' ' << minZ << ' ' << maxX << ' ' << maxZ << '\n';
  out << "height " << height << '\n';
  out << "time " << time << '\n';
  out << "spawn " << spawn.x << ' ' << spawn.y << ' ' << spawnYaw << '\n';
  out << "gate " << gate.x << ' ' << gate.y << '\n';
  // Voxel layers: one row per z (north to south), one character per x. '.' empty, '#' wall,
  // '=' low block.
  for (int y = 1; y <= height; ++y) {
    out << "layer " << y << '\n';
    for (int z = minZ; z < maxZ; ++z) {
      std::string row;
      for (int x = minX; x < maxX; ++x) row += glyph(cell(x, y, z));
      out << row << '\n';
    }
  }
  for (const auto& c : cores) out << "core " << c.x << ' ' << c.y << '\n';
  for (const auto& t : traps) out << "trap " << t.min.x << ' ' << t.min.y << ' ' << t.max.x << ' ' << t.max.y << ' ' << t.offset << '\n';
  for (const auto& s : switches) out << "switch " << s.at.x << ' ' << s.at.y << ' ' << s.trap << '\n';
  for (const auto& c : checkpoints) out << "checkpoint " << c.at.x << ' ' << c.at.y << ' ' << c.respawn.x << ' ' << c.respawn.y << '\n';
  for (const auto& m : movers) out << "mover " << m.a.x << ' ' << m.a.y << ' ' << m.b.x << ' ' << m.b.y << ' ' << m.period << ' ' << m.offset << '\n';
  for (const auto& d : doors) out << "door " << d.at.x << ' ' << d.at.y << ' ' << d.halfWidth << ' ' << (d.alongZ ? 'z' : 'x') << '\n';
  for (const auto& p : plates) out << "plate " << p.at.x << ' ' << p.at.y << ' ' << p.door << '\n';
  for (const auto& p : pads) out << "pad " << p.min.x << ' ' << p.min.y << ' ' << p.max.x << ' ' << p.max.y << ' ' << p.direction.x << ' ' << p.direction.y << '\n';
  for (const auto& p : pickups) out << "pickup " << p.at.x << ' ' << p.at.y << ' ' << pickupName(p.kind) << '\n';
  if (!out) return fail(error, "write failed: " + path.string());
  return true;
}

bool LevelData::load(const std::filesystem::path& path, LevelData& result, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return fail(error, "cannot open " + path.string());
  LevelData d;
  d.cells.clear();
  std::string line;
  int lineNumber = 0, layer = 0, row = 0;
  bool sized = false;
  while (std::getline(in, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (layer > 0) {
      // Inside a voxel layer: rows until the layer is full.
      for (int x = 0; x < d.width() && x < int(line.size()); ++x) d.setCell(d.minX + x, layer, d.minZ + row, material(line[std::size_t(x)]));
      if (++row >= d.depth()) layer = 0;
      continue;
    }
    if (line.empty() || line[0] == '#') continue;
    std::istringstream s(line);
    std::string key; s >> key;
    const auto bad = [&] { return fail(error, path.filename().string() + " line " + std::to_string(lineNumber) + ": cannot read '" + line + "'"); };
    if (key == "name" || key == "message") {
      std::string text; std::getline(s, text);
      if (!text.empty() && text[0] == ' ') text.erase(0, 1);
      (key == "name" ? d.name : d.message) = text;
    } else if (key == "room") {
      if (!(s >> d.minX >> d.minZ >> d.maxX >> d.maxZ) || d.minX < 1 || d.minZ < 1 || d.maxX > 63 || d.maxZ > 63 || d.maxX <= d.minX || d.maxZ <= d.minZ) return bad();
      d.cells.assign(std::size_t(d.height * d.width() * d.depth()), Empty); sized = true;
    } else if (key == "height") {
      int h = 0; if (!sized || !(s >> h) || h < minHeight || h > maxHeight) return bad();
      d.setHeight(h);
    } else if (key == "time") { if (!(s >> d.time)) return bad(); }
    else if (key == "spawn") { if (!(s >> d.spawn.x >> d.spawn.y)) return bad(); s >> d.spawnYaw; }
    else if (key == "gate") { if (!(s >> d.gate.x >> d.gate.y)) return bad(); }
    else if (key == "layer") {
      if (!sized || !(s >> layer) || layer < 1 || layer > maxHeight) return bad();
      if (layer > d.height) d.setHeight(layer);  // A file without a height line grows to fit.
      row = 0;
    } else if (key == "core") { glm::vec2 c; if (!(s >> c.x >> c.y)) return bad(); d.cores.push_back(c); }
    else if (key == "trap") { Trap t; if (!(s >> t.min.x >> t.min.y >> t.max.x >> t.max.y)) return bad(); s >> t.offset; d.traps.push_back(t); }
    else if (key == "switch") { Switch w; if (!(s >> w.at.x >> w.at.y >> w.trap)) return bad(); d.switches.push_back(w); }
    else if (key == "checkpoint") { Checkpoint c; if (!(s >> c.at.x >> c.at.y >> c.respawn.x >> c.respawn.y)) return bad(); d.checkpoints.push_back(c); }
    else if (key == "mover") { Mover m; if (!(s >> m.a.x >> m.a.y >> m.b.x >> m.b.y >> m.period)) return bad(); s >> m.offset; m.period = std::max(.5F, m.period); d.movers.push_back(m); }
    else if (key == "door") { Door o; char axis = 'x'; if (!(s >> o.at.x >> o.at.y >> o.halfWidth)) return bad(); s >> axis; o.alongZ = axis == 'z'; d.doors.push_back(o); }
    else if (key == "plate") { Plate p; if (!(s >> p.at.x >> p.at.y >> p.door)) return bad(); d.plates.push_back(p); }
    else if (key == "pad") { Pad p; if (!(s >> p.min.x >> p.min.y >> p.max.x >> p.max.y >> p.direction.x >> p.direction.y)) return bad(); d.pads.push_back(p); }
    else if (key == "pickup") { Pickup p; std::string kind; if (!(s >> p.at.x >> p.at.y >> kind)) return bad(); p.kind = pickupKind(kind); d.pickups.push_back(p); }
    else return bad();
  }
  if (!sized) return fail(error, path.filename().string() + ": no room line");
  // Links to items that do not exist are dropped rather than crashing the game.
  d.switches.erase(std::remove_if(d.switches.begin(), d.switches.end(), [&](const Switch& w) { return w.trap < 0 || w.trap >= int(d.traps.size()); }), d.switches.end());
  d.plates.erase(std::remove_if(d.plates.begin(), d.plates.end(), [&](const Plate& p) { return p.door < 0 || p.door >= int(d.doors.size()); }), d.plates.end());
  result = std::move(d);
  return true;
}
}  // namespace relay
