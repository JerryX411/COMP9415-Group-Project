#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <string>
#include "imgui.h"
#include "app/AppUi.h"
#include "app/BenchmarkRun.h"
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
  int frames=0,level=0; bool paused=false,play=false,benchmark=false,frozen=false,diagnostics=false,normal=true,compare=false,overview=false,nextLevel=false,surveillance=false; glm::vec3 warp{-1}; float face=0; std::string capture,benchmarkOutput,effect,changeArg; float normalStrength=1;
  for(int i=1;i<argc;++i) {
   std::string arg=argv[i];
   if(arg=="--play") play=true;
   else if(arg=="--surveillance") {surveillance=true;play=true;}
   else if(arg=="--frames"&&i+1<argc) frames=std::stoi(argv[++i]);
   else if(arg=="--level"&&i+1<argc) level=std::clamp(std::stoi(argv[++i]),0,relay::Game::levelCount-1);
   else if(arg=="--overview") overview=true;
   else if(arg=="--pause") {play=true;paused=true;}  // captures: open on the pause menu
   else if(arg=="--next") nextLevel=true;  // captures: start the change to the next level at once
   else if(arg=="--change"&&i+1<argc) changeArg=argv[++i];  // captures: start, restart
   else if(arg=="--warp"&&i+3<argc) {warp={std::stof(argv[i+1]),std::stof(argv[i+2]),std::stof(argv[i+3])};i+=3;}
   else if(arg=="--face"&&i+1<argc) face=std::stof(argv[++i]);
   else if(arg=="--capture"&&i+1<argc) capture=argv[++i];
   else if(arg=="--benchmark")benchmark=true;
   else if(arg=="--benchmark-output"&&i+1<argc)benchmarkOutput=argv[++i];
   else if(arg=="--normal-off")normal=false;
   else if(arg=="--normal-strength"&&i+1<argc){normalStrength=std::stof(argv[++i]);if(!std::isfinite(normalStrength)||normalStrength<0||normalStrength>2)throw std::runtime_error("Normal strength must be between 0 and 2");}
   else if(arg=="--compare-normals")compare=true;
   else if(arg=="--diagnostics")diagnostics=true;
   else if(arg=="--freeze")frozen=true;
   else if(arg=="--effect"&&i+1<argc)effect=argv[++i];
   else throw std::runtime_error("Unknown argument: "+arg);
  }
  if(benchmark)return craft::app::runLiteBenchmark(level,benchmarkOutput);
  craft::graphics::Window window(1440,900,"Core Explorer Lite | Standalone Edition");
  auto* native=window.nativeHandle(); relay::GameInput keys(native);
  craft::app::AppUi ui(window); relay::Game game; game.reset(level);
  if(!effect.empty()) {
   play=true;frozen=true;
   if(effect=="pad") {game.warpPlayer(game.jumpPads()[0].position);for(int i=0;i<24;++i)game.update(1.F/120,{});}
   else if(effect=="core") {game.warpPlayer(game.cores()[0].position+glm::vec3(0,-.8F,1));relay::Input use;use.interactPressed=true;game.update(.01F,use);for(int i=0;i<46;++i)game.update(1.F/120,{});}
   else if(effect=="impact") {for(int i=0;i<492;++i)game.update(1.F/120,{});const auto& trap=game.traps()[0];game.warpPlayer({trap.area.min.x+.6F,trap.area.min.y,trap.area.min.z+.6F});for(int i=0;i<10;++i)game.update(1.F/120,{});}
   else if(effect=="run") {relay::Input dash;dash.movement={0,-1};dash.sprintHeld=true;for(int i=0;i<70;++i)game.update(1.F/120,dash);}
   else if(effect=="land") {game.warpPlayer(game.player().position+glm::vec3(0,3.5F,0));for(int i=0;i<200&&!game.player().grounded;++i)game.update(1.F/120,{});for(int i=0;i<6;++i)game.update(1.F/120,{});}
   else if(effect=="reboot") {
    // Capture: lose all armour on the first floor trap, then freeze mid-reboot.
    const auto& trap=game.traps()[0];
    for(int k=0;k<12&&game.state()==relay::RunState::Playing;++k){for(int i=0;i<140;++i)game.update(1.F/120,{});while(game.trapPhase(0)!=relay::TrapPhase::Active)game.update(1.F/120,{});game.warpPlayer({trap.area.min.x+.6F,trap.area.min.y,trap.area.min.z+.6F});game.update(1.F/120,{});}
    for(int i=0;i<150;++i)game.update(1.F/120,{});
   }
   else throw std::runtime_error("Effect must be pad, core, impact, run, land or reboot");
  }
  // Capture helpers: --warp x y z puts the courier there, --face turns the view (degrees).
  if(warp.x>=0){game.warpPlayer(warp);for(int i=0;i<12;++i)game.update(1.F/120,{});}
  relay::Renderer renderer; relay::FollowCamera camera; camera.reset(game);
  if(face!=0)camera.orbit(face*8.F,0);
  camera.setSurveillance(surveillance,game);
  relay::RenderOptions options; options.overview=overview; options.normalMapping=normal;options.normalStrength=normalStrength;options.normalCompare=compare;options.diagnostics=diagnostics;
  enum class Screen {Title,Play,Pause,Settings}; Screen screen=paused?Screen::Pause:play?Screen::Play:Screen::Title;
  bool settingsPause=false,captured=false; double mx=0,my=0,last=glfwGetTime();
  // Level changes. Enter after a win: the scene fades to black, the next level loads behind a title
  // card, then fades back in. Start from the menu: the same card from black. Restart (R, or the
  // pause menu): a shorter fade with a "RESTART" card. The game holds until the card has gone.
  // Menu (Enter after the last level): a fade to black, then the title screen fades in.
  enum class Change {Next,Start,Restart,Menu};
  struct LevelChange { float t=-1,fadeOut=0,cardEnd=0,end=0; int level=0; bool loaded=false; Change kind=Change::Next; };
  LevelChange change;
  const auto beginChange=[&](Change kind,int next) {
   if(change.t>=0) return;
   change=kind==Change::Next?LevelChange{0,.55F,2.15F,2.8F,next,false,kind}
         :kind==Change::Start?LevelChange{0,0,1.6F,2.25F,next,false,kind}
         :kind==Change::Menu?LevelChange{0,.55F,.7F,1.3F,next,false,kind}
         :LevelChange{0,.3F,1.05F,1.5F,next,false,kind};
  };
  if(nextLevel) beginChange(Change::Next,(level+1)%relay::Game::levelCount);
  if(changeArg=="start") beginChange(Change::Start,level);
  if(changeArg=="restart") beginChange(Change::Restart,level);
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
   if(focused&&screen==Screen::Play) {
    // A toggle needs the queued key-down event, even if a quick tap has already
    // been released before the next native held-key sample. Repeats stay off.
    if(ImGui::IsKeyPressed(ImGuiKey_V,false))camera.setSurveillance(!camera.surveillance(),game);
    if(keys.pressed(GLFW_KEY_F1))options.diagnostics=!options.diagnostics;
    if(keys.pressed(GLFW_KEY_F2)){options.normalMapping=!options.normalMapping;options.normalCompare=false;}
    if(keys.pressed(GLFW_KEY_F3))options.normalCompare=!options.normalCompare;
    if(keys.pressed(GLFW_KEY_F4))frozen=!frozen;
    if(keys.pressed(GLFW_KEY_F5))options.normalStrength=options.normalStrength>=2?0:options.normalStrength+.5F;
    if(keys.pressed(GLFW_KEY_F6))options.optimised=!options.optimised;
   }
   const auto& bindings=relay::keyBindings();
   const bool control=screen==Screen::Play&&focused;
   const bool wantsCapture=control&&game.state()==relay::RunState::Playing&&!camera.surveillance()&&frames==0;
   if(wantsCapture!=captured) {
    captured=wantsCapture; glfwSetInputMode(native,GLFW_CURSOR,captured?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
    if(glfwRawMouseMotionSupported()) glfwSetInputMode(native,GLFW_RAW_MOUSE_MOTION,captured?GLFW_TRUE:GLFW_FALSE);
    glfwGetCursorPos(native,&mx,&my);
   }
   double x,y;glfwGetCursorPos(native,&x,&y);
   if(captured) {camera.orbit(float(x-mx),float(y-my));camera.zoom(ImGui::GetIO().MouseWheel);if(frozen&&(x!=mx||y!=my||ImGui::GetIO().MouseWheel!=0))camera.update(game,dt);} mx=x;my=y;
   const bool finished=game.state()==relay::RunState::Won&&game.level()==relay::Game::levelCount-1;
   if(control&&!finished&&keys.pressed(bindings[relay::KeyAction::Restart])) beginChange(Change::Restart,level);
   relay::Input input;
   if(control) {
    using relay::KeyAction;
    input.movement=camera.movement(float(keys.held(bindings[KeyAction::Right]))-float(keys.held(bindings[KeyAction::Left])),float(keys.held(bindings[KeyAction::Forward]))-float(keys.held(bindings[KeyAction::Back])));
    input.jumpPressed=keys.pressed(bindings[KeyAction::Jump]); input.jumpHeld=keys.held(bindings[KeyAction::Jump]);
    input.crouchPressed=keys.pressed(bindings[KeyAction::Crouch]);input.interactPressed=keys.pressed(bindings[KeyAction::Interact]);
    input.sprintHeld=keys.held(bindings[KeyAction::Sprint]);
   }
   if(change.t>=0) {
    change.t+=dt;
    if(!change.loaded&&change.t>=change.fadeOut){
     change.loaded=true;level=change.level;game.reset(level);camera.reset(game);frozen=false;
     if(change.kind==Change::Menu) screen=Screen::Title;
    }
    if(change.t>=change.end) change.t=-1;
   }
   // Out of lives: once the red banner has faded to black, the level restarts.
   if(game.outOfLives()&&game.player().deathTime>=relay::Game::respawnDelay) beginChange(Change::Restart,level);
   const bool holding=change.t>=0&&change.t<change.cardEnd;
   if(screen==Screen::Play&&!frozen&&!holding&&(focused||frames>0)) {
    if(!camera.surveillance())input.facingYaw=camera.yaw();
    game.update(dt,input);
   }
   if(!frozen)camera.update(game,dt);
   options.overview=overview||camera.surveillance();
   options.menu=screen==Screen::Title||(screen==Screen::Settings&&!settingsPause);
   if(options.menu) {
    renderer.drawMenuBackground(w,h,float(now));
   } else { renderer.prepare(game,options,float(now));renderer.draw(w,h,options,camera); }
   if(screen==Screen::Settings) {
    int pressed=-1;for(int key:relay::KeyBindings::supportedKeys()) if(keys.pressed(key)){pressed=key;break;}
    if(relay::drawSettings(relay::keyBindings(),float(now),pressed,keys.pressed(GLFW_KEY_ESCAPE),settingsPause)) screen=settingsPause?Screen::Pause:Screen::Title;
   } else if(screen==Screen::Title||screen==Screen::Pause) {
    auto action=relay::drawMenu(screen==Screen::Title?relay::MenuMode::Title:relay::MenuMode::Pause,level,float(now));
    if(action==relay::MenuAction::Start){beginChange(Change::Start,level);screen=Screen::Play;}
    if(action==relay::MenuAction::Restart){beginChange(Change::Restart,level);screen=Screen::Play;}
    if(action==relay::MenuAction::Resume)screen=Screen::Play;
    if(action==relay::MenuAction::Title)screen=Screen::Title;
    if(action==relay::MenuAction::Quit)window.requestClose();
    if(action==relay::MenuAction::Settings){settingsPause=screen==Screen::Pause;screen=Screen::Settings;}
   } else { const bool unused=ui.draw({},game,options,renderer,focused); (void)unused; }
   if(int next=ui.takeLevelRequest();next>=0) beginChange(Change::Next,next);
   if(ui.takeMenuRequest()) beginChange(Change::Menu,0);
   if(change.t>=0) {
    const auto ease=[](float x){x=std::clamp(x,0.F,1.F);return x*x*(3-2*x);};
    const float t=change.t,fade=change.kind==Change::Restart?.2F:.3F;
    const float black=t<change.fadeOut?ease(t/change.fadeOut):t<change.cardEnd?1.F:1-ease((t-change.cardEnd)/(change.end-change.cardEnd));
    const float card=change.kind==Change::Menu?0.F:t<change.fadeOut?0.F:ease((t-change.fadeOut-.05F)/fade)*(1-ease((t-(change.cardEnd-fade))/fade));
    // Numbered as on the title menu: the tutorial, then the missions as Level 1 and Level 2.
    const std::string place=change.level==0?std::string("TUTORIAL"):"LEVEL "+std::to_string(change.level);
    const std::string kicker=(change.kind==Change::Restart?"RESTART  /  ":"")+place;
    relay::drawLevelTransition(black,card,kicker.c_str(),relay::Game::nameOf(change.level));
   }
   if(screen==Screen::Play)ui.drawDiagnostics(game,options,renderer,camera,frozen);
   ui.render();
   if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL error");
   if(frames>0&&++count>=frames) {if(!capture.empty())relay::saveScreenshot(capture,w,h);std::cout<<"lite_smoke level="<<level<<" health="<<game.player().health<<" particles="<<game.liveParticleCount()<<" normal="<<options.normalMapping<<" intro=none gl_error=0\n";window.requestClose();}
   window.swapBuffers();
  }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 return 0;
}
