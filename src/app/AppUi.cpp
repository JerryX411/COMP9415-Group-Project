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
  // One rounded face for everything: Arial Rounded MT Bold is read from the Windows font folder at
  // run time (not redistributed with the project); elsewhere the bundled Rajdhani is used.
  std::filesystem::path face=craft::core::assetPath("fonts/Rajdhani-SemiBold.ttf");
  if(const char* windows=std::getenv("WINDIR")) {
    const auto rounded=std::filesystem::path(windows)/"Fonts"/"segoeui.ttf";
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
 drawAcrylicRect(d,{28*s,28*s},{350*s,132*s},12*s,.88F);
 drawTracked(d,22*s,{48*s,42*s},graphiteSoft,"ROBOT  /  ARMOUR",2*s,true);
 for(int i=0;i<5;++i) drawSlanted(d,{48*s+i*55*s,84*s},{92*s+i*55*s,108*s},6*s,i<game.player().health?IM_COL32(76,119,153,255):IM_COL32(208,211,216,255),steel);
 drawAcrylicRect(d,{size.x-340*s,28*s},{size.x-28*s,132*s},12*s,.88F);
 drawTracked(d,26*s,{size.x-320*s,48*s},graphite,game.levelName(),1*s,true);
 const std::string progress="CORES  "+std::to_string(game.collectedCount())+" / "+std::to_string(game.cores().size())+(game.gateUnlocked()?"   EXIT OPEN":"");
 drawTracked(d,20*s,{size.x-320*s,90*s},graphite,progress.c_str(),0);
 const int tenths=int(game.elapsed()*10);
 char clock[48];std::snprintf(clock,sizeof(clock),"TIME  %02d:%02d.%d",tenths/600,(tenths/10)%60,tenths%10);
 drawAcrylicRect(d,{size.x*.5F-145*s,28*s},{size.x*.5F+145*s,100*s},3*s,1);
 drawTracked(d,28*s,{size.x*.5F-122*s,50*s},graphite,clock,0);
 drawAcrylicRect(d,{28*s,208*s},{850*s,258*s},3*s,1);
 drawTracked(d,19*s,{44*s,222*s},graphite,game.tutorialHint(),0);
 drawAcrylicRect(d,{28*s,size.y-88*s},{size.x-28*s,size.y-28*s},10*s,.84F);
 const auto key=[](KeyAction a){return KeyBindings::keyName(keyBindings()[a]);};
 const std::string controls=key(KeyAction::Forward)+key(KeyAction::Left)+key(KeyAction::Back)+key(KeyAction::Right)+" MOVE   "+key(KeyAction::Jump)+" JUMP   "+key(KeyAction::Crouch)+" CROUCH   "+key(KeyAction::Interact)+" INTERACT   "+key(KeyAction::Restart)+" RESTART   ESC PAUSE";
 drawTracked(d,20*s,{48*s,size.y-70*s},graphite,controls.c_str(),.8F*s,true);
 if(game.availableInteraction()!=Interaction::None&&game.state()==RunState::Playing) {
  drawAcrylicRect(d,{size.x*.5F-225*s,size.y-180*s},{size.x*.5F+225*s,size.y-110*s},12*s,.94F);
  drawTracked(d,27*s,{size.x*.5F-195*s,size.y-160*s},graphite,(key(KeyAction::Interact)+(game.availableInteraction()==Interaction::Core?"  /  COLLECT CORE":"  /  RETRACT SPIKES")).c_str(),1*s,true);
 }
 if(game.messageTime()>0) {
  drawAcrylicRect(d,{28*s,146*s},{720*s,196*s},9*s,.9F);
  drawTracked(d,19*s,{44*s,160*s},graphite,game.message().c_str(),.3F*s);
 }
 if(game.state()==RunState::Won) {
  drawAcrylicRect(d,{size.x*.5F-280*s,size.y*.4F},{size.x*.5F+280*s,size.y*.4F+195*s},3*s,1);
  drawTracked(d,38*s,{size.x*.5F-210*s,size.y*.4F+24*s},graphite,"LEVEL COMPLETE",0);
  drawTracked(d,23*s,{size.x*.5F-230*s,size.y*.4F+140*s},graphite,(key(KeyAction::Restart)+" RESTART   |   ENTER NEXT LEVEL").c_str(),0);
  if(ImGui::IsKeyPressed(ImGuiKey_Enter,false))levelRequest_=1-game.level();
 }
 if(game.state()==RunState::Won) drawTracked(d,26*s,{size.x*.5F-125*s,size.y*.4F+87*s},graphite,clock,0);
 if(game.state()==RunState::Lost) {
  drawAcrylicRect(d,{size.x*.5F-280*s,size.y*.4F},{size.x*.5F+280*s,size.y*.4F+195*s},14*s,.97F);
  drawTracked(d,26*s,{size.x*.5F-125*s,size.y*.4F+87*s},graphite,clock,0);
  drawTracked(d,44*s,{size.x*.5F-245*s,size.y*.4F+24*s},danger,"ROBOT DISABLED",2*s,true);
  drawTracked(d,25*s,{size.x*.5F-190*s,size.y*.4F+140*s},graphite,("PRESS "+key(KeyAction::Restart)+" TO RESTART").c_str(),1*s,true);
 }
 return false;
}
} // namespace craft::app
