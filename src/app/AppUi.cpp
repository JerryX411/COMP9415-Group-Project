#include "app/AppUi.h"
#include "app/Minimap.h"
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

bool AppUi::draw(const BenchmarkSnapshot&,relay::Game& game,relay::RenderOptions& options,const relay::Renderer& renderer,bool) {
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
 // Match the advanced HUD positions without panels or outlines.
 const auto& player=game.player();
 const float t=game.elapsed(),hitAge=player.motion.hitAge;
 const float recoil=hitAge<.45F?std::exp(-10*hitAge)*std::sin(hitAge*65)*5*s:0;
 const ImU32 armourTone=player.health<=1?danger:player.health<=2?amber:glowCyan;
 const auto cell=[&](float x,float y,float w,float h,ImU32 colour){
  const ImVec2 points[]{{x+7*s,y},{x+w,y},{x+w-7*s,y+h},{x,y+h}};
  d->AddConvexPolyFilled(points,4,colour);
 };
 // A filled shield glyph, followed by five distinct armour cells.
 const ImVec2 shield[]{{46*s,40*s},{65*s,40*s},{65*s,52*s},{55.5F*s,63*s},{46*s,52*s}};
 d->AddConvexPolyFilled(shield,5,armourTone);
 for(int i=0;i<5;++i) {
  const float x=(80+i*42)*s+recoil,y=41*s;
  const bool full=i<player.health;
  cell(x,y,35*s,24*s,full?armourTone:IM_COL32(155,181,198,55));
  if(full)cell(x+2*s,y+2*s,31*s,3*s,withAlpha(ice,.7F));
  if(i==player.health&&hitAge<.5F) {
   const float fade=1-hitAge/.5F;
   cell(x,y-hitAge*18*s,35*s,24*s,withAlpha(danger,fade));
   for(int j=0;j<5;++j){const float angle=-2.5F+j*.8F;const float travel=hitAge*(50+j*12)*s;
    cell(x+16*s+std::cos(angle)*travel,y+10*s+std::sin(angle)*travel+hitAge*hitAge*100*s,7*s,4*s,withAlpha(danger,fade));}
  }
 }
 {
  // Lives: little robot heads under the armour (the one just lost flashes red). The tutorial
  // yard never runs out: one head and an infinity sign.
  const float ly=96*s;
  const auto head=[&](ImVec2 c,ImU32 colour,bool filled){
   if(filled)d->AddCircleFilled(c,11*s,withAlpha(colour,.18F),20);
   d->AddCircle(c,11*s,colour,20,2*s);
   for(float e:{-1.F,1.F})d->AddRectFilled({c.x+e*4.5F*s-2*s,c.y-3*s},{c.x+e*4.5F*s+2*s,c.y+3*s},colour);
  };
  const auto live=[&](ImVec2 c){d->AddCircleFilled(c,15*s,withAlpha(glowCyan,.22F),20);head(c,glowCyan,true);};
  if(!game.livesLimited()) {
   live({56*s,ly});
   ImVec2 inf[48];const ImVec2 ic(94*s,ly);
   for(int i=0;i<48;++i){const float a=float(i)*6.2831853F/48,dd=1+std::sin(a)*std::sin(a);inf[i]={ic.x+16*s*std::cos(a)/dd,ic.y+16*s*std::sin(a)*std::cos(a)/dd};}
   d->AddPolyline(inf,48,glowCyan,ImDrawFlags_Closed,2.5F*s);
  } else {
   const bool losing=game.state()==RunState::Lost&&player.health<=0;
   for(int i=0;i<Game::livesPerLevel;++i) {
    const ImVec2 c((56+i*34)*s,ly);
    const bool alive=i<game.lives(),justLost=losing&&i==game.lives();
    if(alive)live(c);
    else head(c,justLost?withAlpha(danger,.5F+.5F*std::sin(player.deathTime*14)):withAlpha(ice,.25F),justLost);
   }
  }
 }
 if(hudRevision_!=game.revision()){hudRevision_=game.revision();hudCoreCount_=game.collectedCount();coreCollectedAt_=-10;}
 if(game.collectedCount()>hudCoreCount_)coreCollectedAt_=t;
 hudCoreCount_=game.collectedCount();
 const float age=t-coreCollectedAt_;
 const int coreCount=int(game.cores().size());
 for(int i=0;i<coreCount;++i){
  const bool collected=i<hudCoreCount_;
  const float pop=collected&&i==hudCoreCount_-1&&age<.55F?std::sin(std::min(1.F,age/.55F)*3.14159F)*.35F:0;
  const ImVec2 c(size.x*.5F+(i-(coreCount-1)*.5F)*46*s,size.y-61*s);
  ImVec2 hex[6];for(int k=0;k<6;++k){const float angle=k*1.0471976F+.5235988F;hex[k]={c.x+std::cos(angle)*17*s*(1+pop),c.y+std::sin(angle)*17*s*(1+pop)};}
  d->AddConvexPolyFilled(hex,6,collected?mint:IM_COL32(155,181,198,75));
  if(collected)d->AddCircleFilled({c.x-4*s,c.y-5*s},3*s,withAlpha(ice,.8F),8);
 }
 const int seconds=int(std::ceil(game.timeRemaining()));
 char clock[48];
 if(game.timedLevel())std::snprintf(clock,sizeof(clock),"%d:%02d",seconds/60,seconds%60);
 else std::snprintf(clock,sizeof(clock),"UNLIMITED");
 const ImU32 clockColour=!game.timedLevel()?glowCyan:seconds<=10?danger:seconds<=30?amber:ice;
 if(game.timedLevel()) {
  const float font=82*s,width=drawTracked(nullptr,font,{},clockColour,clock,0);
  drawTracked(d,font,{size.x-48*s-width,28*s},clockColour,clock,0);
 } else {
  ImVec2 infinity[64];const ImVec2 c(size.x-125*s,69*s);
  for(int i=0;i<64;++i){const float angle=i*6.2831853F/64,denom=1+std::sin(angle)*std::sin(angle);infinity[i]={c.x+63*s*std::cos(angle)/denom,c.y+63*s*std::sin(angle)*std::cos(angle)/denom};}
  d->AddPolyline(infinity,64,clockColour,ImDrawFlags_Closed,7*s);
 }
 const auto key=[](KeyAction a){return KeyBindings::keyName(keyBindings()[a]);};
 // The showcase and Helios use symbols instead of teaching text, interaction prompts or key
 // hints (the text paths remain for the code-built fallback yard).
 const bool words=!game.wordless();
 if(game.state()==RunState::Playing&&words) {
  if(game.messageTime()>0)label(40,130,28,main,game.message());
  const auto available=game.availableInteraction();
  if(available!=Interaction::None)
   centered(height-155,34,mint,key(KeyAction::Interact)+(available==Interaction::Core?"  COLLECT CORE":available==Interaction::Portal?"  WAKE PORTAL":"  DISABLE TRAP"));
 }
 if(game.state()==RunState::Playing&&!words) {
  // The exit: a door glyph that always marks the gate, pinned to the screen edge (with a
  // chevron pointing on) while the gate is off screen. Amber while sealed, mint once open.
  const auto at=renderer.projectToEdge(game.gatePosition()+glm::vec3(0,3.4F,0),size.x,size.y,54*s,(options.overview?130:190)*s);
  const bool mapOverlap=!options.overview&&drawYardMinimap(*d,game,size,s).overlaps({at.x,at.y},44*s);
  if(options.overview) {
   // A constant-pixel locator remains legible even beneath an upper floor.
   const auto q=renderer.projectToEdge(player.position+glm::vec3(0,.9F,0),size.x,size.y,12*s,120*s);
   const ImVec2 p(q.x,q.y);
   d->AddCircle(p,10*s,IM_COL32(3,12,20,230),32,5*s);
   d->AddCircle(p,10*s,glowCyan,32,2*s);
   d->AddCircleFilled(p,2.5F*s,main,12);
  }
  // The minimap has its own exit direction cue when this glyph would cover it.
  if(!mapOverlap) {
   const bool open=game.gateUnlocked();const ImU32 c=open?mint:amber;
   const ImVec2 p(at.x,at.y);const float r=(options.overview?14:24)*s,pulse=open?.75F+.25F*std::sin(t*4):1.F;
   d->AddCircleFilled(p,r,withAlpha(slate,.62F),40);
   d->AddCircle(p,r,withAlpha(c,.9F*pulse),40,2*s);
   const float w=r*.62F,top=p.y-r*.55F,bottom=p.y+r*.55F;
   d->PathLineTo({p.x-w*.5F,bottom});d->PathLineTo({p.x-w*.5F,top+w*.5F});
   d->PathArcTo({p.x,top+w*.5F},w*.5F,3.14159265F,6.2831853F,12);d->PathLineTo({p.x+w*.5F,bottom});
   d->PathStroke(c,0,2.2F*s);
   d->AddLine({p.x-w*.75F,bottom},{p.x+w*.75F,bottom},c,2.2F*s);
   if(open) {
    d->AddRectFilled({p.x-w*.5F+2*s,top+w*.5F},{p.x+w*.5F-2*s,bottom-1*s},withAlpha(c,.35F*pulse));
    d->AddQuadFilled({p.x-w*.5F,top+w*.35F},{p.x-w*.05F,top+w*.6F},{p.x-w*.05F,bottom-2*s},{p.x-w*.5F,bottom},withAlpha(c,.9F));
   } else {
    d->AddRectFilled({p.x-w*.5F+3*s,top+w*.55F},{p.x+w*.5F-3*s,bottom-2*s},withAlpha(c,.55F));
    d->AddCircleFilled({p.x+w*.22F,(top+bottom)*.5F+r*.08F},1.8F*s,withAlpha(slate,.9F));
   }
   if(at.z<.5F) {
    const glm::vec2 dir=glm::normalize(glm::vec2(at.x-size.x*.5F,at.y-size.y*.5F)+glm::vec2(0,1e-3F));
    const glm::vec2 tip=glm::vec2(at.x,at.y)+dir*(r+10*s),side(-dir.y,dir.x);
    const glm::vec2 l=tip-dir*9.F*s+side*7.F*s,rr=tip-dir*9.F*s-side*7.F*s;
    d->AddTriangleFilled({tip.x,tip.y},{l.x,l.y},{rr.x,rr.y},withAlpha(c,.9F));
   }
  }
 }
 const std::string controls=key(KeyAction::Forward)+key(KeyAction::Left)+key(KeyAction::Back)+key(KeyAction::Right)+" MOVE   "+key(KeyAction::Sprint)+" SPRINT   "+key(KeyAction::Jump)+" JUMP   "+key(KeyAction::Crouch)+" CROUCH   "+key(KeyAction::Interact)+" INTERACT   "+key(KeyAction::Restart)+" RESTART   ESC PAUSE   F1 GRAPHICS";
 if(words)centered(height-28,20,secondary,controls);
 if(game.respawning()||game.outOfLives()) {
  // Reboot (full edition): a banner and a progress bar after the fall, then a fade to black for
  // the beam-in. With no lives left the banner turns red and the level restarts (main.cpp).
  const float since=player.deathTime,shown=std::clamp((since-.6F)/.4F,0.F,1.F);
  const float by=height*.30F;
  const bool out=game.outOfLives();
  if(words) {
   char reboot[96];
   if(out) std::snprintf(reboot,sizeof(reboot),"ALL LIVES LOST  //  RESTARTING");
   else if(game.livesLimited()) std::snprintf(reboot,sizeof(reboot),"REBOOTING  //  %d %s LEFT",game.lives(),game.lives()==1?"LIFE":"LIVES");
   else std::snprintf(reboot,sizeof(reboot),"REBOOTING");
   centered(by,40,withAlpha(out?danger:glowCyan,shown),reboot);
  }
  if(!out) {
   const float progress=std::clamp(since/Game::respawnDelay,0.F,1.F),barX=size.x*.5F-210*s;
   for(int i=0;i<24;++i) {
    const bool done=float(i)/24.F<progress;const ImU32 c=withAlpha(done?glowCyan:IM_COL32(112,162,192,255),(done?.9F:.3F)*shown);
    cell(barX+float(i)*17.5F*s,(by+64)*s,12*s,6*s,c);
   }
  }
  const float black=std::clamp((since-(Game::respawnDelay-.5F))/.45F,0.F,1.F);
  if(black>0)ImGui::GetForegroundDrawList()->AddRectFilled({0,0},size,withAlpha(IM_COL32(2,7,14,255),black));
 } else if(game.state()!=RunState::Playing) {
  centered(height*.4F,52,game.state()==RunState::Won?mint:danger,game.state()==RunState::Won?"LEVEL COMPLETE":game.timedOut()?"TIME UP":"ROBOT DISABLED");
  centered(height*.4F+78,32,main,clock);
  // After the last level the only way on is back to the main menu (Enter); R does nothing there.
  const bool won=game.state()==RunState::Won,final=won&&game.level()==Game::levelCount-1;
  centered(height*.4F+140,28,main,final?std::string("ENTER  MAIN MENU"):key(KeyAction::Restart)+(won?" RESTART   /   ENTER NEXT LEVEL":" RESTART"));
  if(won&&(ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false))) {
   if(final) menuRequest_=true; else levelRequest_=(game.level()+1)%Game::levelCount;
  }
 }
 return false;
}
void AppUi::drawDiagnostics(const relay::Game& game,const relay::RenderOptions& options,const relay::Renderer& renderer,const relay::FollowCamera& camera,bool frozen) {
 const auto size=ImGui::GetIO().DisplaySize;const float s=std::min(size.x/1440.F,size.y/900.F);
 auto* d=ImGui::GetForegroundDrawList();
 const auto text=[&](float x,float y,const std::string& value){relay::drawTracked(d,22*s,{x*s,y*s},relay::palette::ice,value.c_str(),0);};
 if(options.normalCompare){text(size.x/s*.25F-95,100,"NORMAL MAP OFF");text(size.x/s*.75F-95,100,"NORMAL MAP ON");}
 if(frozen)text(size.x/s*.5F-110,220,"FROZEN  /  F4 RESUME");
 if(camera.surveillance())text(size.x/s*.5F-90,34,"GLOBAL CAM  /  V");
 if(!options.diagnostics)return;
 const auto stats=renderer.stats();const auto eye=camera.eye();char value[180];float y=260;
 const auto row=[&](const std::string& v){text(40,y,v);y+=30;};
 row("F1 DIAGNOSTICS  |  F4 FREEZE: "+std::string(frozen?"ON":"OFF"));
 row("F2 NORMAL MAP: "+std::string(options.normalMapping?"ON":"OFF")+"  |  F3 SPLIT: "+(options.normalCompare?"ON":"OFF"));
 std::snprintf(value,sizeof(value),"F5 STRENGTH %.1f  |  F6 HIDDEN-FACE REMOVAL: %s",options.normalStrength,options.optimised?"ON":"OFF");row(value);
 std::snprintf(value,sizeof(value),"CAMERA  %.2f, %.2f, %.2f  |  FOV %.1f",eye.x,eye.y,eye.z,camera.fov());row(value);
 row("TRIANGLES "+std::to_string(stats.submittedTriangles)+"  |  DRAW CALLS "+std::to_string(stats.drawCalls));
 row("SOLID VOXELS "+std::to_string(stats.voxels)+"  |  EXPOSED FACES "+std::to_string(stats.visibleFaces));
 row("PARTICLES "+std::to_string(game.liveParticleCount())+" / "+std::to_string(game.particles().size())+"  |  WORLD 96 x 96 x 96");
 row("BENCHMARK: --benchmark --benchmark-output result.csv");
}
} // namespace craft::app
