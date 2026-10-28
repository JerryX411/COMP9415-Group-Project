#include "app/AppUi.h"
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "graphics/Window.h"
#include "core/AssetPath.h"
namespace craft::app {
AppUi::AppUi(graphics::Window& window) {
  IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename=nullptr;
  ImGui::GetIO().Fonts->AddFontFromFileTTF(craft::core::assetPath("fonts/Rajdhani-SemiBold.ttf").string().c_str(),20.F);
  auto& s=ImGui::GetStyle(); s.WindowRounding=1; s.FrameRounding=1; s.WindowPadding={18,14};
  s.Colors[ImGuiCol_WindowBg]={.055F,.085F,.11F,.95F};
  s.Colors[ImGuiCol_Button]={.12F,.31F,.43F,1}; s.Colors[ImGuiCol_ButtonHovered]={.17F,.45F,.61F,1};
  if(!ImGui_ImplGlfw_InitForOpenGL(window.nativeHandle(),true)) { ImGui::DestroyContext(); throw std::runtime_error("ImGui GLFW initialization failed"); }
  if(!ImGui_ImplOpenGL3_Init("#version 330 core")) { ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); throw std::runtime_error("ImGui OpenGL initialization failed"); }
}
AppUi::~AppUi() { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); }
void AppUi::beginFrame() { ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame(); }
void AppUi::render() { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }
bool AppUi::draw(const BenchmarkSnapshot& benchmark, relay::Game& game, relay::RenderOptions& options, const relay::Renderer& renderer, bool focused) {
  const auto display=ImGui::GetIO().DisplaySize;
  constexpr auto fixed=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings;
  ImGui::SetNextWindowPos({20,20}); ImGui::SetNextWindowSize({options.gallery?385.F:350.F,0});
  ImGui::Begin("Mission",nullptr,fixed);
  ImGui::TextColored({.45F,.76F,.96F,1},"VOXEL RELAY  /  FACILITY 06"); ImGui::Separator();
  if(options.gallery) {
    ImGui::TextUnformatted("COURIER MOTION WORKSHOP");
    const char* clips[]{"Jog / planted steps","Sprint / body rhythm","Jump / tuck / landing","Brake / recovery step","Pivot / head leads","Idle / wrist check","Recover / fist pump","Impact / recoil","Victory / celebration","High drop / impact hold","Reactivation / wake"};
    ImGui::SetNextItemWidth(-1); ImGui::Combo("##Motion",&options.motionPreview,clips,11);
    ImGui::TextWrapped("Shared in-game rig: foot placement, weight shift, secondary motion and LED expressions.");
    ImGui::TextUnformatted("Press F3 to return to the test chamber.");
  } else {
    ImGui::Text("%02d  %s",game.level()+1,game.levelName());
    ImGui::Text("CORES  %d / %d",game.collectedCount(),int(game.cores().size()));
    ImGui::SameLine(190); ImGui::TextColored(game.timeRemaining()<30?ImVec4(1,.45F,.25F,1):ImVec4(1,1,1,1),"TIME  %03d",int(std::ceil(game.timeRemaining())));
    ImGui::ProgressBar(float(game.collectedCount())/float(game.cores().size()),{-1,5},"");
    ImGui::Text("ARMOUR  %d / 5",game.player().health);
    for(int i=0;i<5;++i) { if(i) ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_PlotHistogram,i<game.player().health?ImVec4(.28F,.65F,.86F,1):ImVec4(.22F,.28F,.31F,1)); ImGui::ProgressBar(1,{54,7},""); ImGui::PopStyleColor(); }
    ImGui::TextWrapped(game.gateUnlocked()?"Exit powered. Walk through the open gate.":"Approach each core and press E to recover it.");
    if(game.player().sprinting) ImGui::TextColored({.45F,.76F,.96F,1},"SPRINT ACTIVE");
    if(game.player().interactionTime>0) ImGui::TextColored({.45F,.76F,.96F,1},game.player().interaction==relay::Interaction::Core?"RECOVERING CORE...":"OPERATING RELAY...");
    if(!focused) ImGui::TextColored({1,.7F,.35F,1},"PAUSED - click the game to resume");
    if(game.messageTime()>0) { ImGui::PushTextWrapPos(0); ImGui::TextColored({.45F,.76F,.96F,1},"%s",game.message().c_str()); ImGui::PopTextWrapPos(); }
  }
  ImGui::End();
  ImGui::SetNextWindowPos({display.x*.5F,display.y-20},ImGuiCond_Always,{.5F,1});
  ImGui::Begin("Controls",nullptr,fixed);
  ImGui::TextUnformatted("WASD Move   SHIFT Sprint   MOUSE Look   SPACE Jump   E Interact   TAB Cursor   ESC Pause");
  ImGui::TextColored({.66F,.75F,.77F,1},"SPACE Tap for a low jump / hold for height   |   WHEEL Zoom   |   Five hits destroy the courier");
  ImGui::End();
  if(options.collisionOverlay&&!options.gallery) {
    auto* draw=ImGui::GetBackgroundDrawList();
    const auto line=[&](glm::vec3 a,glm::vec3 b,ImU32 colour) {
      const auto pa=renderer.project(a,display.x,display.y),pb=renderer.project(b,display.x,display.y);
      // project() returns an off-screen sentinel for points behind the camera.
      if(pa.x<-1000||pb.x<-1000) return;
      draw->AddLine({pa.x,pa.y},{pb.x,pb.y},colour,1.5F);
    };
    const auto box=[&](relay::Bounds bounds,ImU32 colour) {
      for(int axis=0;axis<3;++axis) for(int a=0;a<2;++a) for(int b=0;b<2;++b) {
        auto start=bounds.min,end=bounds.min;
        start[(axis+1)%3]=end[(axis+1)%3]=a?bounds.max[(axis+1)%3]:bounds.min[(axis+1)%3];
        start[(axis+2)%3]=end[(axis+2)%3]=b?bounds.max[(axis+2)%3]:bounds.min[(axis+2)%3];
        end[axis]=bounds.max[axis]; line(start,end,colour);
      }
    };
    const auto ring=[&](glm::vec3 p,float radius,ImU32 colour) {
      for(int i=0;i<40;++i) {
        const float a=i*6.2831853F/40,b=(i+1)*6.2831853F/40;
        line(p+glm::vec3(std::cos(a)*radius,0,std::sin(a)*radius),p+glm::vec3(std::cos(b)*radius,0,std::sin(b)*radius),colour);
      }
    };
    const auto feet=game.player().position;
    ring(feet,relay::Game::playerRadius,IM_COL32(100,210,255,255));
    ring(feet+glm::vec3(0,relay::Game::playerHeight,0),relay::Game::playerRadius,IM_COL32(100,210,255,255));
    ring(feet+glm::vec3(0,.02F,0),relay::Game::footRadius,IM_COL32(255,225,80,255));
    for(const auto& mover:game.movers()) for(const auto& part:mover.collisionParts()) box(part,IM_COL32(255,150,60,255));
    for(const auto& trap:game.traps()) for(float x=trap.area.min.x+.5F;x<trap.area.max.x;x+=1)
      for(float z=trap.area.min.z+.5F;z<trap.area.max.z;z+=1) {
        auto tile=trap.tileBounds(x,z); tile.max.y=trap.area.min.y+relay::FloorTrap::surfaceHeight;
        box(tile,IM_COL32(255,110,90,255));
      }
  }
  auto label=[&](glm::vec3 p,const char* text,ImU32 colour) {
    auto xy=renderer.project(p,display.x,display.y); auto size=ImGui::CalcTextSize(text);
    const ImVec2 start(xy.x-size.x*.5F,xy.y-size.y*.5F); auto* draw=ImGui::GetBackgroundDrawList();
    draw->AddRectFilled({start.x-6,start.y-4},{start.x+size.x+6,start.y+size.y+4},IM_COL32(12,24,30,225),4);
    draw->AddText(start,colour,text);
  };
  if(!options.gallery) {
    label(game.player().position+glm::vec3(0,2.35F,0),"YOU",IM_COL32(220,255,245,255));
    label(game.gatePosition()+glm::vec3(0,3.7F,0),game.gateUnlocked()?"EXIT / OPEN":"EXIT / LOCKED",IM_COL32(220,255,245,255));
    for(std::size_t i=0;i<game.traps().size();++i) {
      const auto phase=game.trapPhase(i);
      const char* name=phase==relay::TrapPhase::Active?"DANGER":phase==relay::TrapPhase::Warning?"WARNING":phase==relay::TrapPhase::Disabled?"DISABLED":"SAFE";
      char text[64]; std::snprintf(text,sizeof(text),"%s  %.1fs",name,game.trapPhaseRemaining(i));
      auto centre=(game.traps()[i].area.min+game.traps()[i].area.max)*.5F;
      label(centre+glm::vec3(0,.45F,0),text,phase==relay::TrapPhase::Active?IM_COL32(255,130,100,255):IM_COL32(245,229,170,255));
    }
    const auto available=game.availableInteraction();
    if(available==relay::Interaction::Core) label(game.cores()[std::size_t(game.nearestCore())].position+glm::vec3(0,.65F,0),"E / RECOVER CORE",IM_COL32(160,255,220,255));
    if(available==relay::Interaction::Switch) label(game.switches()[std::size_t(game.nearestSwitch())].position+glm::vec3(0,1.65F,0),"E / DISABLE FOR 5s",IM_COL32(255,224,150,255));
  }
  if(game.state()!=relay::RunState::Playing&&!options.gallery && (game.state()==relay::RunState::Won||game.player().deathTime>1.1F)) {
    ImGui::SetNextWindowPos({display.x*.5F,display.y*.26F},ImGuiCond_Always,{.5F,.5F}); ImGui::SetNextWindowSize({360,0});
    ImGui::Begin("Result",nullptr,fixed);
    ImGui::TextColored({.45F,.76F,.96F,1},game.state()==relay::RunState::Won?"RELAY COMPLETE":game.player().health<=0?"COURIER DESTROYED":"POWER DEPLETED");
    ImGui::Text("Cores: %d / %d   Time: %.1fs",game.collectedCount(),int(game.cores().size()),game.timeRemaining());
    if(ImGui::Button("Restart level [R]")) game.reset(game.level());
    ImGui::SameLine(); if(ImGui::Button("Other level")) game.reset(1-game.level()); ImGui::End();
  }
  bool startBenchmark=false;
  if(options.diagnostics||benchmark.running) {
    ImGui::SetNextWindowPos({display.x-20,20},ImGuiCond_FirstUseEver,{1,0}); ImGui::SetNextWindowSize({310,0});
    ImGui::Begin("Whitebox tools [F1]",nullptr,ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BeginDisabled(benchmark.running);
    if(ImGui::Button("Courtyard [1]")) game.reset(0);
    ImGui::SameLine(); if(ImGui::Button("Corridors [2]")) game.reset(1);
    ImGui::Checkbox("Motion workshop [F3]",&options.gallery); ImGui::Checkbox("Whitebox materials",&options.whitebox);
    ImGui::Checkbox("Overview camera [F2]",&options.overview);
    ImGui::Checkbox("Show collision shapes",&options.collisionOverlay);
    if(options.collisionOverlay) ImGui::TextWrapped("Blue: body. Yellow: feet. Orange/red: hazard geometry. Close F1 to play with this overlay.");
    ImGui::Checkbox("Remove internal voxel faces",&options.optimised); ImGui::Checkbox("Normal mapping",&options.normalMapping); ImGui::EndDisabled();
    const auto& s=renderer.stats(); ImGui::SeparatorText("Scene diagnostics");
    ImGui::Text("Voxels %d / 32768",s.voxels); ImGui::Text("Faces %d / %d",s.visibleFaces,s.totalFaces);
    ImGui::Text("Triangles %d | Draws %d",s.submittedTriangles,s.drawCalls); ImGui::Text("Particles %d / 384",game.liveParticleCount());
    const auto p=game.player().position; ImGui::Text("Player %.2f %.2f %.2f",p.x,p.y,p.z); ImGui::Text("Grounded: %s",game.player().grounded?"yes":"no");
    ImGui::SeparatorText("1920 x 1080 / fixed scene"); startBenchmark=drawBenchmarkSection(benchmark); ImGui::End();
  }
  return startBenchmark;
}
bool AppUi::drawBenchmarkSection(const BenchmarkSnapshot& b) const {
  if(b.running) {
    if(b.warmupFrames<b.requiredWarmupFrames) ImGui::Text("Warm-up: %zu / %zu",b.warmupFrames,b.requiredWarmupFrames);
    else ImGui::Text("Measured: %zu / %zu",b.measuredFrames,b.requiredMeasuredFrames);
    return false;
  }
  if(b.complete) { ImGui::Text("Mean %.4f ms/frame",b.averageMilliseconds); ImGui::Text("Samples %zu | FPS %.1f",b.measuredFrames,b.averageFramesPerSecond); }
  return ImGui::Button("Run benchmark");
}
}
