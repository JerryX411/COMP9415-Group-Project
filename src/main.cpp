#include <algorithm>
#include <array>
#include <cstdio>
#include "core/ArrivalSequence.h"
#include "core/LevelTransition.h"
#include "core/KeyBindings.h"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
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
enum class Screen { Title,Playing,Paused,Settings };
}
int main(int argc,char** argv) {
  try {
    relay::RenderOptions options;
    std::filesystem::path capture,record; std::string scene;
    int frameLimit=0,level=0,manualPage=-1; bool title=true,forceMenu=false,forcePause=false,forceSettings=false,switchLevel=false,inputSelfTest=false,startArrival=false;
    // Edited maps (assets/levels/levelN.txt from RelayEditor) load by default; --builtin skips them.
    relay::Game::useLevelFiles=true;
    for(int i=1;i<argc;++i) {
      const std::string arg=argv[i];
      if(arg=="--baseline") options.optimised=false;
      else if(arg=="--no-normal-map") options.normalMapping=false;
      else if(arg=="--gallery") options.gallery=true;
      else if(arg=="--overview") options.overview=true;
      else if(arg=="--menu"||arg=="--title") forceMenu=true;
      else if(arg=="--pause") forcePause=true;
      else if(arg=="--settings") forceSettings=true;
      else if(arg=="--switch") switchLevel=true;
      else if(arg=="--play") title=false;
      else if(arg=="--builtin") relay::Game::useLevelFiles=false;
      else if(arg=="--intro") { title=false; startArrival=true; }
      else if(arg=="--level2") level=1;
      else if(arg=="--training") level=relay::Game::tutorialLevel;
      else if(arg=="--manual"&&i+1<argc) manualPage=std::stoi(argv[++i]);
      else if(arg=="--tools") options.diagnostics=true;
      else if(arg=="--colliders") options.collisionOverlay=true;
      else if(arg=="--input-self-test") inputSelfTest=true;
      else if(arg=="--frames"&&i+1<argc) frameLimit=std::stoi(argv[++i]);
      else if(arg=="--capture"&&i+1<argc) capture=argv[++i];
      else if(arg=="--record"&&i+1<argc) record=argv[++i];
      else if(arg=="--motion"&&i+1<argc) { options.gallery=true; options.motionPreview=std::clamp(std::stoi(argv[++i]),0,16); }
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
    // Saved key bindings, if any, replace the defaults for this session.
    relay::keyBindings().load(relay::settingsPath());
    const auto& bindings=relay::keyBindings();
    relay::Game game; game.reset(level); relay::Renderer renderer; relay::FollowCamera camera;
    craft::app::AppUi ui(window); craft::app::SceneBenchmark benchmark;
    if(manualPage>=0) ui.openPopup(manualPage);
    const bool cliBenchmark=craft::app::benchmarkRequestedFromEnvironment();
    if(cliBenchmark) { options.gallery=false; options.overview=true; startBenchmark(window,benchmark); }
    if(frameLimit>0) window.setVSync(false);
    Screen screen=(forceMenu||(title&&frameLimit==0&&!options.gallery&&!cliBenchmark))?Screen::Title:Screen::Playing;
    if(forcePause) screen=Screen::Paused;
    if(scene=="drop") game.warpPlayer({7,8,24});
    if(scene=="checkpoint") game.warpPlayer(game.checkpoints()[0].respawn);
    if(scene=="reboot") {
      // Activate the first checkpoint, then lose all armour on the trap.
      game.warpPlayer(game.checkpoints()[0].respawn); game.update(.05F,{}); game.update(.01F,{{0,0},false,true}); game.update(1.F,{});
      for(int hit=0;hit<5;++hit) {
        game.warpPlayer({7,1,24});
        for(int i=0;i<120;++i) game.update(.01F,{});
        while(game.trapPhase(0)!=relay::TrapPhase::Active) game.update(.01F,{});
        game.warpPlayer({17,1,13}); game.update(.01F,{});
      }
    }
    // Capture-only: an open lane along the south wall for a side view of a slide.
    if(scene=="slide"||scene=="slidehop") game.warpPlayer({5.5F,1,7});
    if(scene=="walljump") { game.warpPlayer({14,1,22}); game.setBoostForDebug(2.4F); }
    if(scene=="boost"||scene=="flight"||scene=="dive") game.warpPlayer({6,1,26.9F});
    if(scene=="flipup") game.warpPlayer({4.8F,1,26.9F});
    if(scene=="slidepad"||scene=="slidejump") game.warpPlayer({4.8F,1,26.9F});
    // Capture-only: sprint and slide through the training tunnel.
    if(scene=="tunnel") { game.reset(relay::Game::tutorialLevel); game.warpPlayer({37.5F,1,39}); }
    if(scene=="shield") game.warpPlayer({12.5F,1,11.8F});
    if(scene=="spring") game.warpPlayer(game.pickups()[1].position);
    if(scene=="edge") game.warpPlayer({27.6F,1,16});
    if(scene=="clock") game.warpPlayer(game.pickups()[2].position+glm::vec3(-3.F,0,0));
    // Capture-only: sprint onto the open training-hall pad, camera behind.
    if(scene=="launch") { game.reset(relay::Game::tutorialLevel); game.warpPlayer({16.6F,1,9.75F}); }
    // Capture-only: press the corridor plate, then stand before its door.
    if(scene=="door") { game.reset(1); game.warpPlayer(game.plates()[0].position); game.update(.1F,{}); game.warpPlayer({8.6F,1,24}); }
    // Capture-only: a wall with a one-voxel-high, three-wide hole across the spawn lane.
    if(scene=="crawl") for(int x=12;x<14;++x) for(int z=20;z<29;++z) for(int y=1;y<4;++y) if(!(y==1&&z>=23&&z<=25)) game.setVoxelForDebug(x,y,z,2);
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
    float freeze=0; relay::Input heldEdges;
    const GLFWvidmode* mode=glfwGetVideoMode(glfwGetPrimaryMonitor());
    const float refreshInterval=1.F/float(mode&&mode->refreshRate>0?mode->refreshRate:60);
    // Capture runs report frame cost by stage; glFinish makes the draw stages include GPU time.
    std::vector<float> frameMs; std::array<double,4> stageMs{}; double stageStart=0;
    // CRAFT_PROFILE=1 prints real-time frame pacing every two seconds (hitches are frames over 20 ms).
    const bool profile=std::getenv("CRAFT_PROFILE")!=nullptr; double profileStart=glfwGetTime(),worstInterval=0; int profileFrames=0,hitches=0;
    const auto stage=[&](int index) { if(frameLimit<=0) return; glFinish(); const double t=glfwGetTime(); stageMs[std::size_t(index)]+=(t-stageStart)*1000; stageStart=t; };
    relay::LevelTransition transition; if(switchLevel) transition.start(1-game.level());
    bool captured=false,cursorFree=false,settingsFromPause=forcePause,savedOverview=options.overview;
    if(forceSettings) screen=Screen::Settings;
    std::cout<<"renderer="<<glGetString(GL_RENDERER)<<" OpenGL="<<glGetString(GL_VERSION)<<'\n';
    while(!window.shouldClose()) {
      keys.prepare(ImGui::GetIO().WantTextInput);
      window.pollEvents(); keys.poll(); auto [width,height]=window.framebufferSize();
      const double now=glfwGetTime(); float dt=frameLimit>0?1.F/60.F:std::clamp(float(now-last),0.F,.10F); last=now;
      // With vsync the display advances in whole refresh intervals, but measured frame gaps
      // jitter by a few milliseconds; snapping near-interval gaps keeps motion even.
      if(frameLimit<=0&&std::abs(dt-refreshInterval)<.004F) dt=refreshInterval;
      const double frameStart=now; stageStart=now;
      if(profile&&frameLimit<=0) {
        const double interval=double(dt)*1000; ++profileFrames; worstInterval=std::max(worstInterval,interval); if(interval>20) ++hitches;
        if(now-profileStart>=2) { std::cout<<std::fixed<<std::setprecision(1)<<"profile fps="<<profileFrames/(now-profileStart)<<" worst_ms="<<worstInterval<<" hitches="<<hitches<<std::endl; profileStart=now; profileFrames=hitches=0; worstInterval=0; }
      }
      if(width==0||height==0) { glfwWaitEventsTimeout(.05); continue; }
      ui.beginFrame();
      const bool focused=glfwGetWindowAttrib(native,GLFW_FOCUSED)==GLFW_TRUE;
      const bool running=benchmark.snapshot().running;
      // Esc opens and closes the pause menu; the settings page handles Esc itself.
      if(focused&&!running&&screen!=Screen::Settings&&!ui.popupOpen()&&keys.pressed(GLFW_KEY_ESCAPE)) {
        if(options.gallery) { options.gallery=false; screen=Screen::Title; }
        else if(screen==Screen::Playing&&!transition.active()) screen=Screen::Paused;
        else if(screen==Screen::Paused) { screen=Screen::Playing; cursorFree=false; }
      }
      const bool arriving=arrival.active();
      arrival.update(dt,keys.pressed(GLFW_KEY_ENTER)||keys.pressed(bindings[relay::KeyAction::Jump]),!running&&screen==Screen::Playing&&(focused||frameLimit>0));
      options.wakeTime=arrival.active()?arrival.elapsed:-1;
      // Level loads go through a blackout; it holds gameplay until the fade-in starts.
      if(transition.update(dt)) { game.reset(transition.target); camera.reset(game); cameraRevision=game.revision(); freeze=0; heldEdges=relay::Input{}; }
      const bool acceptsKeys=focused&&!running&&!arriving&&!transition.blocksGameplay()&&screen==Screen::Playing&&!ImGui::GetIO().WantTextInput;
      if(acceptsKeys) {
        if(keys.pressed(GLFW_KEY_F1)) options.diagnostics=!options.diagnostics;
        if(keys.pressed(GLFW_KEY_F2)) options.overview=!options.overview;
        if(keys.pressed(GLFW_KEY_F3)) options.gallery=!options.gallery;
        if(keys.pressed(bindings[relay::KeyAction::Restart])) transition.start(game.level());
        if(relay::Game::missionsUnlocked&&keys.pressed(GLFW_KEY_1)) { transition.start(0); options.gallery=false; }
        if(relay::Game::missionsUnlocked&&keys.pressed(GLFW_KEY_2)) { transition.start(1); options.gallery=false; }
      }
      const bool wantsCapture=focused&&!arriving&&screen==Screen::Playing&&!options.gallery&&!options.diagnostics&&!options.overview&&!cursorFree&&!running&&!ui.popupOpen()&&game.state()==relay::RunState::Playing&&frameLimit==0;
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
        using relay::KeyAction;
        const float right=float(keys.held(bindings[KeyAction::Right]))-float(keys.held(bindings[KeyAction::Left]));
        const float forward=float(keys.held(bindings[KeyAction::Forward]))-float(keys.held(bindings[KeyAction::Back]));
        input.movement=options.overview?glm::vec2(.759257F,-.650791F)*right+glm::vec2(-.650791F,-.759257F)*forward:camera.movement(right,forward);
        input.jumpPressed=keys.pressed(bindings[KeyAction::Jump]); input.interactPressed=keys.pressed(bindings[KeyAction::Interact]);
        input.jumpHeld=keys.held(bindings[KeyAction::Jump]);
        input.sprintHeld=keys.held(bindings[KeyAction::Sprint]);
        input.crouchPressed=keys.pressed(bindings[KeyAction::Crouch]);
        input.sprintPressed=keys.pressed(bindings[KeyAction::Sprint]);
      }
      if(frameLimit>0) {
        if(scene=="switch") input.interactPressed=frame==2;
        if(scene=="pickup"||scene=="checkpoint") input.interactPressed=frame==2;
        if(scene=="sprint") { input.movement={1,0}; input.sprintHeld=true; }
        // Standing with Shift held: the backpack glows softly (compare with "calm").
        if(scene=="shiftidle") input.sprintHeld=true;
        if(scene=="crouch"||scene=="crawl") { input.movement={1,0}; input.crouchHeld=true; }
        // Sprint at the hole, then slide through it.
        if(scene=="slide") { input.movement={1,0}; input.sprintHeld=true; input.crouchPressed=frame==30; input.sprintPressed=frame==60; }
        // Slide tricks: a jump out of the slide (scissor twist); a boosted wall kick.
        if(scene=="slidehop") { input.movement={1,0}; input.sprintHeld=true; input.crouchPressed=frame==30; input.jumpPressed=frame==42; input.jumpHeld=frame<70; }
        if(scene=="walljump") { input.movement={1,0}; input.sprintHeld=true; input.jumpPressed=input.jumpHeld=game.wallTouching(); }
        // Walk onto the north-lane speed pad.
        if(scene=="boost") input.movement={1,0};
        // Sprint onto the pad and fly.
        if(scene=="flight"||scene=="launch"||scene=="dive") { input.movement={1,0}; input.sprintHeld=true; }
        // Boost tricks: dive from a flight (barrel roll), spring out of a slide (front flip).
        if(scene=="dive") input.crouchPressed=frame==34;
        if(scene=="flipup") { input.movement={1,0}; input.sprintHeld=true; input.crouchPressed=frame==20; input.sprintPressed=frame==40; }
        if(scene=="tunnel") { input.movement={1,0}; input.sprintHeld=true; input.crouchPressed=frame==24; }
        // Walk into the courtyard shield; take the spring boots and jump.
        if(scene=="shield"&&frame>12) input.movement={0,1};
        // Clock pickup: walk into it (+30 s).
        if(scene=="clock"&&frame>20) input.movement={1,0};
        // Map edge: look in from beyond the east wall.
        if(scene=="edge"&&frame==1) camera.orbit((camera.yaw()-1.5708F)/.0016F,(.35F-camera.pitch())/.0016F);
        if(scene=="clock"&&frame==1) camera.orbit(1230,-60);  // look east, toward the clock
        if(scene=="spring") { input.jumpPressed=frame==30; input.jumpHeld=frame<60; }
        // Slide across the pad for the burst.
        if(scene=="slidepad"||scene=="slidejump") { input.movement={1,0}; input.sprintHeld=true; input.crouchPressed=frame==20; }
        if(scene=="slidejump") { input.jumpPressed=frame==34; input.jumpHeld=frame<60; }
        // Walk through the opened blast door.
        if(scene=="door"&&frame>20) input.movement={1,0};
        if(scene=="jump") { input.movement={0,-1}; input.jumpPressed=frame==2; input.jumpHeld=frame<35; }
        if(scene=="exit"&&frame>50) input.movement={0,-1};
        if(scene=="orbit") camera.orbit(2,-.1F);
        if(scene=="follow") input.movement=camera.movement(0,1);
      }
      if(!running&&!arriving&&!transition.blocksGameplay()&&screen==Screen::Playing&&!options.gallery&&!options.diagnostics&&!cursorFree&&!ui.popupOpen()&&(focused||frameLimit>0)) {
        // Hit-stop runs the simulation at 8% speed for a few frames (it reads as a beat of
        // slow motion rather than a stalled frame); pressed edges wait until it ends.
        freeze=std::max(freeze,game.takeHitStop());
        if(freeze>0) {
          freeze-=dt; heldEdges.jumpPressed|=input.jumpPressed; heldEdges.interactPressed|=input.interactPressed; heldEdges.crouchPressed|=input.crouchPressed; heldEdges.sprintPressed|=input.sprintPressed;
          relay::Input slow=input; slow.jumpPressed=slow.interactPressed=slow.crouchPressed=slow.sprintPressed=false; game.update(dt*.08F,slow);
        }
        else { input.jumpPressed|=heldEdges.jumpPressed; input.interactPressed|=heldEdges.interactPressed; input.crouchPressed|=heldEdges.crouchPressed; input.sprintPressed|=heldEdges.sprintPressed; heldEdges=relay::Input{}; game.update(dt,input); }
      }
      if(arrival.active()&&screen==Screen::Playing&&(focused||frameLimit>0)) game.present(dt,arrival.elapsed);
      if(cameraRevision!=game.revision()) { camera.reset(game); cameraRevision=game.revision(); }
      if(!running) camera.update(game,dt);
      // Pause keeps the frozen level on screen; only the title shows the chamber.
      options.menu=screen==Screen::Title||(screen==Screen::Settings&&!settingsFromPause);
      if(options.menu) {
        const auto& io=ImGui::GetIO();
        if(io.MousePos.x>-1e6F&&io.DisplaySize.x>0) options.menuLook={io.MousePos.x/io.DisplaySize.x*2-1,io.MousePos.y/io.DisplaySize.y*2-1};
      }
      stage(0);
      renderer.prepare(game,options,running?0:frameLimit>0?float(frame)/60:float(now));
      stage(1);
      benchmark.beginSceneFrame(); renderer.draw(width,height,options,camera);
      stage(2);
      const bool completed=benchmark.endSceneFrame();
      if(screen==Screen::Settings) {
        // The first key pressed this frame feeds a pending rebind.
        int pressedKey=-1;
        for(int key:relay::KeyBindings::supportedKeys()) if(keys.pressed(key)) { pressedKey=key; break; }
        if(relay::drawSettings(relay::keyBindings(),frameLimit>0?float(frame)/60:float(now),pressedKey,keys.pressed(GLFW_KEY_ESCAPE),settingsFromPause))
          screen=settingsFromPause?Screen::Paused:Screen::Title;
      } else if(screen!=Screen::Playing) {
        const auto mode=screen==Screen::Title?relay::MenuMode::Title:relay::MenuMode::Pause;
        auto action=relay::drawMenu(mode,level,frameLimit>0?float(frame)/60:float(now),game.activeCheckpoint()>=0);
        if(action==relay::MenuAction::Start) { game.reset(relay::Game::missionsUnlocked?level:relay::Game::tutorialLevel); camera.reset(game); screen=Screen::Playing; options.gallery=false; cursorFree=false; options.diagnostics=false; arrival.start(); }
        if(action==relay::MenuAction::Resume) { screen=Screen::Playing; cursorFree=false; }
        if(action==relay::MenuAction::Gallery) { screen=Screen::Playing; options.gallery=true; arrival.cancel(); }
        if(action==relay::MenuAction::Title) { screen=Screen::Title; arrival.cancel(); }
        if(action==relay::MenuAction::Quit) window.requestClose();
        if(action==relay::MenuAction::Checkpoint) { game.returnToCheckpoint(); screen=Screen::Playing; }
        if(action==relay::MenuAction::Restart) { transition.start(game.level()); screen=Screen::Playing; }
        if(action==relay::MenuAction::Settings) { settingsFromPause=screen==Screen::Paused; screen=Screen::Settings; }
      } else if(arrival.active()) { relay::drawArrival(arrival.elapsed);
      } else if(record.empty()||!options.gallery) {
        if(ui.draw(benchmark.snapshot(),game,options,renderer,focused)&&!running) {
          savedOverview=options.overview; options.overview=true; startBenchmark(window,benchmark);
        }
      }
      if(const int request=ui.takeLevelRequest(); request>=0) transition.start(request);
      // Out of lives: once the shutdown has played, the level restarts behind the black transition.
      if(game.outOfLives()&&game.player().deathTime>=relay::Game::respawnDelay&&!transition.active()) transition.start(game.level());
      if(transition.active()) relay::drawLevelTransition(transition.black(),transition.card(),transition.progress(),transition.target,relay::Game::nameOf(transition.target),frameLimit>0?float(frame)/60:float(now));
      ui.render();
      stage(3);
      if(frameLimit>0&&frame>=20) frameMs.push_back(float((glfwGetTime()-frameStart)*1000));
      const auto error=glGetError(); if(error!=GL_NO_ERROR) throw std::runtime_error("OpenGL error "+std::to_string(error));
      ++frame;
      if(!record.empty()&&frame%2==0) {
        char name[32]; std::snprintf(name,sizeof(name),"frame-%04d.png",frame/2);
        relay::saveScreenshot(record/name,width,height);
      }
      if(frameLimit>0&&frame>=frameLimit) {
        if(!capture.empty()) relay::saveScreenshot(capture,width,height);
        std::cout<<"smoke_result frames="<<frame<<" level="<<game.level()+1<<" cores="<<game.collectedCount()<<" health="<<game.player().health<<" state="<<int(game.state())<<" elapsed="<<game.elapsed()<<" timer="<<game.timeRemaining()<<" arrival="<<arrival.active()<<" camera="<<(options.overview?"overview":"follow")<<" particles="<<game.liveParticleCount()<<" gl_error=0\n";
        if(!frameMs.empty()) {
          auto sorted=frameMs; std::sort(sorted.begin(),sorted.end());
          double sum=0; for(float v:frameMs) sum+=v; const double n=double(frameMs.size()),all=double(frame);
          std::cout<<std::fixed<<std::setprecision(2)<<"perf avg_ms="<<sum/n<<" p99_ms="<<sorted[std::size_t(n*.99)]<<" max_ms="<<sorted.back()<<" max_frame="<<20+(std::max_element(frameMs.begin(),frameMs.end())-frameMs.begin())
            <<" sim="<<stageMs[0]/all<<" prepare="<<stageMs[1]/all<<" draw="<<stageMs[2]/all<<" ui="<<stageMs[3]/all<<std::endl;
        }
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
