#include <algorithm>
#include <array>
#include <cstdio>
#include "core/ArrivalSequence.h"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include "imgui.h"
#include "app/AppUi.h"
#include "app/RelayMenu.h"
#include "app/SceneBenchmark.h"
#include "core/FollowCamera.h"
#include "graphics/RelayRenderer.h"
#include "graphics/Screenshot.h"
#include "graphics/OpenGL.h"
#include "graphics/Window.h"
#include "graphics/GameInput.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace {
void startBenchmark(craft::graphics::Window& window,craft::app::SceneBenchmark& benchmark) {
  window.setFramebufferSize(craft::app::kBenchmarkFramebufferWidth,craft::app::kBenchmarkFramebufferHeight);
  window.setVSync(false); benchmark.start();
}
enum class Screen { Title,Playing,Paused };
}
int main(int argc,char** argv) {
  try {
    relay::RenderOptions options;
    std::filesystem::path capture,record; std::string scene;
    int frameLimit=0,level=0; bool title=true,forceMenu=false,inputSelfTest=false,startArrival=false;
    for(int i=1;i<argc;++i) {
      const std::string arg=argv[i];
      if(arg=="--baseline") options.optimised=false;
      else if(arg=="--no-normal-map") options.normalMapping=false;
      else if(arg=="--gallery") options.gallery=true;
      else if(arg=="--overview") options.overview=true;
      else if(arg=="--menu"||arg=="--title") forceMenu=true;
      else if(arg=="--play") title=false;
      else if(arg=="--intro") { title=false; startArrival=true; }
      else if(arg=="--level2") level=1;
      else if(arg=="--tools") options.diagnostics=true;
      else if(arg=="--colliders") options.collisionOverlay=true;
      else if(arg=="--input-self-test") inputSelfTest=true;
      else if(arg=="--frames"&&i+1<argc) frameLimit=std::stoi(argv[++i]);
      else if(arg=="--capture"&&i+1<argc) capture=argv[++i];
      else if(arg=="--record"&&i+1<argc) record=argv[++i];
      else if(arg=="--motion"&&i+1<argc) { options.gallery=true; options.motionPreview=std::clamp(std::stoi(argv[++i]),0,10); }
      else if(arg=="--scene"&&i+1<argc) scene=argv[++i];
      else throw std::runtime_error("Unknown argument: "+arg);
    }
    if(!capture.empty()&&frameLimit<=0) frameLimit=60;
    if(!record.empty()&&frameLimit<=0) frameLimit=180;
    craft::graphics::Window window(1440,900,"Voxel Relay | Third-Person Prototype");
    if(!record.empty()) window.setFramebufferSize(960,600);
    auto* native=window.nativeHandle();
    relay::GameInput keys(native);
    if(inputSelfTest) {
      const bool passed=keys.nativeSelfTest();
      std::cout<<"input_self_test ime_detach_restore="<<passed<<" layout_unchanged="<<passed<<'\n';
      return passed?0:1;
    }
    relay::Game game; game.reset(level); relay::Renderer renderer; relay::FollowCamera camera;
    craft::app::AppUi ui(window); craft::app::SceneBenchmark benchmark;
    const bool cliBenchmark=craft::app::benchmarkRequestedFromEnvironment();
    if(cliBenchmark) { options.gallery=false; options.overview=true; startBenchmark(window,benchmark); }
    if(frameLimit>0) window.setVSync(false);
    Screen screen=(forceMenu||(title&&frameLimit==0&&!options.gallery&&!cliBenchmark))?Screen::Title:Screen::Playing;
    if(scene=="drop") game.warpPlayer({7,8,24});
    if(scene=="switch") game.warpPlayer(game.switches()[0].position+glm::vec3(0,0,1));
    if(scene=="jump") game.warpPlayer({11.6F,1,21.1F});
    if(scene=="pickup") game.warpPlayer({9,1,23.4F});
    if(scene=="trap-edge") { game.warpPlayer({14.70F,1,12.5F}); for(int i=0;i<410;++i) game.update(.01F,{}); }
    if(scene=="knockback") { game.reset(1); game.warpPlayer({14,1,10.5F}); }
    if(scene=="death") {
      for(int hit=0;hit<5;++hit) {
        game.warpPlayer({7,1,24});
        for(int i=0;i<150;++i) game.update(.01F,{});
        while(game.trapPhase(0)!=relay::TrapPhase::Active) game.update(.01F,{});
        game.warpPlayer({17,1,13}); game.update(.01F,{});
      }
    }
    if(scene=="hurt") { for(int i=0;i<5;++i) game.update(.82F,{}); game.warpPlayer({17,1,13}); }
    if(scene=="exit") {
      const auto cores=game.cores();
      for(const auto& c:cores) { game.warpPlayer({c.position.x,1,c.position.z}); game.update(.5F,{{0,0},false,true}); }
      game.warpPlayer({25,1,9});
    }
    camera.reset(game); int cameraRevision=game.revision();
    relay::ArrivalSequence arrival; if(startArrival) arrival.start();
    double last=glfwGetTime(),mouseX=0,mouseY=0; int frame=0;
    bool captured=false,cursorFree=false,savedOverview=options.overview;
    std::cout<<"renderer="<<glGetString(GL_RENDERER)<<" OpenGL="<<glGetString(GL_VERSION)<<'\n';
    while(!window.shouldClose()) {
      keys.prepare(ImGui::GetIO().WantTextInput);
      window.pollEvents(); keys.poll(); auto [width,height]=window.framebufferSize();
      const double now=glfwGetTime(); const float dt=frameLimit>0?1.F/60.F:std::clamp(float(now-last),0.F,.10F); last=now;
      if(width==0||height==0) { glfwWaitEventsTimeout(.05); continue; }
      ui.beginFrame();
      const bool focused=glfwGetWindowAttrib(native,GLFW_FOCUSED)==GLFW_TRUE;
      const bool running=benchmark.snapshot().running;
      if(focused&&!running&&keys.pressed(GLFW_KEY_ESCAPE)) {
        if(options.gallery) { options.gallery=false; screen=Screen::Title; }
        else if(screen==Screen::Playing) screen=Screen::Paused;
        else if(screen==Screen::Paused) { screen=Screen::Playing; cursorFree=false; }
      }
      const bool arriving=arrival.active();
      arrival.update(dt,keys.pressed(GLFW_KEY_ENTER)||keys.pressed(GLFW_KEY_SPACE),!running&&screen==Screen::Playing&&(focused||frameLimit>0));
      options.wakeTime=arrival.active()?arrival.elapsed:-1;
      const bool acceptsKeys=focused&&!running&&!arriving&&screen==Screen::Playing&&!ImGui::GetIO().WantTextInput;
      if(acceptsKeys) {
        if(keys.pressed(GLFW_KEY_F1)) options.diagnostics=!options.diagnostics;
        if(keys.pressed(GLFW_KEY_F2)) options.overview=!options.overview;
        if(keys.pressed(GLFW_KEY_F3)) options.gallery=!options.gallery;
        if(keys.pressed(GLFW_KEY_TAB)) cursorFree=!cursorFree;
        if(keys.pressed(GLFW_KEY_R)) game.reset(game.level());
        if(keys.pressed(GLFW_KEY_1)) { game.reset(0); options.gallery=false; }
        if(keys.pressed(GLFW_KEY_2)) { game.reset(1); options.gallery=false; }
      }
      const bool wantsCapture=focused&&!arriving&&screen==Screen::Playing&&!options.gallery&&!options.diagnostics&&!options.overview&&!cursorFree&&!running&&game.state()==relay::RunState::Playing&&frameLimit==0;
      if(wantsCapture!=captured) {
        captured=wantsCapture; glfwSetInputMode(native,GLFW_CURSOR,captured?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
        if(glfwRawMouseMotionSupported()) glfwSetInputMode(native,GLFW_RAW_MOUSE_MOTION,captured?GLFW_TRUE:GLFW_FALSE);
        glfwGetCursorPos(native,&mouseX,&mouseY);
      }
      double x=0,y=0; glfwGetCursorPos(native,&x,&y);
      if(captured) { camera.orbit(float(x-mouseX),float(y-mouseY)); camera.zoom(ImGui::GetIO().MouseWheel); }
      mouseX=x; mouseY=y;
      relay::Input input;
      if(acceptsKeys&&!cursorFree&&!options.diagnostics&&!options.gallery) {
        const float right=float(keys.held(GLFW_KEY_D))-float(keys.held(GLFW_KEY_A));
        const float forward=float(keys.held(GLFW_KEY_W))-float(keys.held(GLFW_KEY_S));
        input.movement=options.overview?glm::vec2(.759257F,-.650791F)*right+glm::vec2(-.650791F,-.759257F)*forward:camera.movement(right,forward);
        input.jumpPressed=keys.pressed(GLFW_KEY_SPACE); input.interactPressed=keys.pressed(GLFW_KEY_E);
        input.jumpHeld=keys.held(GLFW_KEY_SPACE);
        input.sprintHeld=keys.held(GLFW_KEY_LEFT_SHIFT)||keys.held(GLFW_KEY_RIGHT_SHIFT);
      }
      if(frameLimit>0) {
        if(scene=="switch") input.interactPressed=frame==2;
        if(scene=="pickup") input.interactPressed=frame==2;
        if(scene=="sprint") { input.movement={1,0}; input.sprintHeld=true; }
        if(scene=="jump") { input.movement={0,-1}; input.jumpPressed=frame==2; input.jumpHeld=frame<35; }
        if(scene=="exit"&&frame>50) input.movement={0,-1};
        if(scene=="orbit") camera.orbit(2,-.1F);
        if(scene=="follow") input.movement=camera.movement(0,1);
      }
      if(!running&&!arriving&&screen==Screen::Playing&&!options.gallery&&!options.diagnostics&&!cursorFree&&(focused||frameLimit>0)) game.update(dt,input);
      if(cameraRevision!=game.revision()) { camera.reset(game); cameraRevision=game.revision(); }
      if(!running) camera.update(game,dt);
      options.menu=screen!=Screen::Playing;
      renderer.prepare(game,options,running?0:frameLimit>0?float(frame)/60:float(now));
      benchmark.beginSceneFrame(); renderer.draw(width,height,options,camera);
      const bool completed=benchmark.endSceneFrame();
      if(screen!=Screen::Playing) {
        auto action=relay::drawMenu(screen==Screen::Paused,level,frameLimit>0?float(frame)/60:float(now));
        if(focused&&keys.pressed(GLFW_KEY_ENTER)) action=screen==Screen::Paused?relay::MenuAction::Resume:relay::MenuAction::Start;
        if(action==relay::MenuAction::Start) { game.reset(level); camera.reset(game); screen=Screen::Playing; options.gallery=false; cursorFree=false; options.diagnostics=false; arrival.start(); }
        if(action==relay::MenuAction::Resume) { screen=Screen::Playing; cursorFree=false; }
        if(action==relay::MenuAction::Gallery) { screen=Screen::Playing; options.gallery=true; arrival.cancel(); }
        if(action==relay::MenuAction::Title) { screen=Screen::Title; arrival.cancel(); }
        if(action==relay::MenuAction::Quit) window.requestClose();
      } else if(arrival.active()) { relay::drawArrival(arrival.elapsed);
      } else if(record.empty()||!options.gallery) {
        if(ui.draw(benchmark.snapshot(),game,options,renderer,focused)&&!running) {
          savedOverview=options.overview; options.overview=true; startBenchmark(window,benchmark);
        }
        if(cursorFree&&!options.gallery) {
          ImGui::SetNextWindowPos({float(width)*.5F,20},ImGuiCond_Always,{.5F,0});
          ImGui::Begin("Cursor status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize);
          ImGui::TextUnformatted("CURSOR FREE / PAUSED  -  TAB TO RESUME"); ImGui::End();
        }
      }
      ui.render();
      const auto error=glGetError(); if(error!=GL_NO_ERROR) throw std::runtime_error("OpenGL error "+std::to_string(error));
      ++frame;
      if(!record.empty()&&frame%2==0) {
        char name[32]; std::snprintf(name,sizeof(name),"frame-%04d.png",frame/2);
        relay::saveScreenshot(record/name,width,height);
      }
      if(frameLimit>0&&frame>=frameLimit) {
        if(!capture.empty()) relay::saveScreenshot(capture,width,height);
        std::cout<<"smoke_result frames="<<frame<<" level="<<game.level()+1<<" cores="<<game.collectedCount()<<" health="<<game.player().health<<" state="<<int(game.state())<<" elapsed="<<game.elapsed()<<" timer="<<game.timeRemaining()<<" arrival="<<arrival.active()<<" camera="<<(options.overview?"overview":"follow")<<" particles="<<game.liveParticleCount()<<" gl_error=0\n";
        window.requestClose();
      }
      window.swapBuffers();
      if(completed) {
        const auto b=benchmark.snapshot(); const auto s=renderer.stats();
        std::cout<<std::fixed<<std::setprecision(4)<<"benchmark_result sample_count="<<b.measuredFrames<<" average_ms="<<b.averageMilliseconds<<" average_fps="<<b.averageFramesPerSecond<<" resolution="<<width<<'x'<<height<<" workload=voxel-relay-level-"<<game.level()+1<<" face_removal="<<options.optimised<<" normal_mapping="<<options.normalMapping<<" triangles="<<s.submittedTriangles<<'\n';
        options.overview=savedOverview; window.setVSync(true); if(cliBenchmark) window.requestClose();
      }
    }
  } catch(const std::exception& e) { std::cerr<<"Voxel Relay failed: "<<e.what()<<'\n'; return 1; }
  return 0;
}
