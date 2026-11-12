#pragma once
#include "core/ButtonState.h"
struct GLFWwindow;
namespace relay {
class GameInput {
 public:
  explicit GameInput(GLFWwindow* window);
  ~GameInput();
  GameInput(const GameInput&)=delete;
  GameInput& operator=(const GameInput&)=delete;
  void prepare(bool textInputWanted);
  void poll();
  bool held(int key) const { return buttons_.held(key); }
  bool pressed(int key) const { return buttons_.pressed(key); }
  bool released(int key) const { return buttons_.released(key); }
  bool nativeSelfTest();
 private:
  void suppressIme(bool suppress);
  static void keyEvent(GLFWwindow* window,int key,int scancode,int action,int mods);
  void (*previousKeyCallback_)(GLFWwindow*,int,int,int,int)=nullptr;
  ButtonState::Snapshot pressEvents_{};
  GLFWwindow* window_;
  ButtonState buttons_;
  void* handle_=nullptr;
  void* savedContext_=nullptr;
  bool suppressed_=false;
};
}
