#include "graphics/GameInput.h"
#include "core/KeyBindings.h"
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
#ifdef _WIN32
namespace {
// GLFW key code to Windows virtual key; letters, digits and space already agree.
int virtualKey(int key) {
  if(key>=GLFW_KEY_F1&&key<=GLFW_KEY_F12) return VK_F1+(key-GLFW_KEY_F1);
  if(key>=GLFW_KEY_KP_0&&key<=GLFW_KEY_KP_9) return VK_NUMPAD0+(key-GLFW_KEY_KP_0);
  switch(key) {
    case GLFW_KEY_LEFT_SHIFT: return VK_LSHIFT; case GLFW_KEY_RIGHT_SHIFT: return VK_RSHIFT;
    case GLFW_KEY_LEFT_CONTROL: return VK_LCONTROL; case GLFW_KEY_RIGHT_CONTROL: return VK_RCONTROL;
    case GLFW_KEY_LEFT_ALT: return VK_LMENU; case GLFW_KEY_RIGHT_ALT: return VK_RMENU;
    case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: return VK_RETURN;
    case GLFW_KEY_ESCAPE: return VK_ESCAPE; case GLFW_KEY_TAB: return VK_TAB;
    case GLFW_KEY_BACKSPACE: return VK_BACK; case GLFW_KEY_CAPS_LOCK: return VK_CAPITAL;
    case GLFW_KEY_UP: return VK_UP; case GLFW_KEY_DOWN: return VK_DOWN; case GLFW_KEY_LEFT: return VK_LEFT; case GLFW_KEY_RIGHT: return VK_RIGHT;
    case GLFW_KEY_INSERT: return VK_INSERT; case GLFW_KEY_DELETE: return VK_DELETE; case GLFW_KEY_HOME: return VK_HOME; case GLFW_KEY_END: return VK_END;
    case GLFW_KEY_PAGE_UP: return VK_PRIOR; case GLFW_KEY_PAGE_DOWN: return VK_NEXT;
    case GLFW_KEY_APOSTROPHE: return VK_OEM_7; case GLFW_KEY_COMMA: return VK_OEM_COMMA; case GLFW_KEY_MINUS: return VK_OEM_MINUS;
    case GLFW_KEY_PERIOD: return VK_OEM_PERIOD; case GLFW_KEY_SLASH: return VK_OEM_2; case GLFW_KEY_SEMICOLON: return VK_OEM_1;
    case GLFW_KEY_EQUAL: return VK_OEM_PLUS; case GLFW_KEY_LEFT_BRACKET: return VK_OEM_4; case GLFW_KEY_BACKSLASH: return VK_OEM_5;
    case GLFW_KEY_RIGHT_BRACKET: return VK_OEM_6; case GLFW_KEY_GRAVE_ACCENT: return VK_OEM_3;
    default: return key;
  }
}
}
#endif
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
    // Only game and menu keys are sampled. Printable input still belongs to ImGui.
    for(int key:KeyBindings::supportedKeys()) {
#ifdef _WIN32
      sample[static_cast<unsigned>(key)]=(GetAsyncKeyState(virtualKey(key))&0x8000)!=0;
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
