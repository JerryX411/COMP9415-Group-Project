#include "graphics/GameInput.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
namespace relay {
GameInput::GameInput(GLFWwindow* window):window_(window) {
#ifdef _WIN32
  handle_=glfwGetWin32Window(window);
#endif
}
GameInput::~GameInput() { suppressIme(false); }
void GameInput::suppressIme(bool suppress) {
#ifdef _WIN32
  if(suppress==suppressed_) return;
  auto window=static_cast<HWND>(handle_);
  if(suppress) {
    if(auto context=ImmGetContext(window)) {
      ImmNotifyIME(context,NI_COMPOSITIONSTR,CPS_CANCEL,0);
      ImmReleaseContext(window,context);
    }
    savedContext_=ImmAssociateContext(window,nullptr);
  } else ImmAssociateContext(window,static_cast<HIMC>(savedContext_));
  suppressed_=suppress;
#else
  (void)suppress;
#endif
}
void GameInput::prepare(bool textInputWanted) {
  // No layout changes, global keyboard hooks, or background keyboard polling.
  suppressIme(glfwGetWindowAttrib(window_,GLFW_FOCUSED)&&!textInputWanted);
}
void GameInput::poll() {
  bool focused=glfwGetWindowAttrib(window_,GLFW_FOCUSED)==GLFW_TRUE;
#ifdef _WIN32
  focused=focused && GetForegroundWindow()==static_cast<HWND>(handle_);
#endif
  ButtonState::Snapshot sample{};
  if(focused) {
    // Only game controls are sampled. Printable input still belongs to ImGui.
    constexpr int keys[]{GLFW_KEY_W,GLFW_KEY_A,GLFW_KEY_S,GLFW_KEY_D,GLFW_KEY_E,GLFW_KEY_R,
      GLFW_KEY_SPACE,GLFW_KEY_LEFT_SHIFT,GLFW_KEY_RIGHT_SHIFT,GLFW_KEY_ENTER,GLFW_KEY_ESCAPE,
      GLFW_KEY_TAB,GLFW_KEY_1,GLFW_KEY_2,GLFW_KEY_F1,GLFW_KEY_F2,GLFW_KEY_F3};
    for(int key:keys) {
#ifdef _WIN32
      int vk=key;
      switch(key) {
        case GLFW_KEY_SPACE: vk=VK_SPACE; break;
        case GLFW_KEY_LEFT_SHIFT: vk=VK_LSHIFT; break;
        case GLFW_KEY_RIGHT_SHIFT: vk=VK_RSHIFT; break;
        case GLFW_KEY_ENTER: vk=VK_RETURN; break;
        case GLFW_KEY_ESCAPE: vk=VK_ESCAPE; break;
        case GLFW_KEY_TAB: vk=VK_TAB; break;
        case GLFW_KEY_F1: vk=VK_F1; break;
        case GLFW_KEY_F2: vk=VK_F2; break;
        case GLFW_KEY_F3: vk=VK_F3; break;
      }
      sample[static_cast<unsigned>(key)]=(GetAsyncKeyState(vk)&0x8000)!=0;
#else
      sample[static_cast<unsigned>(key)]=glfwGetKey(window_,key)==GLFW_PRESS;
#endif
    }
  }
  buttons_.update(sample,focused);
  if(!focused) suppressIme(false);
}
bool GameInput::nativeSelfTest() {
#ifdef _WIN32
  suppressIme(false);
  auto window=static_cast<HWND>(handle_);
  const HKL layout=GetKeyboardLayout(0);
  HIMC test=ImmCreateContext(); if(!test) return false;
  HIMC previous=ImmAssociateContext(window,test);
  suppressIme(true);
  HIMC detached=ImmGetContext(window);
  if(detached) ImmReleaseContext(window,detached);
  suppressIme(false);
  HIMC restored=ImmGetContext(window);
  const bool passed=!detached && restored==test && GetKeyboardLayout(0)==layout;
  if(restored) ImmReleaseContext(window,restored);
  ImmAssociateContext(window,previous); ImmDestroyContext(test);
  return passed;
#else
  return true;
#endif
}
}
