#include <algorithm>
#include <iostream>
#include <filesystem>
#include <string>
#include "imgui.h"
#include "app/AppUi.h"
#include "app/RelayMenu.h"
#include "core/KeyBindings.h"
#include "core/FollowCamera.h"
#include "graphics/RelayRenderer.h"
#include "graphics/Window.h"
#include "graphics/GameInput.h"
#include "graphics/OpenGL.h"
#include "graphics/Screenshot.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
int main(int argc,char** argv) {
 try {
  int frames=0,level=0; bool play=false; std::string capture;
  for(int i=1;i<argc;++i) {
   std::string arg=argv[i];
   if(arg=="--play") play=true;
   else if(arg=="--frames"&&i+1<argc) frames=std::stoi(argv[++i]);
   else if(arg=="--level"&&i+1<argc) level=std::clamp(std::stoi(argv[++i]),0,1);
   else if(arg=="--capture"&&i+1<argc) capture=argv[++i];
   else throw std::runtime_error("Unknown argument: "+arg);
  }
  craft::graphics::Window window(1440,900,"Core Explorer Lite | Standalone Edition");
  auto* native=window.nativeHandle(); relay::GameInput keys(native);
  craft::app::AppUi ui(window); relay::Game game; game.reset(level);
  relay::Renderer renderer; relay::FollowCamera camera; camera.reset(game);
  relay::RenderOptions options; options.normalMapping=false;
  enum class Screen {Title,Play,Pause,Settings}; Screen screen=play?Screen::Play:Screen::Title;
  bool settingsPause=false,captured=false; double mx=0,my=0,last=glfwGetTime();
  relay::keyBindings().load(relay::settingsPath());
  if(frames>0) window.setVSync(false);
  int count=0;
  while(!window.shouldClose()) {
   keys.prepare(ImGui::GetIO().WantTextInput); window.pollEvents(); keys.poll();
   auto [w,h]=window.framebufferSize(); if(w<=0||h<=0) {glfwWaitEventsTimeout(.05);continue;}
   const double now=glfwGetTime(); const float dt=frames>0?1.F/60:std::clamp(float(now-last),0.F,.1F);last=now;
   ui.beginFrame(); const bool focused=glfwGetWindowAttrib(native,GLFW_FOCUSED)==GLFW_TRUE;
   if(focused&&keys.pressed(GLFW_KEY_ESCAPE)&&screen!=Screen::Settings) {
    if(screen==Screen::Play) screen=Screen::Pause; else if(screen==Screen::Pause) screen=Screen::Play;
   }
   const auto& bindings=relay::keyBindings();
   const bool control=screen==Screen::Play&&focused;
   const bool wantsCapture=control&&game.state()==relay::RunState::Playing&&frames==0;
   if(wantsCapture!=captured) {
    captured=wantsCapture; glfwSetInputMode(native,GLFW_CURSOR,captured?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
    if(glfwRawMouseMotionSupported()) glfwSetInputMode(native,GLFW_RAW_MOUSE_MOTION,captured?GLFW_TRUE:GLFW_FALSE);
    glfwGetCursorPos(native,&mx,&my);
   }
   double x,y;glfwGetCursorPos(native,&x,&y);
   if(captured) {camera.orbit(float(x-mx),float(y-my));camera.zoom(ImGui::GetIO().MouseWheel);} mx=x;my=y;
   if(control&&keys.pressed(bindings[relay::KeyAction::Restart])) {game.reset(level);camera.reset(game);}
   relay::Input input;
   if(control) {
    using relay::KeyAction;
    input.movement=camera.movement(float(keys.held(bindings[KeyAction::Right]))-float(keys.held(bindings[KeyAction::Left])),float(keys.held(bindings[KeyAction::Forward]))-float(keys.held(bindings[KeyAction::Back])));
    input.jumpPressed=keys.pressed(bindings[KeyAction::Jump]); input.jumpHeld=keys.held(bindings[KeyAction::Jump]);
    input.crouchPressed=keys.pressed(bindings[KeyAction::Crouch]);input.interactPressed=keys.pressed(bindings[KeyAction::Interact]);
   }
   if(screen==Screen::Play&&(focused||frames>0)) game.update(dt,input);
   camera.update(game,dt); options.menu=screen==Screen::Title||(screen==Screen::Settings&&!settingsPause);
   if(options.menu) {
    renderer.drawMenuBackground(w,h,float(now));
   } else { renderer.prepare(game,options,float(now));renderer.draw(w,h,options,camera); }
   if(screen==Screen::Settings) {
    int pressed=-1;for(int key:relay::KeyBindings::supportedKeys()) if(keys.pressed(key)){pressed=key;break;}
    if(relay::drawSettings(relay::keyBindings(),float(now),pressed,keys.pressed(GLFW_KEY_ESCAPE),settingsPause)) screen=settingsPause?Screen::Pause:Screen::Title;
   } else if(screen==Screen::Title||screen==Screen::Pause) {
    auto action=relay::drawMenu(screen==Screen::Title?relay::MenuMode::Title:relay::MenuMode::Pause,level,float(now));
    if(action==relay::MenuAction::Start||action==relay::MenuAction::Restart){game.reset(level);camera.reset(game);screen=Screen::Play;}
    if(action==relay::MenuAction::Resume)screen=Screen::Play;
    if(action==relay::MenuAction::Title)screen=Screen::Title;
    if(action==relay::MenuAction::Quit)window.requestClose();
    if(action==relay::MenuAction::Settings){settingsPause=screen==Screen::Pause;screen=Screen::Settings;}
   } else { const bool unused=ui.draw({},game,options,renderer,focused); (void)unused; }
   if(int next=ui.takeLevelRequest();next>=0){level=next;game.reset(level);camera.reset(game);}
   ui.render();
   if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL error");
   if(frames>0&&++count>=frames) {if(!capture.empty())relay::saveScreenshot(capture,w,h);std::cout<<"lite_smoke level="<<level<<" health="<<game.player().health<<" intro=none gl_error=0\n";window.requestClose();}
   window.swapBuffers();
  }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 return 0;
}
