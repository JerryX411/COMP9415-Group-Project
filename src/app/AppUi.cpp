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
  const auto body=craft::core::assetPath("fonts/Rajdhani-SemiBold.ttf");
  auto* fonts=ImGui::GetIO().Fonts; fonts->AddFontFromFileTTF(body.string().c_str(),22.F);
  // Display face: Agency FB Bold is read from the Windows font folder at run time
  // (not redistributed with the project); elsewhere the bundled Rajdhani is used.
  std::filesystem::path displayFace=body;
  if(const char* windows=std::getenv("WINDIR")) {
    const auto agency=std::filesystem::path(windows)/"Fonts"/"AGENCYB.TTF";
    std::error_code error; if(std::filesystem::exists(agency,error)) displayFace=agency;
  }
  relay::setDisplayFont(fonts->AddFontFromFileTTF(displayFace.string().c_str(),22.F));
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
AppUi::~AppUi() { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); }
void AppUi::beginFrame() {
  ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
  const auto size=ImGui::GetIO().DisplaySize;
  const float scale=std::min(size.x/1440.F,size.y/900.F);
  if(scale>0&&scale!=prewarmedScale_) { relay::prewarmFonts(scale); prewarmedScale_=scale; }
}
void AppUi::render() { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }
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
          hud->AddLine(p0,p1,withAlpha(i%4==0?IM_COL32(176,150,255,255):ice,(.12F+.22F*hash(deal+float(i)*9.1F))*rush),(1.F+2.F*hash(deal*2.3F+float(i)))*s);
        }
      }
      if(player.flightBlend>.01F) edges(IM_COL32(150,110,255,255),.16F*player.flightBlend,.13F);
    }
    if(player.motion.celebrateAge<.6F) edges(mint,.35F*(1-player.motion.celebrateAge/.6F),.16F);
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
    const ImVec2 pa(22*s,20*s),pb(pa.x+262*s,pa.y+98*s);
    // No backing or frame: the icons sit straight on the scene (with soft shadows).
    {
      // Armour: a small shield outline, then five slanted cells; the lost cell flashes red.
      const ImVec2 c(pa.x+34*s,pa.y+32*s);
      const ImVec2 outline[5]{{c.x-8*s,c.y-9*s},{c.x+8*s,c.y-9*s},{c.x+8*s,c.y+1*s},{c.x,c.y+10*s},{c.x-8*s,c.y+1*s}};
      hud->AddPolyline(outline,5,withAlpha(steel,.9F),ImDrawFlags_Closed,2*s);
      const float lostFlash=player.hurtCooldown>0?.5F+.5F*std::sin(player.hurtCooldown*28):0;
      for(int i=0;i<5;++i) {
        const ImVec2 a(pa.x+58*s+float(i)*38*s,pa.y+22*s),b(a.x+32*s,a.y+20*s);
        const bool full=i<player.health,justLost=i==player.health&&player.hurtCooldown>0;
        const ImU32 col=full?cyan:justLost?withAlpha(danger,lostFlash):withAlpha(steel,.22F);
        relay::drawSlanted(hud,{a.x+2*s,a.y+2*s},{b.x+2*s,b.y+2*s},7*s,withAlpha(navy,.45F),withAlpha(navy,.45F));
        relay::drawSlanted(hud,a,b,7*s,col,col);
      }
    }
    {
      // Lives as little courier heads (the one just lost flashes red; training shows a head and an
      // infinity sign), then the shield: a hex that lights while one is up and flashes as it breaks.
      const float ly=pa.y+70*s;
      if(game.tutorial()) { head({pa.x+34*s,ly},cyan,true); infinity({pa.x+72*s,ly},16*s,cyan,2.5F*s); }
      else {
        const bool losing=game.state()==relay::RunState::Lost&&player.health<=0;
        for(int i=0;i<relay::Game::livesPerLevel;++i) {
          const ImVec2 c(pa.x+34*s+float(i)*34*s,ly);
          const bool alive=i<game.lives(),justLost=losing&&i==game.lives();
          head(c,alive?cyan:justLost?withAlpha(danger,.5F+.5F*std::sin(player.deathTime*14)):withAlpha(steel,.22F),alive||justLost);
        }
      }
      hud->AddLine({pa.x+150*s,ly-12*s},{pa.x+150*s,ly+12*s},withAlpha(cyan,.25F),1.2F*s);
      const ImVec2 hc(pa.x+180*s,ly);
      const float broke=player.motion.shieldBreakAge<.5F?1-player.motion.shieldBreakAge/.5F:0;
      ImVec2 hex[6]; for(int k=0;k<6;++k) { const float a=float(k)*1.0472F; hex[k]={hc.x+std::cos(a)*12*s,hc.y+std::sin(a)*12*s}; }
      if(player.shielded) { hud->AddCircleFilled(hc,18*s,withAlpha(cyan,.18F),20); hud->AddConvexPolyFilled(hex,6,cyan); }
      else hud->AddPolyline(hex,6,broke>0?withAlpha(ice,broke):withAlpha(steel,.25F),ImDrawFlags_Closed,2*s);
      std::snprintf(text,sizeof(text),"%d",player.shielded?1:0);
      relay::drawTracked(hud,24*s,{hc.x+22*s,ly-13*s},player.shielded?ice:withAlpha(steel,.5F),text,1*s,true);
    }
    {
      // Cores, bottom centre: hexagonal sockets that fill and glow, and the count.
      const int cores=int(game.cores().size()),collected=game.collectedCount();
      std::snprintf(text,sizeof(text),"%d/%d",collected,cores);
      const float countWidth=relay::drawTracked(nullptr,30*s,{},0,text,2*s,true);
      const float w=36*s+float(cores)*40*s+countWidth+30*s;
      const ImVec2 ka((display.x-w)*.5F,display.y-92*s),kb((display.x+w)*.5F,display.y-30*s);
      const float fresh=player.motion.celebrateAge<.6F?1-player.motion.celebrateAge/.6F:0;
      for(int i=0;i<cores;++i) {
        const ImVec2 c(ka.x+36*s+float(i)*40*s,(ka.y+kb.y)*.5F);
        ImVec2 hex[6]; for(int k=0;k<6;++k) { const float a=float(k)*1.0472F+.5236F; hex[k]={c.x+std::cos(a)*14*s,c.y+std::sin(a)*14*s}; }
        if(i<collected) { hud->AddCircleFilled(c,22*s,withAlpha(mint,.16F+.3F*(i==collected-1?fresh:0)),24); hud->AddConvexPolyFilled(hex,6,mint); }
        else hud->AddPolyline(hex,6,withAlpha(steel,.9F),ImDrawFlags_Closed,2*s);
      }
      relay::drawTracked(hud,30*s,{ka.x+20*s+float(cores)*40*s,(ka.y+kb.y)*.5F-17*s},collected==cores?mint:ice,text,2*s,true);
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
    // Countdown: big glowing digits (an infinity sign in training); amber under 30 s, pulsing red
    // under 10 s.
    const float remaining=game.timeRemaining();
    std::snprintf(text,sizeof(text),"%d:%02d",int(std::ceil(remaining))/60,int(std::ceil(remaining))%60);
    ImU32 clock=!game.timed()?cyan:remaining<10?withAlpha(danger,.75F+.25F*std::sin(t*12)):remaining<30?amber:cyan;
    // A clock pickup: the digits flash gold and +30 S floats up beside them and fades.
    const float bonus=game.timed()&&game.timeBonusAge()<1.6F?game.timeBonusAge()/1.6F:-1;
    const ImU32 gold=IM_COL32(255,205,90,255);
    if(bonus>=0&&bonus<.45F&&std::fmod(bonus,.15F)<.09F) clock=gold;
    const float clockWidth=game.timed()?relay::drawTracked(nullptr,82*s,{},0,text,3*s,true):150*s;
    const ImVec2 ca(display.x-clockWidth-78*s,20*s),cb(display.x-22*s,118*s);
    relay::drawPanel(hud,ca,cb,16*s,withAlpha(navy,.78F),withAlpha(clock,.55F),1.4F*s);
    if(game.timed()) relay::drawGlow(hud,82*s,{cb.x-clockWidth-26*s,ca.y+8*s},clock,clock,text,3*s);
    else { infinity({cb.x-26*s-clockWidth*.5F,ca.y+49*s},58*s,withAlpha(clock,.35F),10*s); infinity({cb.x-26*s-clockWidth*.5F,ca.y+49*s},58*s,clock,4*s); }
    if(bonus>=0) {
      const float k=1-(1-bonus)*(1-bonus),fade=1-bonus*bonus;
      char plus[16]; std::snprintf(plus,sizeof(plus),"+%d S",int(relay::Game::timeBonus));
      const float w=relay::drawTracked(nullptr,40*s,{},0,plus,3*s,true);
      relay::drawGlow(hud,40*s,{cb.x-w-30*s,cb.y+6*s-26*s*k},withAlpha(gold,fade),withAlpha(gold,fade),plus,3*s);
    }
    const auto banner=[&](const char* message,ImU32 accent,float alpha,float by) {
      const float w=relay::drawTracked(nullptr,28*s,{},0,message,2.5F*s,true);
      const float slide=(1-alpha)*-20*s;
      const ImVec2 a((display.x-w)*.5F-34*s,by+slide),b((display.x+w)*.5F+26*s,by+slide+52*s);
      relay::drawPanel(hud,a,b,12*s,withAlpha(navy,.84F*alpha),withAlpha(accent,.7F*alpha),1.4F*s);
      hud->AddRectFilled({a.x+10*s,a.y+12*s},{a.x+16*s,b.y-12*s},withAlpha(accent,alpha));
      relay::drawTracked(hud,28*s,{a.x+30*s,a.y+10*s},withAlpha(ice,alpha),message,2.5F*s,true);
    };
    if(game.messageTime()>0&&game.state()==relay::RunState::Playing) banner(game.message().c_str(),cyan,std::min({1.F,game.messageTime()/.4F,(3.5F-game.messageTime())/.15F}),24*s);
    if(!focused) banner("PAUSED  //  CLICK TO RESUME",amber,1,display.y*.42F);
    if(game.respawning()) {
      // Reboot: a banner and progress bar after the fall, then a fade to black for the beam-in.
      const float since=player.deathTime,shown=std::clamp((since-.9F)/.4F,0.F,1.F),progress=std::clamp(since/relay::Game::respawnDelay,0.F,1.F);
      const float by=display.y*.30F;
      char reboot[96]; std::snprintf(reboot,sizeof(reboot),"%s  //  %d %s LEFT",game.activeCheckpoint()>=0?"REBOOTING AT CHECKPOINT":"REBOOTING AT ENTRY POINT",game.lives(),game.lives()==1?"LIFE":"LIVES");
      banner(reboot,cyan,shown,by);
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
      banner("ALL LIVES LOST  //  RESTARTING SECTOR",danger,shown,display.y*.30F);
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
      const char* rows[][2]{{move.c_str(),"MOVE"},{sprint.c_str(),"SPRINT"},{jump.c_str(),"JUMP"},{crouch.c_str(),"CROUCH/SLIDE"},{use.c_str(),"INTERACT"},{"MOUSE","LOOK"},{"ESC","PAUSE"}};
      // A column on the right edge, keycaps right-aligned, on a soft backing.
      float keyWidth=0,labelWidth=0;
      for(auto& r:rows) { keyWidth=std::max(keyWidth,relay::drawKeycap(nullptr,s,{},0,r[0])); labelWidth=std::max(labelWidth,relay::drawTracked(nullptr,16*s,{},0,r[1],2.5F*s)); }
      const float rowStep=40*s,columnWidth=labelWidth+14*s+keyWidth,right=display.x-30*s;
      const float top=display.y*.5F-float(std::size(rows))*rowStep*.5F;
      hud->AddRectFilled({right-columnWidth-22*s,top-16*s},{right+14*s,top+float(std::size(rows))*rowStep+4*s},withAlpha(navy,.5F*hint),10*s);
      for(std::size_t i=0;i<std::size(rows);++i) {
        const float y=top+float(i)*rowStep;
        const float kw=relay::drawKeycap(nullptr,s,{},0,rows[i][0]);
        relay::drawKeycap(hud,s,{right-kw,y},withAlpha(cyan,hint),rows[i][0]);
        const float lw=relay::drawTracked(nullptr,16*s,{},0,rows[i][1],2.5F*s);
        relay::drawTracked(hud,16*s,{right-keyWidth-14*s-lw,y+5*s},withAlpha(ice,.9F*hint),rows[i][1],2.5F*s);
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
  // World labels: small glass tags with a coloured stripe; prompts show the key to press.
  auto label=[&](glm::vec3 p,const char* text,ImU32 accent,const char* key=nullptr) {
    const auto xy=renderer.project(p,display.x,display.y); if(xy.x<-1000) return;
    if(xy.y<175*s) return;  // Never under the top HUD panels.
    const float keyWidth=key?30*s+10*s:0;
    const float w=relay::drawTracked(nullptr,24*s,{},0,text,2*s,true)+keyWidth;
    const ImVec2 a(xy.x-w*.5F-20*s,xy.y-21*s),b(xy.x+w*.5F+16*s,xy.y+21*s);
    relay::drawPanel(hud,a,b,9*s,withAlpha(navy,.82F),withAlpha(accent,.75F),1.3F*s);
    hud->AddRectFilled({a.x+7*s,a.y+9*s},{a.x+11*s,b.y-9*s},accent);
    float tx=a.x+20*s;
    if(key) {
      // Filled key: dark letter on the accent colour, pulsing gently.
      const float pulse=.8F+.2F*std::sin(game.elapsed()*6);
      hud->AddRectFilled({tx,a.y+6*s},{tx+30*s,b.y-6*s},withAlpha(accent,pulse),4*s);
      relay::drawTracked(hud,24*s,{tx+9*s,a.y+6*s},navy,key,0,true);
      tx+=keyWidth;
    }
    relay::drawTracked(hud,24*s,{tx,a.y+6*s},ice,text,2*s,true);
  };
  if(!options.gallery&&game.state()==relay::RunState::Playing) {
    label(game.gatePosition()+glm::vec3(0,3.7F,0),game.gateUnlocked()?"EXIT OPEN":"EXIT LOCKED",game.gateUnlocked()?mint:amber);
    if(game.tutorial()) {
      // The training objective, with its distance.
      const auto target=game.tutorialTarget();
      const float distance=glm::distance(glm::vec2(target.x,target.z),glm::vec2(game.player().position.x,game.player().position.z));
      char objective[40]; std::snprintf(objective,sizeof(objective),"OBJECTIVE  %dM",int(std::round(distance)));
      if(distance>2.5F) label(target+glm::vec3(0,3.5F,0),objective,mint);
    }
    for(std::size_t i=0;i<game.traps().size();++i) {
      const auto phase=game.trapPhase(i);
      const char* name=phase==relay::TrapPhase::Active?"DANGER":phase==relay::TrapPhase::Warning?"WARNING":phase==relay::TrapPhase::Disabled?"DISABLED":"SAFE";
      char text[64]; std::snprintf(text,sizeof(text),"%s  %.1f",name,game.trapPhaseRemaining(i));
      const ImU32 accent=phase==relay::TrapPhase::Active?danger:phase==relay::TrapPhase::Warning?amber:cyan;
      label((game.traps()[i].area.min+game.traps()[i].area.max)*.5F+glm::vec3(0,.45F,0),text,accent);
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
    hud->AddRectFilled({0,by},{wipe,by+bh},withAlpha(navy,.88F));
    relay::drawScanlines(hud,{0,by},{wipe,by+bh},4*s,withAlpha(accent,.05F));
    hud->AddRectFilled({0,by},{wipe,by+3*s},accent);
    hud->AddRectFilled({0,by+bh-3*s},{wipe,by+bh},withAlpha(accent,.6F));
    const float x0=display.x*.5F-360*s;
    const char* title=won?(game.tutorial()?"TRAINING COMPLETE":"RELAY COMPLETE"):game.player().health<=0?"COURIER DESTROYED":"POWER DEPLETED";
    relay::drawTracked(hud,16*s,{x0,by+26*s},withAlpha(accent,appear),won?"STATION RESTORED  //  ALL CORES ONLINE":"SIGNAL LOST  //  UNIT 07 OFFLINE",3*s);
    relay::drawGlow(hud,92*s,{x0-4*s,by+40*s},withAlpha(ice,appear),accent,title,4*s);
    char value[32]; float sx=x0;
    const auto stat=[&](const char* name,const char* v) {
      relay::drawTracked(hud,13*s,{sx,by+150*s},withAlpha(steel,appear),name,3*s);
      relay::drawTracked(hud,40*s,{sx,by+164*s},withAlpha(ice,appear),v,2*s,true);
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
    ImGui::PushStyleColor(ImGuiCol_Text,{.84F,.97F,1,appear});
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
    fg->AddRectFilled({0,0},display,withAlpha(navy,.62F));
    relay::drawPanel(fg,{x,y},{x+w,y+h},18*s,withAlpha(navy,.97F),withAlpha(mint,.75F),1.6F*s);
    relay::drawScanlines(fg,{x+4*s,y+4*s},{x+w-4*s,y+h-4*s},4*s,withAlpha(mint,.03F));
    char tag[48]; std::snprintf(tag,sizeof(tag),"FIELD MANUAL  //  %d / %d",popupPage_+1,pages);
    fg->AddRectFilled({x+30*s,y+28*s},{x+38*s,y+36*s},amber);
    relay::drawTracked(fg,14*s,{x+48*s,y+22*s},amber,tag,3*s);
    relay::drawGlow(fg,40*s,{x+28*s,y+42*s},ice,mint,manual[popupPage_].title,3*s);
    // Buttons: hover lights them; a click (or the keys below) acts.
    const auto button=[&](ImVec2 a,ImVec2 b,const char* label,bool primary) {
      const bool hover=io.MousePos.x>=a.x&&io.MousePos.x<=b.x&&io.MousePos.y>=a.y&&io.MousePos.y<=b.y;
      const ImU32 fill=primary?withAlpha(hover?ice:mint,.95F):withAlpha(mint,hover?.25F:.08F);
      relay::drawSlanted(fg,a,b,10*s,fill,fill);
      const float tw=relay::drawTracked(nullptr,22*s,{},0,label,3*s,true);
      relay::drawTracked(fg,22*s,{(a.x+b.x-tw)*.5F,a.y+8*s},primary?navy:ice,label,3*s,true);
      return hover&&ImGui::IsMouseClicked(0);
    };
    // Close X in the corner.
    const ImVec2 xa(x+w-62*s,y+22*s),xb(x+w-26*s,y+58*s);
    const bool overClose=io.MousePos.x>=xa.x&&io.MousePos.x<=xb.x&&io.MousePos.y>=xa.y&&io.MousePos.y<=xb.y;
    fg->AddRect(xa,xb,withAlpha(mint,overClose?1.F:.5F),3*s,0,1.6F*s);
    fg->AddLine({xa.x+10*s,xa.y+10*s},{xb.x-10*s,xb.y-10*s},withAlpha(ice,overClose?1.F:.7F),2.5F*s);
    fg->AddLine({xb.x-10*s,xa.y+10*s},{xa.x+10*s,xb.y-10*s},withAlpha(ice,overClose?1.F:.7F),2.5F*s);
    // Illustration plate.
    const ImVec2 ia(x+30*s,y+104*s),ib(x+w-30*s,y+310*s);
    fg->AddRectFilled(ia,ib,withAlpha(IM_COL32(4,14,24,255),.9F),8*s);
    fg->AddRect(ia,ib,withAlpha(mint,.25F),8*s,0,1.2F*s);
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
      for(const auto& t:lines) { relay::drawTracked(fg,size,{x+32*s,ty},withAlpha(ice,.92F),t.c_str(),1.5F*s); ty+=28*s; }
    }
    // Page dots, then the buttons.
    for(int i=0;i<pages;++i) fg->AddCircleFilled({x+40*s+float(i)*22*s,y+h-42*s},6*s,i==popupPage_?mint:withAlpha(steel,.4F),12);
    const bool last=popupPage_==pages-1;
    const bool next=button({x+w-230*s,y+h-66*s},{x+w-30*s,y+h-26*s},last?"GOT IT":"NEXT",true)||ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_Space,false)||ImGui::IsKeyPressed(ImGuiKey_RightArrow,false);
    const bool back=popupPage_>0&&(button({x+w-420*s,y+h-66*s},{x+w-250*s,y+h-26*s},"BACK",false)||ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false));
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
