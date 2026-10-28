#pragma once

#include <string>
#include <utility>

struct GLFWwindow;

namespace craft::graphics {

// Owns one GLFW window and the OpenGL context attached to it.
// Create this object before, and destroy it after, all OpenGL resources.
class Window {
 public:
  // Creates an OpenGL 3.3 core-profile window with the requested size.
  Window(int width, int height, const std::string& title);
  ~Window();

  // A native window and its OpenGL context cannot have two owners.
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  // Returns true after the user requests that the window close.
  [[nodiscard]] bool shouldClose() const;

  // Processes input and window events, including ESC to close.
  void pollEvents();

  // Controls presentation timing. Benchmarks should disable VSync while
  // measuring scene rendering and restore it afterwards.
  void setVSync(bool enabled);

  // Resizes the window until its drawable framebuffer has the requested
  // physical-pixel dimensions, including on high-DPI displays.
  void setFramebufferSize(int width, int height);

  // Allows application code (for example a completed CLI benchmark) to close.
  void requestClose();

  // Presents the completed back buffer on the screen.
  void swapBuffers() const;

  // Returns the drawable size in pixels, which may differ from window size.
  [[nodiscard]] std::pair<int, int> framebufferSize() const;

  // Provides the GLFW handle required by libraries such as ImGui.
  [[nodiscard]] GLFWwindow* nativeHandle() const noexcept;

 private:
  GLFWwindow* handle_ = nullptr;
};

}  // namespace craft::graphics
