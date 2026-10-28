#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace relay {
// Rebindable gameplay actions (camera stays on the mouse; Esc and menus are fixed).
enum class KeyAction : int { Forward, Back, Left, Right, Jump, Sprint, Crouch, Interact, Restart, Count };

struct KeyBindings {
  static constexpr int count = int(KeyAction::Count);
  std::array<int, count> keys = defaults();

  static std::array<int, count> defaults();
  int operator[](KeyAction action) const { return keys[std::size_t(action)]; }
  // Binds key to action. An action that already used the key takes the old key in
  // exchange, so no key is ever bound twice. Reserved keys are refused.
  bool assign(KeyAction action, int key);
  void reset() { keys = defaults(); }
  // Esc and Enter drive the menus; F1-F3, 1 and 2 are debug and level shortcuts.
  static bool reserved(int key);
  // Plain "action=glfwKey" lines; unknown or invalid entries keep their defaults.
  bool load(const std::filesystem::path& path);
  bool save(const std::filesystem::path& path) const;

  static const char* actionName(KeyAction action);
  static std::string keyName(int key);
  // Every key the input layer samples: bindable keys plus the fixed menu keys.
  static const std::vector<int>& supportedKeys();
};

// The bindings in effect for this session, and where they persist.
KeyBindings& keyBindings();
std::filesystem::path settingsPath();
}  // namespace relay
