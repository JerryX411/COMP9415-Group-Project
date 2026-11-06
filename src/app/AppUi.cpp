#include "app/AppUi.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "graphics/Window.h"
#include "graphics/OpenGL.h"
#include "core/AssetPath.h"
#include "app/RelayMenu.h"
#include "core/KeyBindings.h"
#include <string>
#include <utility>
#include <vector>
namespace craft::app {
AppUi::AppUi(graphics::Window& window) {
  IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename=nullptr;
  // Bold system face, with the bundled Rajdhani as a portable fallback.
  std::filesystem::path face=craft::core::assetPath("fonts/Rajdhani-SemiBold.ttf");
  if(const char* windows=std::getenv("WINDIR")) {
    const auto rounded=std::filesystem::path(windows)/"Fonts"/"segoeuib.ttf";
    std::error_code error; if(std::filesystem::exists(rounded,error)) face=rounded;
  }
  auto* fonts=ImGui::GetIO().Fonts;
  relay::setDisplayFont(fonts->AddFontFromFileTTF(face.string().c_str(),22.F));
  auto& s=ImGui::GetStyle(); s.WindowRounding=2; s.FrameRounding=2; s.WindowPadding={18,14};
  s.Colors[ImGuiCol_WindowBg]={.016F,.063F,.118F,.92F}; s.Colors[ImGuiCol_Border]={0,.88F,1,.45F};
  s.Colors[ImGuiCol_Text]={.84F,.97F,1,1}; s.Colors[ImGuiCol_Separator]={0,.88F,1,.35F};
  s.Colors[ImGuiCol_FrameBg]={.02F,.16F,.26F,1}; s.Colors[ImGuiCol_FrameBgHovered]={.03F,.25F,.38F,1};
  s.Colors[ImGuiCol_Header]={0,.55F,.75F,.6F}; s.Colors[ImGuiCol_HeaderHovered]={0,.70F,.90F,.7F};
  s.Colors[ImGuiCol_CheckMark]={0,.88F,1,1}; s.Colors[ImGuiCol_PopupBg]={.016F,.063F,.118F,.97F};
  s.Colors[ImGuiCol_Button]={0,.42F,.6F,1}; s.Colors[ImGuiCol_ButtonHovered]={0,.6F,.82F,1};
  if(!ImGui_ImplGlfw_InitForOpenGL(window.nativeHandle(),true)) { ImGui::DestroyContext(); throw std::runtime_error("ImGui GLFW initialization failed"); }
  if(!ImGui_ImplOpenGL3_Init("#version 330 core")) { ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); throw std::runtime_error("ImGui OpenGL initialization failed"); }
}
AppUi::~AppUi() {
  if(backdropFbo_[0]) { glDeleteFramebuffers(4,backdropFbo_); glDeleteTextures(4,backdropTex_); }
  ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
}
void AppUi::beginFrame() {
  ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
  const auto size=ImGui::GetIO().DisplaySize;
  const float scale=std::min(size.x/1440.F,size.y/900.F);
  if(scale>0&&scale!=prewarmedScale_) { relay::prewarmFonts(scale); prewarmedScale_=scale; }
}
void AppUi::render() { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }

bool AppUi::draw(const BenchmarkSnapshot&,relay::Game& game,relay::RenderOptions&,const relay::Renderer&,bool) {
 using namespace relay; using namespace relay::palette;
 auto* d=ImGui::GetBackgroundDrawList(); const auto size=ImGui::GetIO().DisplaySize;
 const float s=std::min(size.x/1440.F,size.y/900.F);
 const auto label=[&](float x,float y,float font,ImU32 colour,const std::string& value){
  drawTracked(d,font*s,{x*s,y*s},colour,value.c_str(),0);
 };
 const auto centered=[&](float y,float font,ImU32 colour,const std::string& value){
  const float width=drawTracked(nullptr,font*s,{},colour,value.c_str(),0);
  drawTracked(d,font*s,{(size.x-width)*.5F,y*s},colour,value.c_str(),0);
 };
 const ImU32 main=IM_COL32(242,247,252,255),secondary=IM_COL32(200,217,228,255);
 const float height=size.y/s;
 label(40,32,32,game.player().health<=1?danger:main,"ARMOUR  "+std::to_string(game.player().health)+" / 5");
 const std::string progress="CORES  "+std::to_string(game.collectedCount())+" / "+std::to_string(game.cores().size());
 label(size.x/s-340,32,30,main,game.levelName());
 label(size.x/s-390,78,28,game.gateUnlocked()?mint:main,progress+(game.gateUnlocked()?"  /  EXIT OPEN":""));
 const int seconds=int(std::ceil(game.timeRemaining()));
 char clock[48];
 if(game.timedLevel())std::snprintf(clock,sizeof(clock),"TIME  %02d:%02d",seconds/60,seconds%60);
 else std::snprintf(clock,sizeof(clock),"TIME  UNLIMITED");
 centered(32,36,game.timedLevel()&&game.timeRemaining()<=20?danger:main,clock);
 const auto key=[](KeyAction a){return KeyBindings::keyName(keyBindings()[a]);};
 if(game.state()==RunState::Playing) {
  label(40,182,26,secondary,game.tutorialHint());
  if(game.messageTime()>0)label(40,130,28,main,game.message());
  if(game.availableInteraction()!=Interaction::None)
   centered(height-155,34,mint,key(KeyAction::Interact)+(game.availableInteraction()==Interaction::Core?"  COLLECT CORE":"  RETRACT SPIKES"));
 }
 const std::string controls=key(KeyAction::Forward)+key(KeyAction::Left)+key(KeyAction::Back)+key(KeyAction::Right)+" MOVE   "+key(KeyAction::Jump)+" JUMP   "+key(KeyAction::Crouch)+" CROUCH   "+key(KeyAction::Interact)+" INTERACT   "+key(KeyAction::Restart)+" RESTART   ESC PAUSE";
 centered(height-60,26,secondary,controls);
 if(game.state()!=RunState::Playing) {
  centered(height*.4F,52,game.state()==RunState::Won?mint:danger,game.state()==RunState::Won?"LEVEL COMPLETE":game.timedOut()?"TIME UP":"ROBOT DISABLED");
  centered(height*.4F+78,32,main,clock);
  centered(height*.4F+140,28,main,key(KeyAction::Restart)+(game.state()==RunState::Won?" RESTART   /   ENTER NEXT LEVEL":" RESTART"));
  if(game.state()==RunState::Won&&ImGui::IsKeyPressed(ImGuiKey_Enter,false))levelRequest_=1-game.level();
 }
 return false;
}
} // namespace craft::app
