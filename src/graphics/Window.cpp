#include "graphics/Window.h"

#include "graphics/OpenGL.h"

#define GLFW_INCLUDE_NONE
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <GLFW/glfw3.h>

namespace craft::graphics {

namespace {

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
  // OpenGL must be told which part of the framebuffer receives the image.
  glViewport(0, 0, width, height);
}

}  // namespace

Window::Window(int width, int height, const std::string& title) {
  if (glfwInit() != GLFW_TRUE) {
    throw std::runtime_error("Failed to initialise GLFW");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  // macOS requires forward compatibility for core-profile contexts.
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

  handle_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (handle_ == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create the GLFW window");
  }

  glfwMakeContextCurrent(handle_);

#if !defined(__APPLE__)
  // GLAD loads the OpenGL function pointers supplied by the active context.
  if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
    glfwDestroyWindow(handle_);
    handle_ = nullptr;
    glfwTerminate();
    throw std::runtime_error("Failed to load OpenGL functions with GLAD");
  }
#endif

  glfwSetFramebufferSizeCallback(handle_, framebufferSizeCallback);
  // Wait for vertical refresh when swapping buffers to avoid visible tearing.
  setVSync(true);
}

Window::~Window() {
  if (handle_ != nullptr) {
    glfwDestroyWindow(handle_);
  }
  glfwTerminate();
}

bool Window::shouldClose() const {
  return glfwWindowShouldClose(handle_) == GLFW_TRUE;
}

void Window::pollEvents() {
  glfwPollEvents();

  // Escape belongs to the game's pause/menu flow; the close button still exits.
}

void Window::setVSync(bool enabled) {
  glfwSwapInterval(enabled ? 1 : 0);
}

void Window::setFramebufferSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("Framebuffer dimensions must be positive");
  }

  constexpr int kMaximumResizeAttempts = 4;
  constexpr double kResizeTimeoutSeconds = 0.5;
  for (int attempt = 0; attempt < kMaximumResizeAttempts; ++attempt) {
    const auto [framebufferWidth, framebufferHeight] = framebufferSize();
    if (framebufferWidth == width && framebufferHeight == height) {
      return;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(handle_, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0 || framebufferWidth <= 0 ||
        framebufferHeight <= 0) {
      throw std::runtime_error("Cannot resize an unavailable framebuffer");
    }

    const int targetWindowWidth = std::max(
        1, static_cast<int>(std::lround(
               static_cast<double>(width) * static_cast<double>(windowWidth) /
               static_cast<double>(framebufferWidth))));
    const int targetWindowHeight = std::max(
        1, static_cast<int>(std::lround(
               static_cast<double>(height) * static_cast<double>(windowHeight) /
               static_cast<double>(framebufferHeight))));
    glfwSetWindowSize(handle_, targetWindowWidth, targetWindowHeight);

    // X11/Wayland apply resizes asynchronously, so give the window manager
    // a bounded amount of time to deliver the new framebuffer size.
    const double deadline = glfwGetTime() + kResizeTimeoutSeconds;
    for (double remaining = kResizeTimeoutSeconds; remaining > 0.0;
         remaining = deadline - glfwGetTime()) {
      glfwWaitEventsTimeout(remaining);
      const auto [newWidth, newHeight] = framebufferSize();
      if (newWidth == width && newHeight == height) {
        return;
      }
    }
  }

  const auto [actualWidth, actualHeight] = framebufferSize();
  if (actualWidth != width || actualHeight != height) {
    throw std::runtime_error(
        "Window system could not provide the requested framebuffer size");
  }
}

void Window::requestClose() {
  glfwSetWindowShouldClose(handle_, GLFW_TRUE);
}

void Window::swapBuffers() const {
  glfwSwapBuffers(handle_);
}

std::pair<int, int> Window::framebufferSize() const {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(handle_, &width, &height);
  return {width, height};
}

GLFWwindow* Window::nativeHandle() const noexcept {
  return handle_;
}

}  // namespace craft::graphics
