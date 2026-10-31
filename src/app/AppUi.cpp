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
    const auto rounded=std::filesystem::path(windows)/"Fonts"/"ARLRDBD.TTF";
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
void AppUi::render() { ImGui::Render(); captureBackdrop(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }
void AppUi::captureBackdrop() {
  // Acrylic: the frame so far (the 3D scene, before any UI) is halved four times with linear
  // filtering; the sixteenth-size copy, magnified back with linear filtering, reads as a soft blur.
  const auto& io=ImGui::GetIO();
  const int width=int(io.DisplaySize.x*io.DisplayFramebufferScale.x),height=int(io.DisplaySize.y*io.DisplayFramebufferScale.y);
  if(width<32||height<32) return;
  GLint read=0,draw=0,texture=0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
  if(width!=backdropWidth_||height!=backdropHeight_) {
    if(backdropFbo_[0]) { glDeleteFramebuffers(4,backdropFbo_); glDeleteTextures(4,backdropTex_); }
    glGenFramebuffers(4,backdropFbo_); glGenTextures(4,backdropTex_);
    for(int i=0;i<4;++i) {
      glBindTexture(GL_TEXTURE_2D,backdropTex_[i]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,std::max(1,width>>(i+1)),std::max(1,height>>(i+1)),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER,backdropFbo_[i]);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,backdropTex_[i],0);
    }
    backdropWidth_=width; backdropHeight_=height;
  }
  GLuint source=GLuint(draw); int w=width,h=height;
  for(int i=0;i<4;++i) {
    const int nw=std::max(1,width>>(i+1)),nh=std::max(1,height>>(i+1));
    glBindFramebuffer(GL_READ_FRAMEBUFFER,source); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,backdropFbo_[i]);
    glBlitFramebuffer(0,0,w,h,0,0,nw,nh,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    source=backdropFbo_[i]; w=nw; h=nh;
  }
  glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read)); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw)); glBindTexture(GL_TEXTURE_2D,GLuint(texture));
  relay::setBackdrop(ImTextureID(backdropTex_[3]));
}
bool AppUi::draw(const BenchmarkSnapshot& benchmark, relay::Game& game, relay::RenderOptions& options, const relay::Renderer& renderer, bool focused) {
  const auto display=ImGui::GetIO().DisplaySize;
  constexpr auto fixed=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings;
  const float s=std::min(display.x/1440.F,display.y/900.F);
  auto* hud=ImGui::GetBackgroundDrawList();
  using namespace relay::palette;
  using relay::withAlpha;
  if(options.gallery) {
    ImGui::SetNextWindowPos({20,20}); ImGui::SetNextWindowSize({385.F,0});
    ImGui::Begin("Mission",nullptr,fixed);
    ImGui::TextColored({0,.88F,1,1},"VOXEL RELAY  //  MOTION WORKSHOP"); ImGui::Separator();
    const char* clips[]{"Jog / planted steps","Sprint / body rhythm","Jump / tuck / landing","Brake / recovery step","Pivot / head leads","Idle / wrist check","Recover / fist pump","Impact / recoil","Victory / celebration","High drop / impact hold","Reactivation / wake","Greeting / wave","Crouch / sneak","Electric shock","Slide / heel skid","Low flight / speed pad","Boosted stride / speed pad"};
    ImGui::SetNextItemWidth(-1); ImGui::Combo("##Motion",&options.motionPreview,clips,17);
    ImGui::TextWrapped("Shared in-game rig: foot placement, weight shift, secondary motion and LED expressions.");
    ImGui::TextUnformatted("Press F3 to return to the test chamber.");
    ImGui::End();
  } else {
    // Holographic HUD: sector panel top-left, countdown top-right, banners centred.
    const auto& player=game.player();
    const float t=game.elapsed();
    // Screen-edge feedback: red on hits, mint on pickups, white on heavy landings, a blast on shutdown.
    const auto edges=[&](ImU32 colour,float alpha,float depth) {
      if(alpha<=.01F) return;
      const ImU32 on=withAlpha(colour,alpha),off=withAlpha(colour,0);
      const float w=display.x*depth,h=display.y*depth;
      hud->AddRectFilledMultiColor({0,0},{w,display.y},on,off,off,on);
      hud->AddRectFilledMultiColor({display.x-w,0},display,off,on,on,off);
      hud->AddRectFilledMultiColor({0,0},{display.x,h},on,on,off,off);
      hud->AddRectFilledMultiColor({0,display.y-h},display,off,off,on,on);
    };
    if(player.hurtCooldown>.6F&&player.deathTime<=0&&player.motion.spawnAge>1) {
      // Blunt: a heavy red bruise at the edges. Electric: cyan edges that flicker.
      const float k=(player.hurtCooldown-.6F)/.5F;
      if(player.motion.hitKind==1) edges(cyan,.55F*k*(std::sin(float(ImGui::GetTime())*61.F)>0?1.F:.35F),.2F);
      else edges(danger,.7F*k*k,.24F);
    }
    if(player.motion.shockAge<.8F) {
      // Electrocution glitch: thin bars tear across the screen.
      const float glitch=std::clamp((.8F-player.motion.shockAge)/.3F,0.F,1.F);
      const auto hash=[](float n) { const float v=std::sin(n*12.9898F)*43758.5453F; return v-std::floor(v); };
      const float seed=std::floor(float(ImGui::GetTime())*24.F);
      for(int i=0;i<5;++i) {
        const float by=hash(seed*7.F+float(i))*display.y,bh=(2+hash(seed*13.F+float(i))*8)*s,shift=(hash(seed*5.F+float(i))-.5F)*40*s;
        hud->AddRectFilled({shift,by},{display.x+shift,by+bh},withAlpha(i%2?cyan:ice,.18F*glitch));
      }
    }
    {
      // Speed lines: above sprint speed, streaks rush in from the screen edges (anime style),
      // re-dealt 30 times a second; a flight also tints the edges violet.
      const float v=glm::length(player.locomotion),rush=std::clamp((v-relay::Game::sprintSpeed-.3F)/5.F,0.F,1.F);
      if(rush>0&&game.state()==relay::RunState::Playing) {
        const auto hash=[](float n) { const float h=std::sin(n*12.9898F)*43758.5453F; return h-std::floor(h); };
        const ImVec2 c(display.x*.5F,display.y*.52F); const float r=std::hypot(display.x,display.y)*.5F,deal=std::floor(t*30.F);
        for(int i=0;i<36;++i) {
          const float a=hash(deal*3.1F+float(i)*1.7F)*6.2831853F,start=r*(.58F+.25F*hash(deal*5.3F+float(i))),len=r*(.08F+.22F*hash(deal*7.9F+float(i)))*rush;
          const ImVec2 p0(c.x+std::cos(a)*start,c.y+std::sin(a)*start),p1(c.x+std::cos(a)*(start+len),c.y+std::sin(a)*(start+len));
          const ImU32 streak=game.jetting()?(i%3==0?IM_COL32(255,240,170,255):IM_COL32(255,205,60,255)):i%4==0?IM_COL32(176,150,255,255):ice;
          hud->AddLine(p0,p1,withAlpha(streak,(.12F+.22F*hash(deal+float(i)*9.1F))*rush*(game.jetting()?1.4F:1.F)),(1.F+2.F*hash(deal*2.3F+float(i)))*s);
        }
      }
      if(player.flightBlend>.01F) edges(IM_COL32(150,110,255,255),.16F*player.flightBlend,.13F);
    }
    if(player.motion.celebrateAge<.6F) edges(mint,.35F*(1-player.motion.celebrateAge/.6F),.16F);
    // A crimson tile taking time: a short crimson pulse at the edges, and the screen is "hacked"
    // for a moment: a few red and black glitch slices torn sideways, columns of red hex code on
    // black strips and a corrupted error block, re-dealt ten times a second (kept soft and slow
    // enough not to strobe) and thinning out as it fades.
    if(game.timePenaltyAge()<.45F) edges(IM_COL32(220,20,45,255),.22F*(1-game.timePenaltyAge()/.45F),.12F);
    if(game.timePenaltyAge()<.7F&&game.state()==relay::RunState::Playing) {
      const float age=game.timePenaltyAge(),fade=1-age/.7F,deal=std::floor(float(ImGui::GetTime())*10.F);
      const auto hash=[](float n) { const float h=std::sin(n*12.9898F)*43758.5453F; return h-std::floor(h); };
      const ImU32 red=IM_COL32(255,36,60,255),black=IM_COL32(0,0,0,255);
      for(int i=0;i<5;++i) {
        if(hash(deal*1.3F+float(i))>.3F+.7F*fade) continue;
        const float y=hash(deal*7.F+float(i)*3.F)*display.y,h=(3+22*hash(deal*13.F+float(i)))*s,shift=(hash(deal*5.F+float(i))-.5F)*80*s;
        hud->AddRectFilled({shift,y},{display.x+shift,y+h},withAlpha(i%2?red:black,(i%2?.16F:.25F)*fade));
      }
      const char* digits="0123456789ABCDEF";
      for(int c=0;c<5;++c) {
        if(hash(deal*2.1F+float(c)*9.F)>.4F+.6F*fade) continue;
        const float x=hash(float(c)*17.3F+std::floor(deal/6.F))*display.x,y0=hash(float(c)*5.9F+std::floor(deal/6.F))*display.y*.7F;
        const int rows=5+int(9*hash(float(c)*3.3F));
        for(int r=0;r<rows;++r) {
          char line[16];
          const auto byte=[&](int k) { return digits[int(hash(deal*.7F+float(c*31+r*7+k))*16)%16]; };
          std::snprintf(line,sizeof(line),r%4==0?"0x%c%c%c%c":"%c%c %c%c",byte(0),byte(1),byte(2),byte(3));
          const float ry=y0+float(r)*20*s,w=relay::drawTracked(nullptr,16*s,{},0,line,2*s);
          hud->AddRectFilled({x-4*s,ry-1*s},{x+w+4*s,ry+19*s},withAlpha(black,.35F*fade));
          relay::drawTracked(hud,16*s,{x,ry},withAlpha(r==rows-1?IM_COL32(255,255,255,255):red,fade*(.35F+.3F*hash(deal+float(c*13+r)))),line,2*s);
        }
      }
      for(int i=0;i<1;++i) {
        if(hash(deal*3.9F+float(i)*11.F)>.5F*fade+.15F) continue;
        const ImVec2 a(hash(deal*4.4F+float(i))*display.x*.8F,hash(deal*6.6F+float(i))*display.y*.8F),b(a.x+(60+120*hash(deal+float(i)*2.F))*s,a.y+(16+30*hash(deal*9.F+float(i)))*s);
        hud->AddRectFilled(a,b,withAlpha(red,.3F*fade));
        for(float yy=a.y+4*s;yy<b.y-3*s;yy+=6*s) hud->AddRectFilled({a.x+4*s,yy},{a.x+4*s+(b.x-a.x-8*s)*hash(yy+deal),yy+2.5F*s},withAlpha(black,.5F*fade));
      }
    }
    if(player.motion.shieldBreakAge<.35F) edges(ice,.32F*(1-player.motion.shieldBreakAge/.35F),.12F);
    // A portal jump: a bright cyan-white flash that clears in a moment (it also hides the cut).
    if(player.motion.teleportAge<.35F) { const float k=1-player.motion.teleportAge/.35F; hud->AddRectFilled({0,0},display,withAlpha(IM_COL32(210,250,255,255),.6F*k*k)); edges(glowCyan,.4F*k,.2F); }
    // Jetpack: a big yellow flash at ignition, then the edges shimmer yellow while the boost runs.
    if(player.motion.jetAge<.25F) { const float k=1-player.motion.jetAge/.25F; hud->AddRectFilled({0,0},display,withAlpha(IM_COL32(255,230,120,255),.28F*k*k)); edges(IM_COL32(255,200,50,255),.45F*k,.18F); }
    else if(game.jetting()) edges(IM_COL32(255,205,60,255),.13F+.07F*std::sin(t*41.F),.14F);
    if(player.motion.landAge<.25F&&player.motion.landStrength>1.1F) edges(ice,.25F*(1-player.motion.landAge/.25F),.12F);
    if(game.state()==relay::RunState::Lost) {
      if(player.deathTime<.3F) hud->AddRectFilled({0,0},display,withAlpha(IM_COL32(255,240,220,255),.55F*(1-player.deathTime/.3F)));
      edges(danger,.45F*std::clamp(1.4F-player.deathTime,0.F,1.F),.24F);
    }
    // Status bars carry no words. Top left: armour cells, then lives and the shield. Bottom
    // centre: the cores. Top right: the clock, with any running power-up timers under it.
    char text[96];
    const auto infinity=[&](ImVec2 c,float r,ImU32 colour,float thickness) {
      ImVec2 path[48];
      for(int i=0;i<48;++i) { const float a=float(i)*6.2831853F/48,d=1+std::sin(a)*std::sin(a); path[i]={c.x+r*std::cos(a)/d,c.y+r*std::sin(a)*std::cos(a)/d}; }
      hud->AddPolyline(path,48,colour,ImDrawFlags_Closed,thickness);
    };
    const auto head=[&](ImVec2 c,ImU32 colour,bool filled) {
      if(filled) hud->AddCircleFilled(c,11*s,withAlpha(colour,.18F),20);
      hud->AddCircle(c,11*s,colour,20,2*s);
      for(float e:{-1.F,1.F}) hud->AddRectFilled({c.x+e*4.5F*s-2*s,c.y-3*s},{c.x+e*4.5F*s+2*s,c.y+3*s},colour);
    };
    const ImVec2 pa(22*s,20*s);
    const int statusStart=hud->VtxBuffer.Size;
    // No backing: armour, lives, the shield and the jetpack sit straight on the scene.
    {
      // Armour: a small shield outline, then five slanted cells in recessed slots. A full cell glows
      // cyan (amber at two cells, red at one, pulsing) with a soft bloom and a bright top edge. A hit
      // breaks the lost cell like a struck card: it punches out red with a twist, shards fly off it,
      // and the row shakes.
      const float hitAge=player.hurtCooldown>0&&player.health<5?1.1F-player.hurtCooldown:10.F;
      const int rowStart=hud->VtxBuffer.Size;
      const ImVec2 c(pa.x+34*s,pa.y+32*s);
      const ImVec2 outline[5]{{c.x-8*s,c.y-9*s},{c.x+8*s,c.y-9*s},{c.x+8*s,c.y+1*s},{c.x,c.y+10*s},{c.x-8*s,c.y+1*s}};
      hud->AddPolyline(outline,5,withAlpha(deepTeal,.85F),ImDrawFlags_Closed,2*s);
      const ImU32 tone=player.health<=1?danger:player.health<=2?amber:glowCyan;
      const float bloom=player.health<=2?.18F+.14F*std::sin(t*6.F):.22F;
      const float lostFlash=player.hurtCooldown>0?.5F+.5F*std::sin(player.hurtCooldown*28):0;
      for(int i=0;i<5;++i) {
        const ImVec2 a(pa.x+58*s+float(i)*38*s,pa.y+22*s),b(a.x+32*s,a.y+20*s);
        const bool full=i<player.health,justLost=i==player.health&&player.hurtCooldown>0;
        const int cellStart=hud->VtxBuffer.Size;
        relay::drawSlanted(hud,a,b,7*s,withAlpha(ice,.16F),withAlpha(ice,.08F));
        if(full) {
          relay::drawSlanted(hud,{a.x-3*s,a.y-3*s},{b.x+3*s,b.y+3*s},8*s,withAlpha(tone,bloom),withAlpha(tone,bloom*.7F));
          relay::drawSlanted(hud,a,b,7*s,tone,withAlpha(tone,.8F));
          relay::drawSlanted(hud,{a.x+.85F*7*s,a.y},{b.x+.85F*7*s,a.y+3*s},1.05F*s,withAlpha(IM_COL32(255,255,255,255),.6F),withAlpha(IM_COL32(255,255,255,255),.15F));
        } else if(justLost) relay::drawSlanted(hud,a,b,7*s,withAlpha(danger,lostFlash),withAlpha(danger,lostFlash));
        if(justLost&&hitAge<.6F) {
          const ImVec2 mid((a.x+b.x)*.5F,(a.y+b.y)*.5F);
          relay::transformSince(hud,cellStart,mid,.3F*std::exp(-8.F*hitAge)*std::sin(hitAge*30),relay::punch(hitAge,.6F,26,9));
          const float f=hitAge/.6F;
          for(int k=0;k<8;++k) {
            const float angle=-2.7F+float(k)*.33F,speed=(170+90*float(k%3))*s;
            const ImVec2 q(mid.x+std::cos(angle)*speed*hitAge,mid.y+std::sin(angle)*speed*hitAge+560*s*hitAge*hitAge);
            const float r=(4+float(k%3)*2)*s*(1-.5F*f);
            const ImU32 shard=withAlpha(k%2?danger:cyan,1-f*f);
            relay::drawSlanted(hud,{q.x-r,q.y-r*.6F},{q.x+r,q.y+r*.6F},r*.6F,shard,shard);
          }
        }
      }
      if(hitAge<.5F) relay::transformSince(hud,rowStart,{pa.x+140*s,pa.y+32*s},0,1,{std::exp(-9.F*hitAge)*9*s*std::sin(hitAge*62),std::exp(-9.F*hitAge)*4*s*std::sin(hitAge*47)});
    }
    {
      // Lives as little courier heads (the one just lost flashes red; training shows a head and an
      // infinity sign), then the shield: a hex that lights while one is up and flashes as it breaks.
      const float ly=pa.y+70*s;
      // Lives left glow: deep teal heads on a cyan bloom; spent ones fade to graphite.
      const auto live=[&](ImVec2 c) { hud->AddCircleFilled(c,15*s,withAlpha(glowCyan,.22F),20); head(c,deepTeal,true); };
      if(!game.timed()) { live({pa.x+34*s,ly}); infinity({pa.x+72*s,ly},16*s,deepTeal,2.5F*s); }
      else {
        const bool losing=game.state()==relay::RunState::Lost&&player.health<=0;
        for(int i=0;i<relay::Game::livesPerLevel;++i) {
          const ImVec2 c(pa.x+34*s+float(i)*34*s,ly);
          const bool alive=i<game.lives(),justLost=losing&&i==game.lives();
          if(alive) live(c);
          else head(c,justLost?withAlpha(danger,.5F+.5F*std::sin(player.deathTime*14)):withAlpha(ice,.25F),justLost);
        }
      }
      hud->AddLine({pa.x+150*s,ly-12*s},{pa.x+150*s,ly+12*s},withAlpha(ice,.25F),1.2F*s);
      const ImVec2 hc(pa.x+180*s,ly);
      const float breakAge=player.motion.shieldBreakAge,broke=breakAge<.7F?1-breakAge/.7F:0;
      ImVec2 hex[6]; for(int k=0;k<6;++k) { const float a=float(k)*1.0472F; hex[k]={hc.x+std::cos(a)*12*s,hc.y+std::sin(a)*12*s}; }
      const int shieldStart=hud->VtxBuffer.Size;
      if(player.shielded) {
        hud->AddCircleFilled(hc,19*s,withAlpha(glowCyan,.25F),20); hud->AddConvexPolyFilled(hex,6,glowCyan);
        ImVec2 inner[6]; for(int k=0;k<6;++k) inner[k]={hc.x+(hex[k].x-hc.x)*.45F,hc.y+(hex[k].y-hc.y)*.45F};
        hud->AddConvexPolyFilled(inner,6,withAlpha(IM_COL32(255,255,255,255),.6F));
      } else hud->AddPolyline(hex,6,broke>0?withAlpha(glowCyan,broke):withAlpha(ice,.35F),ImDrawFlags_Closed,2*s);
      {
        // Jetpack charge: two little tanks, lit orange with a flame under each while a burst is
        // ready; they flare as it fires.
        const ImVec2 jc(pa.x+232*s,ly);
        const bool ready=player.jets>0; const float fire=player.motion.jetAge<.4F?1-player.motion.jetAge/.4F:0;
        const ImU32 orange=IM_COL32(255,140,50,255);
        if(ready||fire>0) hud->AddCircleFilled(jc,15*s,withAlpha(orange,.2F+.3F*fire),20);
        for(float side:{-1.F,1.F}) {
          const ImVec2 a(jc.x+side*5*s-3.5F*s,jc.y-8*s),b(jc.x+side*5*s+3.5F*s,jc.y+5*s);
          if(ready||fire>0) {
            hud->AddRectFilled(a,b,withAlpha(orange,ready?1.F:fire),3*s);
            hud->AddTriangleFilled({a.x+.5F*s,b.y+1*s},{b.x-.5F*s,b.y+1*s},{(a.x+b.x)*.5F,b.y+(6+5*fire+std::sin(t*30)*1.5F)*s},withAlpha(IM_COL32(255,220,140,255),ready?.9F:fire));
          } else hud->AddRect(a,b,withAlpha(ice,.35F),3*s,0,1.5F*s);
        }
      }
      std::snprintf(text,sizeof(text),"%d",player.shielded?1:0);
      relay::drawTracked(hud,24*s,{hc.x+22*s,ly-13*s},player.shielded?deepTeal:broke>0?withAlpha(danger,broke):withAlpha(ice,.55F),text,1*s,true);
      if(broke>0) {
        // A breaking shield shatters like a struck card: a white flash and a swell with a twist,
        // two hex shock rings, sparks, and the hex itself split into twelve wedges that fly apart,
        // spin and fall. The whole status corner shakes (below) and the screen edges flash.
        const float age=breakAge,f=1-broke;
        if(age<.12F) hud->AddConvexPolyFilled(hex,6,withAlpha(IM_COL32(255,255,255,255),1-age/.12F));
        hud->AddCircleFilled(hc,(18+26*f)*s,withAlpha(ice,.35F*broke*broke),24);
        relay::transformSince(hud,shieldStart,hc,.45F*std::exp(-7.F*age)*std::sin(age*24),relay::punch(age,1.F,26,8));
        for(int ring=0;ring<2;++ring) {
          const float k=std::clamp((age-float(ring)*.08F)/.45F,0.F,1.F);
          if(k>0&&k<1) hud->AddCircle(hc,(14+(70+float(ring)*30)*k)*s,withAlpha(ring?cyan:ice,(1-k)*(1-k)),6,(4-2*k)*s);
        }
        for(int k=0;k<10;++k) {
          const float angle=float(k)*.628F+.2F,k0=std::min(1.F,age/.3F);
          if(k0>=1) continue;
          const float r0=(16+40*k0)*s,r1=(16+(40+30*float(k%2))*k0+26*(1-k0))*s;
          hud->AddLine({hc.x+std::cos(angle)*r0,hc.y+std::sin(angle)*r0},{hc.x+std::cos(angle)*r1,hc.y+std::sin(angle)*r1},withAlpha(k%3?ice:cyan,1-k0),2.5F*s);
        }
        for(int k=0;k<12;++k) {
          const ImVec2 v0=hex[k/2],v1=hex[(k/2+1)%6],mid((v0.x+v1.x)*.5F,(v0.y+v1.y)*.5F);
          const ImVec2 p0=hc,p1=k%2?mid:v0,p2=k%2?v1:mid;
          const ImVec2 centroid((p0.x+p1.x+p2.x)/3,(p0.y+p1.y+p2.y)/3);
          const float dx=centroid.x-hc.x,dy=centroid.y-hc.y,len=std::max(.001F,std::sqrt(dx*dx+dy*dy));
          const float speed=(150+60*float(k%3))*s,drag=(1-std::exp(-3.F*age))/3.F,spin=(k%2?1.F:-1.F)*(6+float(k%4))*age,grow=1.9F-.6F*f;
          const ImVec2 at(centroid.x+dx/len*speed*drag,centroid.y+dy/len*speed*drag+300*s*age*age);
          const float co=std::cos(spin)*grow,si=std::sin(spin)*grow;
          const auto place=[&](ImVec2 p) { const float x=p.x-centroid.x,y=p.y-centroid.y; return ImVec2(at.x+x*co-y*si,at.y+x*si+y*co); };
          hud->AddTriangleFilled(place(p0),place(p1),place(p2),withAlpha(k%3==0?IM_COL32(255,255,255,255):k%3==1?cyan:ice,broke));
        }
      }
    }
    if(player.motion.shieldBreakAge<.5F) {
      const float age=player.motion.shieldBreakAge,k=std::exp(-8.F*age);
      relay::transformSince(hud,statusStart,{pa.x+120*s,pa.y+50*s},.03F*k*std::sin(age*40),1,{k*12*s*std::sin(age*64),k*7*s*std::sin(age*51+1.1F)});
    }
    {
      // Cores, bottom centre: hexagonal sockets that fill and glow (no count).
      const int cores=int(game.cores().size()),collected=game.collectedCount();
      const float w=float(cores)*40*s+32*s;
      const ImVec2 ka((display.x-w)*.5F,display.y-92*s),kb((display.x+w)*.5F,display.y-30*s);
      const float fresh=player.motion.celebrateAge<.6F?1-player.motion.celebrateAge/.6F:0;
      for(int i=0;i<cores;++i) {
        const ImVec2 c(ka.x+36*s+float(i)*40*s,(ka.y+kb.y)*.5F);
        const int start=hud->VtxBuffer.Size;
        ImVec2 hex[6]; for(int k=0;k<6;++k) { const float a=float(k)*1.0472F+.5236F; hex[k]={c.x+std::cos(a)*14*s,c.y+std::sin(a)*14*s}; }
        if(i<collected) { hud->AddCircleFilled(c,22*s,withAlpha(mint,.16F+.3F*(i==collected-1?fresh:0)),24); hud->AddConvexPolyFilled(hex,6,mint); }
        else hud->AddPolyline(hex,6,withAlpha(steel,.9F),ImDrawFlags_Closed,2*s);
        if(i==collected-1&&fresh>0) {
          // Just recovered: the socket pops with a twist, a hex ring snaps outward, shards scatter.
          const float age=player.motion.celebrateAge,f=age/.6F;
          relay::transformSince(hud,start,c,.35F*std::exp(-7.F*age)*std::sin(age*22),relay::punch(age,.8F,24,8));
          hud->AddCircle(c,(16+70*age)*s,withAlpha(mint,fresh*fresh),6,3*s*fresh);
          for(int k=0;k<10;++k) {
            const float angle=float(k)*.628F+.3F,speed=(170+70*float(k%3))*s;
            const ImVec2 q(c.x+std::cos(angle)*speed*age,c.y+std::sin(angle)*speed*age+320*s*age*age);
            const float r=(3.5F+float(k%3)*1.5F)*s*(1-.5F*f);
            hud->AddRectFilled({q.x-r,q.y-r},{q.x+r,q.y+r},withAlpha(k%3?mint:ice,1-f*f));
          }
        }
      }
      // Boost meter just above the cores, only while a boost runs: spring boots (a green chevron
      // with a draining ring) and the speed-pad bar. The bar shows a flight's clock (pulsing while
      // grounded), or how much pad speed a slide or stride still carries. It glows white-blue and
      // sheds sparks from its leading edge.
      {
        const bool sprung=player.spring>0;
        const float boost=game.boostMeter();
        const bool pulse=game.boostTimeLeft()>0&&player.boost!=relay::Boost::Flight;
        const float dt=std::clamp(ImGui::GetIO().DeltaTime,0.F,.1F);
        if(sprung||boost>0) {
          const float gy=ka.y-18*s,barWidth=std::max(w-60*s,180*s);
          const float total=(sprung?30*s:0)+(boost>0?barWidth:0);
          float gx=(display.x-total)*.5F;
          if(sprung) {
            const ImU32 green=IM_COL32(120,255,150,255);
            const ImVec2 c(gx+12*s,gy);
            for(int k=0;k<2;++k) { const float y=c.y+3*s-float(k)*7*s; hud->AddPolyline(std::array<ImVec2,3>{ImVec2{c.x-7*s,y+4*s},ImVec2{c.x,y-3*s},ImVec2{c.x+7*s,y+4*s}}.data(),3,green,0,2.5F*s); }
            hud->PathArcTo(c,13*s,-1.5708F,-1.5708F+6.2832F*player.spring/relay::Game::springTime,24); hud->PathStroke(green,0,2*s);
            gx+=30*s;
          }
          if(boost>0) {
            const ImU32 glow=IM_COL32(215,240,255,255);
            const float alpha=pulse?.6F+.4F*std::sin(t*10.F):1.F;
            const ImVec2 bar0(gx+6*s,gy-3.5F*s),bar1(gx+barWidth,gy+3.5F*s),head(bar0.x+(bar1.x-bar0.x)*boost,gy);
            hud->AddRectFilled(bar0,bar1,withAlpha(navy,.35F),3*s);
            // Soft glow: widening, fading layers around the filled part, then the white-blue core.
            for(int i=3;i>=1;--i) { const float g=float(i)*3.F*s; hud->AddRectFilled({bar0.x-g,bar0.y-g},{head.x+g,bar1.y+g},withAlpha(glow,.07F*alpha),3*s+g); }
            hud->AddRectFilled(bar0,{head.x,bar1.y},withAlpha(glow,alpha),3*s);
            hud->AddRectFilled({bar0.x,gy-1*s},{head.x,gy+1*s},withAlpha(IM_COL32(255,255,255,255),alpha),1*s);
            hud->AddCircleFilled(head,9*s,withAlpha(glow,.25F*alpha),16); hud->AddCircleFilled(head,4.5F*s,withAlpha(IM_COL32(255,255,255,255),alpha),12);
            // Sparks: shed from the leading edge (and now and then along the bar), drifting up.
            sparkClock_+=dt;
            while(sparkClock_>1.F/100) {
              sparkClock_-=1.F/100;
              const auto rnd=[&]{ sparkSeed_=sparkSeed_*1664525u+1013904223u; return float(sparkSeed_>>8)/16777216.F; };
              const bool edge=rnd()<.7F;
              const ImVec2 at=edge?head:ImVec2{bar0.x+(head.x-bar0.x)*rnd(),gy};
              const float a=-1.5708F+(rnd()-.5F)*2.6F,sp=(40.F+90.F*rnd())*s;
              hudSparks_.push_back({at,{std::cos(a)*sp-(edge?30.F*s:0),std::sin(a)*sp},0,.35F+.4F*rnd(),(1.6F+2.2F*rnd())*s});
            }
          }
        }
        // Spark update and draw (they finish their flight after the bar has gone).
        for(auto& k:hudSparks_) { k.age+=dt; k.p.x+=k.v.x*dt; k.p.y+=k.v.y*dt; k.v.y+=60.F*s*dt; k.v.x*=std::exp(-2.F*dt); }
        hudSparks_.erase(std::remove_if(hudSparks_.begin(),hudSparks_.end(),[](const HudSpark& k){ return k.age>=k.life; }),hudSparks_.end());
        if(hudSparks_.size()>160) hudSparks_.erase(hudSparks_.begin(),hudSparks_.begin()+long(hudSparks_.size()-160));
        for(const auto& k:hudSparks_) {
          const float f=1-k.age/k.life;
          hud->AddCircleFilled(k.p,k.size*2.6F,withAlpha(IM_COL32(170,215,255,255),.18F*f),8);
          hud->AddCircleFilled(k.p,k.size,withAlpha(IM_COL32(235,248,255,255),f),6);
        }
      }
    }
    // Countdown: a flat sky-blue card in the top right (no frame) with self-lit digits (an infinity
    // sign in training); the digits glow amber under 30 s and red under 10 s, when the card pulses.
    // Time changes hit it like a scored card in a deck-builder. A crimson tile slashes it: a blade
    // streak sweeps across, the card splits along it and snaps back with a hard shake, twist and
    // punch, sparks spray off the cut and shards fly, and a frameless -5 S stamps down beside it,
    // wobbles and falls away. A clock pickup pops the card gold and +30 S springs up and floats off.
    const float remaining=game.timeRemaining();
    std::snprintf(text,sizeof(text),"%d:%02d",int(std::ceil(remaining))/60,int(std::ceil(remaining))%60);
    const auto mix=[](ImU32 x,ImU32 y,float k) {
      const ImVec4 p=ImGui::ColorConvertU32ToFloat4(x),q=ImGui::ColorConvertU32ToFloat4(y);
      return ImGui::ColorConvertFloat4ToU32({p.x+(q.x-p.x)*k,p.y+(q.y-p.y)*k,p.z+(q.z-p.z)*k,p.w+(q.w-p.w)*k});
    };
    // Turn and scale everything drawn since `start` about c, then move it by offset.
    const auto transform=[&](int start,ImVec2 c,float angle,float scale,ImVec2 offset) { relay::transformSince(hud,start,c,angle,scale,offset); };
    const ImU32 ink=IM_COL32(12,34,68,255),sky=IM_COL32(236,238,242,255),white=IM_COL32(255,255,255,255);
    const ImU32 gold=IM_COL32(255,196,60,255),crimson=IM_COL32(236,34,62,255),skyGlow=IM_COL32(255,255,255,255);
    const bool urgent=game.timed()&&remaining<10;
    const ImU32 digits=!game.timed()?deepTeal:urgent?IM_COL32(205,32,58,255):remaining<30?IM_COL32(190,116,12,255):deepTeal;
    const ImU32 halo=urgent?IM_COL32(255,90,120,255):remaining<30&&game.timed()?amber:glowCyan;  // the digits glow on the acrylic
    (void)skyGlow;
    ImU32 fill=urgent?mix(sky,IM_COL32(250,200,210,255),.3F+.3F*std::sin(t*12)):sky;
    const float bonusAge=game.timeBonusAge(),penaltyAge=game.timePenaltyAge();
    // Envelopes: the slash is a hard, fast damped jolt; the pop a softer overshoot.
    const float hit=penaltyAge<.7F?std::exp(-9.F*penaltyAge):0,pop=bonusAge<.7F?std::exp(-7.F*bonusAge):0;
    const ImVec2 shake(hit*18*s*std::sin(penaltyAge*70),hit*9*s*std::sin(penaltyAge*53+1.3F));
    const float twist=hit*.16F*std::sin(penaltyAge*42+.4F)+pop*.07F*std::sin(bonusAge*28);
    const float punch=1+hit*.26F*std::cos(penaltyAge*34)+pop*.22F*std::cos(bonusAge*24);
    if(penaltyAge<.05F) fill=white;
    else if(penaltyAge<.24F) fill=mix(crimson,fill,(penaltyAge-.05F)/.19F);
    if(bonusAge<.16F) fill=mix(fill,gold,1-bonusAge/.16F);
    const float clockWidth=game.timed()?relay::drawTracked(nullptr,82*s,{},0,text,3*s,true):150*s;
    const ImVec2 ca(display.x-clockWidth-78*s,20*s),cb(display.x-22*s,118*s),centre((ca.x+cb.x)*.5F,(ca.y+cb.y)*.5F);
    const auto card=[&](ImVec2 offset) {
      const int start=hud->VtxBuffer.Size;
      relay::drawAcrylicRect(hud,ca,cb,14*s,1,fill);
      const ImU32 under=withAlpha(white,.6F);  // a light emboss under the digits
      if(game.timed()) {
        const ImVec2 at(cb.x-clockWidth-26*s,ca.y+8*s);
        relay::drawTracked(hud,82*s,{at.x,at.y+3*s},under,text,3*s,true);
        relay::drawGlow(hud,82*s,at,digits,halo,text,3*s);
      } else {
        const ImVec2 c(cb.x-26*s-clockWidth*.5F,ca.y+49*s);
        infinity({c.x,c.y+3*s},58*s,under,5*s); infinity(c,58*s,withAlpha(halo,.3F),11*s); infinity(c,58*s,digits,4.5F*s);
      }
      transform(start,centre,twist,punch,{shake.x+offset.x,shake.y+offset.y});
    };
    // The cut: a shallow rising line through the card. Just after the blade passes, the halves
    // above and below it are pushed apart along it, then snap back together.
    const float slashAngle=-.12F,sweep=.06F;
    const ImVec2 along(std::cos(slashAngle),std::sin(slashAngle)),across(-along.y,along.x);
    const float split=penaltyAge>=sweep*.5F&&penaltyAge<.4F?18*s*std::exp(-11.F*(penaltyAge-sweep*.5F)):0;
    if(split>.5F*s) {
      hud->PushClipRect({0,0},{display.x,centre.y},true);
      card({-along.x*split-across.x*split*.45F,-along.y*split-across.y*split*.45F});
      hud->PopClipRect();
      hud->PushClipRect({0,centre.y},display,true);
      card({along.x*split+across.x*split*.45F,along.y*split+across.y*split*.45F});
      hud->PopClipRect();
    } else card({0,0});
    if(penaltyAge<.6F) {
      const auto hash=[](float n) { const float h=std::sin(n*12.9898F)*43758.5453F; return h-std::floor(h); };
      const float half=(cb.x-ca.x)*.5F+80*s;
      const ImVec2 start(centre.x-along.x*half+shake.x,centre.y-along.y*half+shake.y);
      // The blade streak: the tip sweeps across in `sweep` seconds, then the tail chases it and the
      // streak thins away; a white core inside a crimson edge.
      const float head=std::min(1.F,penaltyAge/sweep),tail=std::clamp((penaltyAge-sweep*.6F)/.2F,0.F,1.F);
      if(tail<1) {
        const float width=(1-tail)*(1-tail);
        const ImVec2 h0(start.x+along.x*half*2*head,start.y+along.y*half*2*head),t0(start.x+along.x*half*2*tail,start.y+along.y*half*2*tail);
        const ImVec2 mid((h0.x+t0.x)*.5F,(h0.y+t0.y)*.5F);
        for(int layer=0;layer<2;++layer) {
          const float w=(layer?8.F:20.F)*s*width;
          const ImVec2 q[4]{t0,{mid.x+across.x*w,mid.y+across.y*w},h0,{mid.x-across.x*w,mid.y-across.y*w}};
          hud->AddConvexPolyFilled(q,4,layer?white:withAlpha(crimson,.85F));
        }
      }
      // Sparks off the cut: streaks thrown out to both sides as the tip passes their spot.
      for(int i=0;i<40;++i) {
        const float u=hash(float(i)*1.37F),born=u*sweep,age=penaltyAge-born,life=.3F+.3F*hash(float(i)*4.1F);
        if(age<0||age>life) continue;
        const float side=i%2?1.F:-1.F,speed=(320+520*hash(float(i)*2.9F))*s,fwd=.2F+.9F*hash(float(i)*6.3F);
        const ImVec2 v(along.x*speed*fwd+across.x*speed*side,along.y*speed*fwd+across.y*speed*side);
        const float drag=(1-std::exp(-5.F*age))/5.F,f=age/life;
        const ImVec2 o(start.x+along.x*half*2*u,start.y+along.y*half*2*u);
        const ImVec2 p(o.x+v.x*drag,o.y+v.y*drag+420*s*age*age),tailP(p.x-v.x*.035F*(1-f),p.y-v.y*.035F*(1-f));
        hud->AddLine(tailP,p,withAlpha(f<.35F?white:i%3?crimson:gold,1-f*f),(3.2F-1.8F*f)*s);
      }
      // Shards of the card: flat spinning squares in its colours, flung out and falling.
      for(int i=0;i<16;++i) {
        const float age=penaltyAge-.02F,life=.55F+.15F*hash(float(i)*8.7F);
        if(age<0||age>life) continue;
        const float angle=1.35F+2.F*hash(float(i)*3.3F),speed=(260+260*hash(float(i)*9.1F))*s,f=age/life;
        const float drag=(1-std::exp(-3.5F*age))/3.5F;
        const ImVec2 c(centre.x+std::cos(angle)*(60*s+speed*drag),centre.y+std::sin(angle)*(24*s+speed*drag)+620*s*age*age);
        const float r=(5+5*hash(float(i)*2.2F))*s*(1-f*.5F),spin=float(i)*1.1F+age*(10+float(i%3)*5);
        ImVec2 q[4];
        for(int k=0;k<4;++k) q[k]={c.x+std::cos(spin+float(k)*1.5708F)*r,c.y+std::sin(spin+float(k)*1.5708F)*r*.7F};
        hud->AddQuadFilled(q[0],q[1],q[2],q[3],withAlpha(i%3==0?white:i%3==1?crimson:sky,1-f*f));
      }
    }
    // Frameless numbers beside the card. A stamp slams down from twice its size with a hard
    // overshoot; a pop springs up from nothing. Both wobble, hold, then fall (or float) away.
    const auto number=[&](const char* label,float age,float life,ImU32 colour,ImU32 edge,ImVec2 at,float lean,bool stamp) {
      if(age>=life) return;
      const float size=66*s,w=relay::drawTracked(nullptr,size,{},0,label,3*s,true);
      const float in=std::min(1.F,age/.34F);
      const float scale0=stamp?1+1.3F*std::exp(-13.F*age)*std::cos(age*24.F):std::max(0.F,1-std::exp(-7.F*in)*std::cos(in*12.F));
      const float out=std::clamp((age-life*.62F)/(life*.38F),0.F,1.F),alpha=std::min(1.F,age/.03F)*(1-out*out);
      const float angle=lean+.45F*std::exp(-6.F*age)*std::cos(age*20.F)+(stamp?.5F:-.2F)*out*out;
      const int start=hud->VtxBuffer.Size;
      const ImVec2 p(at.x-w*.5F,at.y-size*.62F);
      relay::drawTracked(hud,size,{p.x,p.y+6*s},withAlpha(ink,.35F*alpha),label,3*s,true);  // hard shadow
      for(int k=0;k<8;++k) {
        const float a=float(k)*.785398F;
        relay::drawTracked(hud,size,{p.x+std::cos(a)*3.5F*s,p.y+std::sin(a)*3.5F*s},withAlpha(edge,alpha),label,3*s,true);
      }
      relay::drawTracked(hud,size,p,withAlpha(colour,alpha),label,3*s,true);
      const float fall=stamp?out*out*90*s:-out*out*60*s;
      transform(start,at,angle,scale0*(1-.25F*out),{0,fall});
    };
    char label[16];
    std::snprintf(label,sizeof(label),"+%d S",int(relay::Game::timeBonus));
    number(label,bonusAge,1.6F,gold,IM_COL32(90,50,0,255),{ca.x+10*s,cb.y+44*s},-.1F,false);
    std::snprintf(label,sizeof(label),"-%d S",int(relay::Game::drainPenalty));
    number(label,penaltyAge,1.3F,crimson,IM_COL32(60,4,16,255),{ca.x+10*s-(bonusAge<1.6F?190*s:0.F),cb.y+44*s},-.14F,true);
    // Banners: flat cards (cut corners, hard shadow, an accent stripe). Given an age they are dealt
    // in like a card, growing with an elastic overshoot from a twist that settles, then sway gently.
    const auto banner=[&](const char* message,ImU32 accent,float alpha,float by,float age=-1) {
      const float w=relay::drawTracked(nullptr,28*s,{},0,message,2.5F*s,true);
      const float slide=age>=0?0:(1-alpha)*-20*s;
      const ImVec2 a((display.x-w)*.5F-34*s,by+slide),b((display.x+w)*.5F+26*s,by+slide+52*s);
      const int start=hud->VtxBuffer.Size;
      relay::drawPanel(hud,a,b,12*s,withAlpha(slate,alpha),withAlpha(accent,alpha),1.4F*s);
      hud->AddRectFilled({a.x+11*s,a.y+13*s},{a.x+15*s,b.y-13*s},withAlpha(accent,.85F*alpha));
      relay::drawTracked(hud,28*s,{a.x+30*s,a.y+11.5F*s},withAlpha(IM_COL32(255,255,255,255),.6F*alpha),message,2.5F*s,true);  // emboss
      relay::drawTracked(hud,28*s,{a.x+30*s,a.y+10*s},withAlpha(graphite,alpha),message,2.5F*s,true);
      if(age>=0) relay::transformSince(hud,start,{(a.x+b.x)*.5F,(a.y+b.y)*.5F},-.14F*std::exp(-6.F*age)*std::cos(age*18)+.008F*std::sin(t*1.6F),relay::elasticIn(age,.32F));
    };
    if(game.messageTime()>0&&game.state()==relay::RunState::Playing) banner(game.message().c_str(),cyan,std::min({1.F,game.messageTime()/.4F,(3.5F-game.messageTime())/.15F}),24*s,3.5F-game.messageTime());
    if(!focused) banner("PAUSED  //  CLICK TO RESUME",amber,1,display.y*.42F);
    if(game.respawning()) {
      // Reboot: a banner and progress bar after the fall, then a fade to black for the beam-in.
      const float since=player.deathTime,shown=std::clamp((since-.9F)/.4F,0.F,1.F),progress=std::clamp(since/relay::Game::respawnDelay,0.F,1.F);
      const float by=display.y*.30F;
      char reboot[96]; std::snprintf(reboot,sizeof(reboot),"%s  //  %d %s LEFT",game.activeCheckpoint()>=0?"REBOOTING AT CHECKPOINT":"REBOOTING AT ENTRY POINT",game.lives(),game.lives()==1?"LIFE":"LIVES");
      banner(reboot,cyan,shown,by,since-.9F);
      const float barX=display.x*.5F-210*s;
      for(int i=0;i<24;++i) {
        const bool done=float(i)/24.F<progress; const ImU32 c=withAlpha(done?cyan:steel,(done?.9F:.3F)*shown);
        relay::drawSlanted(hud,{barX+float(i)*17.5F*s,by+64*s},{barX+float(i)*17.5F*s+12*s,by+70*s},3*s,c,c);
      }
      const float black=std::clamp((since-(relay::Game::respawnDelay-.6F))/.5F,0.F,1.F);
      if(black>0) ImGui::GetForegroundDrawList()->AddRectFilled({0,0},display,withAlpha(IM_COL32(2,7,14,255),black));
    }
    if(game.outOfLives()) {
      // Last life gone: a red banner, then the screen goes black and the level restarts.
      const float since=player.deathTime,shown=std::clamp((since-.9F)/.4F,0.F,1.F);
      banner("ALL LIVES LOST  //  RESTARTING SECTOR",danger,shown,display.y*.30F,since-.9F);
      const float black=std::clamp((since-(relay::Game::respawnDelay-.6F))/.5F,0.F,1.F);
      if(black>0) ImGui::GetForegroundDrawList()->AddRectFilled({0,0},display,withAlpha(IM_COL32(2,7,14,255),black));
    }
    if(player.motion.spawnAge<.3F) ImGui::GetForegroundDrawList()->AddRectFilled({0,0},display,withAlpha(IM_COL32(2,7,14,255),1-player.motion.spawnAge/.3F));
    // Controls fade out once the player is moving; ESC brings the menu back.
    const float hint=std::clamp((12.F-t)/2.F,0.F,1.F);
    if(hint>0&&game.state()==relay::RunState::Playing) {
      // Hints follow the player's bindings.
      const auto& kb=relay::keyBindings(); using relay::KeyAction; using relay::KeyBindings;
      const std::string move=KeyBindings::keyName(kb[KeyAction::Forward])+KeyBindings::keyName(kb[KeyAction::Left])+KeyBindings::keyName(kb[KeyAction::Back])+KeyBindings::keyName(kb[KeyAction::Right]);
      const std::string sprint=KeyBindings::keyName(kb[KeyAction::Sprint]),jump=KeyBindings::keyName(kb[KeyAction::Jump]),crouch=KeyBindings::keyName(kb[KeyAction::Crouch]),use=KeyBindings::keyName(kb[KeyAction::Interact]);
      const char* rows[][2]{{move.c_str(),"MOVE"},{sprint.c_str(),"SPRINT"},{jump.c_str(),"JUMP"},{crouch.c_str(),"CROUCH/SLIDE"},{use.c_str(),"INTERACT"},{"L MOUSE","WALL"},{"MOUSE","LOOK"},{"ESC","PAUSE"}};
      // A column on the right edge, keycaps right-aligned, on a soft backing.
      float keyWidth=0,labelWidth=0;
      for(auto& r:rows) { keyWidth=std::max(keyWidth,relay::drawKeycap(nullptr,s,{},0,r[0])); labelWidth=std::max(labelWidth,relay::drawTracked(nullptr,16*s,{},0,r[1],2.5F*s)); }
      const float rowStep=40*s,columnWidth=labelWidth+14*s+keyWidth,right=display.x-30*s;
      const float top=display.y*.5F-float(std::size(rows))*rowStep*.5F;
      const ImVec2 backA(right-columnWidth-22*s,top-16*s),backB(right+14*s,top+float(std::size(rows))*rowStep+4*s);
      relay::drawAcrylicRect(hud,backA,backB,10*s,hint,surface);
      for(std::size_t i=0;i<std::size(rows);++i) {
        const float y=top+float(i)*rowStep;
        const float kw=relay::drawKeycap(nullptr,s,{},0,rows[i][0]);
        relay::drawKeycap(hud,s,{right-kw,y},withAlpha(cyan,hint),rows[i][0]);
        const float lw=relay::drawTracked(nullptr,16*s,{},0,rows[i][1],2.5F*s);
        relay::drawTracked(hud,16*s,{right-keyWidth-14*s-lw,y+5*s},withAlpha(graphite,.85F*hint),rows[i][1],2.5F*s);
      }
    }
  }
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
    ring(feet+glm::vec3(0,game.bodyHeight(),0),relay::Game::playerRadius,IM_COL32(100,210,255,255));
    ring(feet+glm::vec3(0,.02F,0),relay::Game::footRadius,IM_COL32(255,225,80,255));
    for(const auto& mover:game.movers()) for(const auto& part:mover.collisionParts()) box(part,IM_COL32(255,150,60,255));
    for(const auto& trap:game.traps()) for(float x=trap.area.min.x+.5F;x<trap.area.max.x;x+=1)
      for(float z=trap.area.min.z+.5F;z<trap.area.max.z;z+=1) {
        auto tile=trap.tileBounds(x,z); tile.max.y=trap.area.min.y+relay::FloorTrap::surfaceHeight;
        box(tile,IM_COL32(255,110,90,255));
      }
  }
  // World labels: small flat tags (cut corners, hard shadow, a coloured stripe) that sway a little
  // on their own beat; prompts show the key to press.
  auto label=[&](glm::vec3 p,const char* text,ImU32 accent,const char* key=nullptr) {
    const auto xy=renderer.project(p,display.x,display.y); if(xy.x<-1000) return;
    if(xy.y<175*s) return;  // Never under the top HUD panels.
    const float keyWidth=key?30*s+10*s:0;
    const float w=relay::drawTracked(nullptr,24*s,{},0,text,2*s,true)+keyWidth;
    const ImVec2 a(xy.x-w*.5F-20*s,xy.y-21*s),b(xy.x+w*.5F+16*s,xy.y+21*s);
    const int start=hud->VtxBuffer.Size;
    relay::drawPanel(hud,a,b,9*s,slate,accent,1.3F*s);
    hud->AddRectFilled({a.x+7*s,a.y+10*s},{a.x+10*s,b.y-10*s},withAlpha(accent,.85F));
    float tx=a.x+20*s;
    if(key) {
      // Filled key: dark letter on the accent colour, pulsing gently.
      const float pulse=.8F+.2F*std::sin(game.elapsed()*6);
      hud->AddRectFilled({tx,a.y+6*s},{tx+30*s,b.y-6*s},withAlpha(accent,pulse),4*s);
      relay::drawTracked(hud,24*s,{tx+9*s,a.y+6*s},navy,key,0,true);
      tx+=keyWidth;
    }
    relay::drawTracked(hud,24*s,{tx,a.y+7.5F*s},withAlpha(IM_COL32(255,255,255,255),.6F),text,2*s,true);  // emboss
    relay::drawTracked(hud,24*s,{tx,a.y+6*s},graphite,text,2*s,true);
    relay::transformSince(hud,start,{xy.x,xy.y},.02F*std::sin(game.elapsed()*1.7F+xy.x*.013F),1);
  };
  if(!options.gallery&&game.state()==relay::RunState::Playing) {
    if(!game.practice()) label(game.gatePosition()+glm::vec3(0,3.7F,0),game.gateUnlocked()?"EXIT OPEN":"EXIT LOCKED",game.gateUnlocked()?mint:amber);
    if(game.tutorial()) {
      // The training objective, with its distance.
      const auto target=game.tutorialTarget();
      const float distance=glm::distance(glm::vec2(target.x,target.z),glm::vec2(game.player().position.x,game.player().position.z));
      char objective[64]; std::snprintf(objective,sizeof(objective),"%s  %dM",game.tutorialHint(),int(std::round(distance)));
      if(distance>2.5F) label(target+glm::vec3(0,3.5F,0),objective,mint);
    }
    for(std::size_t i=0;i<game.traps().size();++i) {
      const auto phase=game.trapPhase(i);
      const char* name=phase==relay::TrapPhase::Active?"DANGER":phase==relay::TrapPhase::Warning?"WARNING":phase==relay::TrapPhase::Disabled?"DISABLED":"SAFE";
      char text[64]; std::snprintf(text,sizeof(text),"%s  %.1f",name,game.trapPhaseRemaining(i));
      const ImU32 accent=phase==relay::TrapPhase::Active?danger:phase==relay::TrapPhase::Warning?amber:cyan;
      label((game.traps()[i].area.min+game.traps()[i].area.max)*.5F+glm::vec3(0,.45F,0),text,accent);
    }
    for(const auto& tile:game.drainTiles()) {
      const glm::vec3 c=(tile.area.min+tile.area.max)*.5F;
      if(glm::distance(c,game.player().position)<12) label(c+glm::vec3(0,.6F,0),"CRIMSON  -5S",IM_COL32(235,40,60,255));
    }
    const auto available=game.availableInteraction();
    const std::string interactKey=relay::KeyBindings::keyName(relay::keyBindings()[relay::KeyAction::Interact]);
    if(available==relay::Interaction::Core) label(game.cores()[std::size_t(game.nearestCore())].position+glm::vec3(0,.8F,0),"RECOVER CORE",mint,interactKey.c_str());
    if(available==relay::Interaction::Switch) label(game.switches()[std::size_t(game.nearestSwitch())].position+glm::vec3(0,1.8F,0),"DISABLE TRAP  5S",amber,interactKey.c_str());
    // Blast doors show their state nearby (and the hold countdown); plates announce themselves.
    for(std::size_t i=0;i<game.doors().size();++i) {
      const auto& door=game.doors()[i];
      if(glm::distance(door.centre,game.player().position)>12) continue;
      char text[32];
      if(door.hold>0) { std::snprintf(text,sizeof(text),"DOOR OPEN  %.1f",door.hold); label(door.centre+glm::vec3(0,1.5F,0),text,cyan); }
      else label(door.centre+glm::vec3(0,1.5F,0),door.open>.02F?"DOOR CLOSING":"DOOR SEALED",amber);
    }
    // Portals: sleeping ends name themselves nearby, an awake one waits for its pair, a linked pair
    // says so at both ends; the nearest sleeping end offers the E prompt.
    for(std::size_t i=0;i<game.portals().size();++i) for(int end=0;end<2;++end) {
      const auto& link=game.portals()[i]; const bool on=end==0?link.fromOn:link.toOn;
      const glm::vec3 at=(end==0?link.from:link.to)+glm::vec3(0,1.9F,0);
      if(glm::distance(at,game.player().position)>12) continue;
      if(available==relay::Interaction::Portal&&game.nearestPortal()==int(i)*2+end) label(at,"ACTIVATE PORTAL",amber,interactKey.c_str());
      else if(!on) label(at,"PORTAL",amber);
      else if(!link.linked()) label(at,"PORTAL  //  FIND ITS PAIR",IM_COL32(255,170,60,255));
      else label(at,"PORTAL  //  LINKED",glowCyan);
    }
    for(const auto& plate:game.plates())
      if(!plate.down&&glm::distance(plate.position,game.player().position)<9) label(plate.position+glm::vec3(0,.7F,0),"PRESSURE PLATE",amber);
    // Dormant checkpoints announce themselves nearby; the closest one offers the E prompt.
    for(std::size_t i=0;i<game.checkpoints().size();++i) {
      const auto& checkpoint=game.checkpoints()[i];
      const bool prompt=available==relay::Interaction::Checkpoint&&int(i)==game.nearestCheckpoint();
      if(prompt) label(checkpoint.position+glm::vec3(0,1.9F,0),"ACTIVATE CHECKPOINT",mint,interactKey.c_str());
      else if(!checkpoint.active&&glm::distance(checkpoint.position,game.player().position)<10) label(checkpoint.position+glm::vec3(0,1.9F,0),"CHECKPOINT",amber);
    }
  }
  if(game.state()!=relay::RunState::Playing&&!options.gallery&&!game.respawning()&&!game.outOfLives() && (game.state()==relay::RunState::Won||game.player().deathTime>1.1F)) {
    // Result: a full-width holographic band that wipes in after the celebration or shutdown.
    const bool won=game.state()==relay::RunState::Won;
    const float appear=std::clamp(won?game.player().motion.victoryAge/.6F-.5F:(game.player().deathTime-1.1F)/.4F,0.F,1.F);
    const ImU32 accent=won?mint:danger;
    const float by=display.y*.12F,bh=300*s,wipe=display.x*appear*appear*(3-2*appear);
    if(wipe>2) relay::drawAcrylicRect(hud,{0,by},{wipe,by+bh},0,1,surface);
    relay::drawScanlines(hud,{0,by},{wipe,by+bh},4*s,withAlpha(accent,.05F));
    hud->AddRectFilled({0,by},{wipe,by+3*s},accent);
    hud->AddRectFilled({0,by+bh-3*s},{wipe,by+bh},withAlpha(accent,.6F));
    const float x0=display.x*.5F-360*s;
    const char* title=won?(game.tutorial()?"TRAINING COMPLETE":"RELAY COMPLETE"):game.player().health<=0?"COURIER DESTROYED":"POWER DEPLETED";
    relay::drawTracked(hud,16*s,{x0,by+26*s},withAlpha(accent,appear),won?"STATION RESTORED  //  ALL CORES ONLINE":"SIGNAL LOST  //  UNIT 07 OFFLINE",3*s);
    // The title stamps down from twice its size once the band has wiped past the middle.
    const float age=(won?game.player().motion.victoryAge-.3F:game.player().deathTime-1.1F)-.3F;
    if(age>0) {
      const int start=hud->VtxBuffer.Size;
      const float tw=relay::drawGlow(hud,92*s,{x0-4*s,by+40*s},withAlpha(deepTeal,std::min(1.F,age/.06F)),glowCyan,title,4*s);
      relay::transformSince(hud,start,{x0+tw*.5F,by+92*s},-.1F*std::exp(-6.F*age)*std::cos(age*18),relay::punch(age,1.F,22,12));
    }
    char value[32]; float sx=x0; int statIndex=0;
    const auto stat=[&](const char* name,const char* v) {
      const float pop=age-.35F-.12F*float(statIndex++);
      const int start=hud->VtxBuffer.Size;
      relay::drawTracked(hud,13*s,{sx,by+150*s},withAlpha(graphiteSoft,appear),name,3*s);
      relay::drawTracked(hud,40*s,{sx,by+164*s},withAlpha(graphite,appear),v,2*s,true);
      relay::transformSince(hud,start,{sx+40*s,by+180*s},-.12F*std::exp(-7.F*std::max(0.F,pop))*std::cos(pop*18),relay::elasticIn(pop,.3F));
      sx+=200*s;
    };
    std::snprintf(value,sizeof(value),"%d / %d",game.collectedCount(),int(game.cores().size())); stat("CORES",value);
    if(game.timed()) { std::snprintf(value,sizeof(value),"%.1f S",game.timeRemaining()); stat("TIME LEFT",value); }
    else { std::snprintf(value,sizeof(value),"%.1f S",game.elapsed()); stat("TIME",value); }
    std::snprintf(value,sizeof(value),"%d / 5",game.player().health); stat("ARMOUR",value);
    ImGui::SetNextWindowPos({x0-8*s,by+226*s}); ImGui::SetNextWindowBgAlpha(0);
    ImGui::Begin("Result",nullptr,fixed|ImGuiWindowFlags_NoBackground); ImGui::PushFont(relay::displayFont(),28*s);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{24*s,8*s}); ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,2*s);
    ImGui::PushStyleColor(ImGuiCol_Button,{0,.88F,1,appear}); ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.55F,.96F,1,appear}); ImGui::PushStyleColor(ImGuiCol_ButtonActive,{1,.69F,.12F,appear});
    ImGui::PushStyleColor(ImGuiCol_Text,{.02F,.07F,.12F,appear});
    if(ImGui::Button("RESTART  [R]")) levelRequest_=game.level();
    ImGui::PopStyleColor(4); ImGui::SameLine(0,14*s);
    ImGui::PushStyleColor(ImGuiCol_Button,{0,.88F,1,.10F*appear}); ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,.88F,1,.25F*appear});
    ImGui::PushStyleColor(ImGuiCol_Text,{.25F,.27F,.31F,appear});
    // Missions stay locked: training and locked runs only offer a restart.
    if(relay::Game::missionsUnlocked&&!game.tutorial()&&ImGui::Button(game.level()==0?"NEXT LEVEL":"FIRST LEVEL")) levelRequest_=1-game.level();
    ImGui::PopStyleColor(3); ImGui::PopStyleVar(2); ImGui::PopFont(); ImGui::End();
  }
  // Training field manual: illustrated pages on armour, reboots and lives. It opens once, when
  // training reaches the checkpoint step or the courier first takes a hit.
  if(game.tutorial()&&!options.gallery&&!popupShown_&&game.state()==relay::RunState::Playing&&(game.tutorialStep()>=6||game.player().health<5)) { openPopup(0); popupOpenedAt_=float(ImGui::GetTime()); }
  if(popupPage_>=0) {
    using namespace relay::palette; using relay::withAlpha;
    auto* fg=ImGui::GetForegroundDrawList();
    const auto& io=ImGui::GetIO();
    constexpr int pages=3;
    struct Page { const char* title; const char* body; };
    const Page manual[pages]{
      {"ARMOUR","The courier carries five armour cells. Every hit from a live plate or a moving hazard breaks one; in missions each hit also costs five seconds. Broken cells only come back with a reboot."},
      {"SHUTDOWN AND REBOOT","When the last cell breaks the courier shuts down, then reboots at the last checkpoint you activated (or the entry point) with full armour. In missions a reboot costs ten seconds and one of your three lives."},
      {"OUT OF LIVES","Lose all three lives and the sector restarts from the beginning, cores and all. Running out of time also ends a mission. Training has no clock and unlimited lives, so experiment freely."}};
    const float w=820*s,h=560*s,x=(display.x-w)*.5F,y=(display.y-h)*.5F;
    fg->AddRectFilled({0,0},display,withAlpha(IM_COL32(20,24,30,255),.35F));
    const int popupStart=fg->VtxBuffer.Size;
    relay::drawPanel(fg,{x,y},{x+w,y+h},18*s,surface,mint,1.6F*s);
    relay::drawScanlines(fg,{x+4*s,y+4*s},{x+w-4*s,y+h-4*s},4*s,withAlpha(mint,.03F));
    char tag[48]; std::snprintf(tag,sizeof(tag),"FIELD MANUAL  //  %d / %d",popupPage_+1,pages);
    fg->AddRectFilled({x+30*s,y+28*s},{x+38*s,y+36*s},amber);
    relay::drawTracked(fg,14*s,{x+48*s,y+22*s},amber,tag,3*s);
    relay::drawGlow(fg,40*s,{x+28*s,y+42*s},deepTeal,glowCyan,manual[popupPage_].title,3*s);
    // Buttons: hover lights them; a click (or the keys below) acts.
    const auto button=[&](ImVec2 a,ImVec2 b,const char* label,bool primary) {
      const bool hover=io.MousePos.x>=a.x&&io.MousePos.x<=b.x&&io.MousePos.y>=a.y&&io.MousePos.y<=b.y;
      const ImU32 fill=primary?withAlpha(hover?ice:mint,.95F):withAlpha(mint,hover?.25F:.08F);
      relay::drawSlanted(fg,a,b,10*s,fill,fill);
      const float tw=relay::drawTracked(nullptr,22*s,{},0,label,3*s,true);
      relay::drawTracked(fg,22*s,{(a.x+b.x-tw)*.5F,a.y+8*s},primary?navy:graphite,label,3*s,true);
      return hover&&ImGui::IsMouseClicked(0);
    };
    // Close X in the corner.
    const ImVec2 xa(x+w-62*s,y+22*s),xb(x+w-26*s,y+58*s);
    const bool overClose=io.MousePos.x>=xa.x&&io.MousePos.x<=xb.x&&io.MousePos.y>=xa.y&&io.MousePos.y<=xb.y;
    fg->AddRect(xa,xb,withAlpha(mint,overClose?1.F:.5F),3*s,0,1.6F*s);
    fg->AddLine({xa.x+10*s,xa.y+10*s},{xb.x-10*s,xb.y-10*s},withAlpha(graphite,overClose?1.F:.7F),2.5F*s);
    fg->AddLine({xb.x-10*s,xa.y+10*s},{xa.x+10*s,xb.y-10*s},withAlpha(graphite,overClose?1.F:.7F),2.5F*s);
    // Illustration plate.
    const ImVec2 ia(x+30*s,y+104*s),ib(x+w-30*s,y+310*s);
    relay::drawAcrylicRect(fg,ia,ib,8*s,1,IM_COL32(205,210,217,255));
    const float time=float(ImGui::GetTime()),cy=(ia.y+ib.y)*.5F;
    const auto arrow=[&](float x0,float x1) {
      fg->AddLine({x0,cy},{x1-10*s,cy},withAlpha(steel,.9F),3*s);
      fg->AddTriangleFilled({x1,cy},{x1-14*s,cy-9*s},{x1-14*s,cy+9*s},withAlpha(steel,.9F));
    };
    const auto cells=[&](float x0,float yy,int full,int cracked) {
      for(int i=0;i<5;++i) {
        const ImVec2 a(x0+float(i)*34*s,yy-12*s),b(a.x+28*s,yy+12*s);
        const bool flash=i==cracked&&std::fmod(time,1.F)<.5F;
        const ImU32 c=i<full?cyan:i==cracked?withAlpha(danger,flash?1.F:.45F):withAlpha(steel,.25F);
        relay::drawSlanted(fg,a,b,7*s,c,c);
      }
    };
    const auto head=[&](ImVec2 c,float r,ImU32 colour,bool alive,float tilt=0) {
      fg->AddCircleFilled(c,r,withAlpha(colour,.15F),28); fg->AddCircle(c,r,colour,28,2.5F*s);
      for(float e:{-1.F,1.F}) {
        const ImVec2 eye(c.x+e*r*.4F*std::cos(tilt),c.y+e*r*.4F*std::sin(tilt));
        if(alive) fg->AddRectFilled({eye.x-r*.14F,eye.y-r*.28F},{eye.x+r*.14F,eye.y+r*.28F},colour,2*s);
        else { const float k=r*.17F; fg->AddLine({eye.x-k,eye.y-k},{eye.x+k,eye.y+k},colour,2.5F*s); fg->AddLine({eye.x+k,eye.y-k},{eye.x-k,eye.y+k},colour,2.5F*s); }
      }
    };
    const auto chip=[&](ImVec2 p,const char* label,ImU32 colour) {
      const float tw=relay::drawTracked(nullptr,18*s,{},0,label,2*s,true);
      fg->AddRectFilled({p.x,p.y},{p.x+tw+20*s,p.y+30*s},withAlpha(colour,.18F),4*s); fg->AddRect({p.x,p.y},{p.x+tw+20*s,p.y+30*s},colour,4*s,0,1.4F*s);
      relay::drawTracked(fg,18*s,{p.x+10*s,p.y+4*s},colour,label,2*s,true);
    };
    const auto bolt=[&](ImVec2 c,float r) {
      const ImVec2 z[6]{{c.x+r*.2F,c.y-r},{c.x-r*.45F,c.y+r*.1F},{c.x+r*.05F,c.y+r*.1F},{c.x-r*.25F,c.y+r},{c.x+r*.5F,c.y-r*.15F},{c.x,c.y-r*.15F}};
      fg->AddPolyline(z,6,amber,ImDrawFlags_Closed,3*s); fg->AddConvexPolyFilled(z,3,withAlpha(amber,.5F));
    };
    const auto block=[&](ImVec2 c,float r) {
      fg->AddRectFilled({c.x-r,c.y-r},{c.x+r,c.y+r},IM_COL32(28,32,36,255),4*s); fg->AddRect({c.x-r,c.y-r},{c.x+r,c.y+r},amber,4*s,0,2*s);
      fg->AddRectFilled({c.x-r*.7F,c.y-r*.15F},{c.x+r*.7F,c.y+r*.15F},withAlpha(danger,.85F));
    };
    const auto beacon=[&](ImVec2 c) {
      fg->AddEllipseFilled({c.x,c.y+50*s},{44*s,12*s},withAlpha(mint,.2F)); fg->AddEllipse({c.x,c.y+50*s},{44*s,12*s},mint,0,32,2*s);
      fg->AddRectFilled({c.x-6*s,c.y-24*s},{c.x+6*s,c.y+50*s},withAlpha(ice,.85F),3*s);
      fg->AddCircleFilled({c.x,c.y-34*s},13*s,mint); fg->AddCircleFilled({c.x,c.y-34*s},26*s,withAlpha(mint,.15F+.1F*std::sin(time*4)));
    };
    const auto restart=[&](ImVec2 c,float r) {
      fg->PathArcTo(c,r,.6F,5.6F,32); fg->PathStroke(cyan,0,4*s);
      const ImVec2 tip(c.x+r*std::cos(.6F),c.y+r*std::sin(.6F));
      fg->AddTriangleFilled({tip.x+10*s,tip.y-4*s},{tip.x-8*s,tip.y-8*s},{tip.x,tip.y+12*s},cyan);
    };
    const auto clockIcon=[&](ImVec2 c,float r,ImU32 colour) {
      fg->AddCircle(c,r,colour,28,2.5F*s); fg->AddLine(c,{c.x,c.y-r*.65F},colour,2.5F*s); fg->AddLine(c,{c.x+r*.5F,c.y},colour,2.5F*s);
    };
    const float l=ia.x,span=ib.x-ia.x;
    if(popupPage_==0) {
      // Hazards -> a cell breaks (-5 s in missions).
      bolt({l+span*.10F,cy-30*s},30*s); block({l+span*.10F,cy+40*s},26*s);
      arrow(l+span*.19F,l+span*.33F);
      cells(l+span*.37F,cy,4,4);
      chip({l+span*.37F+30*s,cy+30*s},"-1 CELL",danger);
      clockIcon({l+span*.82F,cy-10*s},26*s,amber); chip({l+span*.82F-34*s,cy+30*s},"-5 S",amber);
    } else if(popupPage_==1) {
      // Shutdown -> reboot at the beacon -> full armour (-10 s, -1 life in missions).
      head({l+span*.10F,cy},34*s,danger,false,.5F); cells(l+span*.03F,cy+60*s,0,-1);
      arrow(l+span*.19F,l+span*.30F);
      beacon({l+span*.39F,cy-6*s});
      arrow(l+span*.48F,l+span*.58F);
      cells(l+span*.61F,cy-20*s,5,-1);
      chip({l+span*.61F,cy+18*s},"-10 S",amber);
      for(int i=0;i<3;++i) head({l+span*.80F+float(i)*30*s,cy+33*s},11*s,i<2?cyan:withAlpha(steel,.4F),i<2);
    } else {
      // No lives left -> the sector restarts; training: unlimited.
      for(int i=0;i<3;++i) head({l+span*.06F+float(i)*40*s,cy-20*s},16*s,withAlpha(steel,.45F),false);
      clockIcon({l+span*.12F,cy+46*s},22*s,danger); chip({l+span*.12F+30*s,cy+32*s},"0:00",danger);
      arrow(l+span*.30F,l+span*.44F);
      restart({l+span*.53F,cy},40*s);
      ImVec2 path[48];
      for(int i=0;i<48;++i) { const float a=float(i)*6.2831853F/48,d=1+std::sin(a)*std::sin(a); path[i]={l+span*.82F+46*s*std::cos(a)/d,cy-10*s+46*s*std::sin(a)*std::cos(a)/d}; }
      fg->AddPolyline(path,48,mint,ImDrawFlags_Closed,4*s);
      chip({l+span*.82F-52*s,cy+36*s},"TRAINING",mint);
    }
    // Text, word-wrapped to the plate width.
    {
      std::vector<std::string> lines; std::string line,word; const float size=19*s,width=w-64*s;
      const auto flush=[&]{ if(word.empty()) return; const std::string next=line.empty()?word:line+" "+word;
        if(!line.empty()&&relay::drawTracked(nullptr,size,{},0,next.c_str(),1.5F*s)>width) { lines.push_back(line); line=word; } else line=next; word.clear(); };
      for(const char* c=manual[popupPage_].body;*c;++c) { if(*c==' ') flush(); else word+=*c; }
      flush(); if(!line.empty()) lines.push_back(line);
      float ty=ib.y+22*s;
      for(const auto& t:lines) { relay::drawTracked(fg,size,{x+32*s,ty},withAlpha(graphite,.92F),t.c_str(),1.5F*s); ty+=28*s; }
    }
    // Page dots, then the buttons.
    for(int i=0;i<pages;++i) fg->AddCircleFilled({x+40*s+float(i)*22*s,y+h-42*s},6*s,i==popupPage_?mint:withAlpha(steel,.4F),12);
    const bool last=popupPage_==pages-1;
    const bool next=button({x+w-230*s,y+h-66*s},{x+w-30*s,y+h-26*s},last?"GOT IT":"NEXT",true)||ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_Space,false)||ImGui::IsKeyPressed(ImGuiKey_RightArrow,false);
    const bool back=popupPage_>0&&(button({x+w-420*s,y+h-66*s},{x+w-250*s,y+h-26*s},"BACK",false)||ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false));
    {
      const float age=float(ImGui::GetTime())-popupOpenedAt_;
      if(age<.8F) relay::transformSince(fg,popupStart,{x+w*.5F,y+h*.5F},.05F*std::exp(-7.F*age)*std::sin(age*16),1-.2F*std::exp(-9.F*age)*std::cos(age*20));
    }
    // Ignore the press that opened it, so a held key or click cannot skip the first page.
    if(float(ImGui::GetTime())-popupOpenedAt_>.2F) {
      if((overClose&&ImGui::IsMouseClicked(0))||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||(next&&last)) popupPage_=-1;
      else if(next) ++popupPage_;
      else if(back) --popupPage_;
    }
  }
  bool startBenchmark=false;
  if(options.diagnostics||benchmark.running) {
    ImGui::SetNextWindowPos({display.x-20,20},ImGuiCond_FirstUseEver,{1,0}); ImGui::SetNextWindowSize({310,0});
    ImGui::Begin("Whitebox tools [F1]",nullptr,ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BeginDisabled(benchmark.running);
    if(ImGui::Button("Courtyard [1]")) levelRequest_=0;
    ImGui::SameLine(); if(ImGui::Button("Corridors [2]")) levelRequest_=1;
    ImGui::Checkbox("Motion workshop [F3]",&options.gallery); ImGui::Checkbox("Whitebox materials",&options.whitebox);
    ImGui::Checkbox("Overview camera [F2]",&options.overview);
    ImGui::Checkbox("Show collision shapes",&options.collisionOverlay);
    if(options.collisionOverlay) ImGui::TextWrapped("Blue: body. Yellow: feet. Orange/red: hazard geometry. Close F1 to play with this overlay.");
    ImGui::Checkbox("Remove internal voxel faces",&options.optimised); ImGui::Checkbox("Normal mapping",&options.normalMapping); ImGui::EndDisabled();
    const auto& s=renderer.stats(); ImGui::SeparatorText("Scene diagnostics");
    ImGui::Text("Voxels %d / 32768",s.voxels); ImGui::Text("Faces %d / %d",s.visibleFaces,s.totalFaces);
    ImGui::Text("Triangles %d | Draws %d",s.submittedTriangles,s.drawCalls); ImGui::Text("Particles %d / %d",game.liveParticleCount(),int(relay::Game::particleCapacity));
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
