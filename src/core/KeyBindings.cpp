#include "core/KeyBindings.h"
#include <algorithm>
#include <fstream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace relay {
namespace {
constexpr const char* ids[KeyBindings::count]{"forward", "back", "left", "right", "jump", "sprint", "crouch", "interact", "restart"};
constexpr const char* names[KeyBindings::count]{"MOVE FORWARD", "MOVE BACK", "MOVE LEFT", "MOVE RIGHT", "JUMP", "SPRINT", "CROUCH / SLIDE", "INTERACT", "RESTART LEVEL"};
}

std::array<int, KeyBindings::count> KeyBindings::defaults() {
  return {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_C, GLFW_KEY_E, GLFW_KEY_R};
}

bool KeyBindings::reserved(int key) {
  return key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_F1 ||
         key == GLFW_KEY_F2 || key == GLFW_KEY_F3 || key == GLFW_KEY_1 || key == GLFW_KEY_2;
}

bool KeyBindings::assign(KeyAction action, int key) {
  const auto& supported = supportedKeys();
  if (reserved(key) || std::find(supported.begin(), supported.end(), key) == supported.end()) return false;
  auto& slot = keys[std::size_t(action)];
  for (auto& other : keys) if (other == key && &other != &slot) other = slot;
  slot = key;
  return true;
}

bool KeyBindings::load(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) return false;
  auto loaded = defaults();
  std::string line;
  while (std::getline(file, line)) {
    const auto split = line.find('=');
    if (split == std::string::npos) continue;
    const auto id = line.substr(0, split);
    for (int i = 0; i < count; ++i) if (id == ids[i]) {
      try { loaded[std::size_t(i)] = std::stoi(line.substr(split + 1)); } catch (...) {}
    }
  }
  // Apply through assign() so a hand-edited file cannot create duplicates or reserved keys.
  keys = defaults();
  for (int i = 0; i < count; ++i) assign(KeyAction(i), loaded[std::size_t(i)]);
  return true;
}

bool KeyBindings::save(const std::filesystem::path& path) const {
  std::ofstream file(path);
  if (!file) return false;
  file << "# Voxel Relay key bindings (GLFW key codes)\n";
  for (int i = 0; i < count; ++i) file << ids[i] << '=' << keys[std::size_t(i)] << '\n';
  return bool(file);
}

const char* KeyBindings::actionName(KeyAction action) { return names[int(action)]; }

std::string KeyBindings::keyName(int key) {
  if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return std::string(1, char('A' + key - GLFW_KEY_A));
  if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return std::string(1, char('0' + key - GLFW_KEY_0));
  if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return "NUM " + std::to_string(key - GLFW_KEY_KP_0);
  switch (key) {
    case GLFW_KEY_SPACE: return "SPACE";
    case GLFW_KEY_LEFT_SHIFT: return "L-SHIFT";
    case GLFW_KEY_RIGHT_SHIFT: return "R-SHIFT";
    case GLFW_KEY_LEFT_CONTROL: return "L-CTRL";
    case GLFW_KEY_RIGHT_CONTROL: return "R-CTRL";
    case GLFW_KEY_LEFT_ALT: return "L-ALT";
    case GLFW_KEY_RIGHT_ALT: return "R-ALT";
    case GLFW_KEY_TAB: return "TAB";
    case GLFW_KEY_CAPS_LOCK: return "CAPS";
    case GLFW_KEY_BACKSPACE: return "BACKSPACE";
    case GLFW_KEY_ENTER: return "ENTER";
    case GLFW_KEY_ESCAPE: return "ESC";
    case GLFW_KEY_UP: return "UP";
    case GLFW_KEY_DOWN: return "DOWN";
    case GLFW_KEY_LEFT: return "LEFT";
    case GLFW_KEY_RIGHT: return "RIGHT";
    case GLFW_KEY_INSERT: return "INSERT";
    case GLFW_KEY_DELETE: return "DELETE";
    case GLFW_KEY_HOME: return "HOME";
    case GLFW_KEY_END: return "END";
    case GLFW_KEY_PAGE_UP: return "PG UP";
    case GLFW_KEY_PAGE_DOWN: return "PG DN";
    case GLFW_KEY_APOSTROPHE: return "'";
    case GLFW_KEY_COMMA: return ",";
    case GLFW_KEY_MINUS: return "-";
    case GLFW_KEY_PERIOD: return ".";
    case GLFW_KEY_SLASH: return "/";
    case GLFW_KEY_SEMICOLON: return ";";
    case GLFW_KEY_EQUAL: return "=";
    case GLFW_KEY_LEFT_BRACKET: return "[";
    case GLFW_KEY_BACKSLASH: return "\\";
    case GLFW_KEY_RIGHT_BRACKET: return "]";
    case GLFW_KEY_GRAVE_ACCENT: return "`";
    default: return "?";
  }
}

const std::vector<int>& KeyBindings::supportedKeys() {
  static const std::vector<int> keys = [] {
    std::vector<int> list;
    for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; ++k) list.push_back(k);
    for (int k = GLFW_KEY_0; k <= GLFW_KEY_9; ++k) list.push_back(k);
    for (int k = GLFW_KEY_F1; k <= GLFW_KEY_F12; ++k) list.push_back(k);
    for (int k = GLFW_KEY_KP_0; k <= GLFW_KEY_KP_9; ++k) list.push_back(k);
    for (int k : {GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL,
                  GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT, GLFW_KEY_TAB, GLFW_KEY_CAPS_LOCK, GLFW_KEY_BACKSPACE, GLFW_KEY_ENTER,
                  GLFW_KEY_KP_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_INSERT,
                  GLFW_KEY_DELETE, GLFW_KEY_HOME, GLFW_KEY_END, GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN, GLFW_KEY_APOSTROPHE,
                  GLFW_KEY_COMMA, GLFW_KEY_MINUS, GLFW_KEY_PERIOD, GLFW_KEY_SLASH, GLFW_KEY_SEMICOLON, GLFW_KEY_EQUAL,
                  GLFW_KEY_LEFT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_GRAVE_ACCENT})
      list.push_back(k);
    return list;
  }();
  return keys;
}

KeyBindings& keyBindings() {
  static KeyBindings bindings;
  return bindings;
}
std::filesystem::path settingsPath() { return std::filesystem::current_path() / "voxel_relay_keys.ini"; }
}  // namespace relay
