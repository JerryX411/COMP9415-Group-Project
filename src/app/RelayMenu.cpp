#include "app/RelayMenu.h"
#include "core/RelayGame.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
namespace relay {
namespace {
using namespace palette;
float smooth(float x) { x=std::clamp(x,0.F,1.F); return x*x*(3-2*x); }
ImFont* display=nullptr;
struct Item { const char* label; MenuAction action; };
struct MenuState {
  int selected=0; MenuMode mode=MenuMode::Title; float lockedAt=-10;
  float lastDraw=-10, shownAt=0;
  float highlight[5]{};
  ImVec2 mouse{-1,-1};
} menu;
const char* levelNames[]{"01  COURTYARD","02  CORRIDORS"};
}

ImU32 withAlpha(ImU32 c,float a) { return (c&~IM_COL32_A_MASK)|(ImU32(float((c>>IM_COL32_A_SHIFT)&0xFF)*std::clamp(a,0.F,1.F))<<IM_COL32_A_SHIFT); }
void setDisplayFont(ImFont* font) { display=font; }
ImFont* displayFont() { return display?display:ImGui::GetFont(); }
void prewarmFonts(float s) {
  // Every size the menus and HUD use (times the layout scale); ImGui bakes sizes lazily otherwise.
  constexpr float sizes[]{11,12,13,14,15,16,17,18,19,20,21,22,24,25,26,28,30,32,34,38,40,44,46,48,50,58,78,82,92,94,104,112,132};
  for(ImFont* font:{ImGui::GetFont(),displayFont()}) for(float size:sizes) {
    ImFontBaked* baked=font->GetFontBaked(size*s);
    for(ImWchar c=32;c<127;++c) baked->FindGlyph(c);
  }
}

// Body text is Rajdhani SemiBold, thickened with a second offset pass; the
// display face is already bold and only gets a hairline of extra weight.
float drawTracked(ImDrawList* d,float size,ImVec2 p,ImU32 c,const char* text,float tracking,bool useDisplay) {
  auto* font=useDisplay?displayFont():ImGui::GetFont(); float x=p.x;
  const float weight=std::max(.6F,size*(useDisplay?.012F:.035F));
  for(const char* q=text;*q;++q) {
    const char glyph[2]{*q,0};
    if(d) { d->AddText(font,size,{x,p.y},c,glyph); d->AddText(font,size,{x+weight,p.y},c,glyph); }
    x+=font->CalcTextSizeA(size,FLT_MAX,0,glyph).x+weight+tracking;
  }
  return x-p.x-tracking;
}
float drawGlow(ImDrawList* d,float size,ImVec2 p,ImU32 c,ImU32 glow,const char* text,float tracking) {
  if(d) {
    // Two rings of faint copies read as a soft bloom around the letters.
    const float a=float((c>>IM_COL32_A_SHIFT)&0xFF)/255.F;
    for(float radius:{size*.055F,size*.028F}) for(int i=0;i<8;++i) {
      const float t=float(i)*.785398F;
      drawTracked(d,size,{p.x+std::cos(t)*radius,p.y+std::sin(t)*radius},withAlpha(glow,a*(radius>size*.04F?.07F:.13F)),text,tracking,true);
    }
  }
  return drawTracked(d,size,p,c,text,tracking,true);
}
float drawKeycap(ImDrawList* d,float s,ImVec2 p,ImU32 c,const char* key) {
  const float w=drawTracked(nullptr,16*s,{},c,key,1.5F*s)+18*s;
  if(d) {
    d->AddRectFilled(p,{p.x+w,p.y+29*s},withAlpha(navy,.55F*float((c>>IM_COL32_A_SHIFT)&0xFF)/255.F),3*s);
    d->AddRect(p,{p.x+w,p.y+29*s},c,3*s,0,1.6F*s);
    d->AddLine({p.x+3*s,p.y+29*s},{p.x+w-3*s,p.y+29*s},c,2.5F*s);
    drawTracked(d,16*s,{p.x+9*s,p.y+5*s},c,key,1.5F*s);
  }
  return w;
}
void drawPanel(ImDrawList* d,ImVec2 a,ImVec2 b,float cut,ImU32 fill,ImU32 edge,float thickness) {
  const ImVec2 points[6]{{a.x+cut,a.y},{b.x,a.y},{b.x,b.y-cut},{b.x-cut,b.y},{a.x,b.y},{a.x,a.y+cut}};
  d->AddConvexPolyFilled(points,6,fill);
  if(thickness>0) d->AddPolyline(points,6,edge,ImDrawFlags_Closed,thickness);
}
void drawBrackets(ImDrawList* d,ImVec2 a,ImVec2 b,float l,ImU32 c,float t) {
  for(int corner=0;corner<4;++corner) {
    const ImVec2 p(corner&1?b.x:a.x,corner&2?b.y:a.y);
    const float sx=corner&1?-1.F:1.F,sy=corner&2?-1.F:1.F;
    d->AddLine(p,{p.x+sx*l,p.y},c,t); d->AddLine(p,{p.x,p.y+sy*l},c,t);
  }
}
void drawScanlines(ImDrawList* d,ImVec2 a,ImVec2 b,float spacing,ImU32 c) {
  for(float y=a.y;y<b.y;y+=spacing) d->AddLine({a.x,y},{b.x,y},c,1);
}
void drawSlanted(ImDrawList* d,ImVec2 a,ImVec2 b,float slant,ImU32 left,ImU32 right) {
  // Horizontal gradient built from slices, since draw lists only blend rectangles.
  constexpr int slices=12;
  const auto mix=[&](float t) {
    ImU32 out=0;
    for(int shift:{IM_COL32_R_SHIFT,IM_COL32_G_SHIFT,IM_COL32_B_SHIFT,IM_COL32_A_SHIFT}) {
      const float l=float((left>>shift)&0xFF),r=float((right>>shift)&0xFF);
      out|=ImU32(l+(r-l)*t)<<shift;
    }
    return out;
  };
  for(int i=0;i<slices;++i) {
    const float t0=float(i)/slices,t1=float(i+1)/slices,x0=a.x+(b.x-a.x)*t0,x1=a.x+(b.x-a.x)*t1;
    const ImVec2 q[4]{{x0+slant,a.y},{x1+slant,a.y},{x1,b.y},{x0,b.y}};
    d->AddConvexPolyFilled(q,4,mix((t0+t1)*.5F));
  }
}

// Glass control slab with a slanted, glowing right edge (title, pause and settings).
void drawSlab(ImDrawList* d,ImVec2 size,float s,float time,float intro,bool dim) {
  if(dim) d->AddRectFilled({0,0},size,withAlpha(navy,.55F));
  const float panel=std::max(.44F*size.x,660*s)*(.6F+.4F*intro),lean=130*s;
  {
    const ImVec2 slab[4]{{0,0},{panel,0},{panel-lean,size.y},{0,size.y}};
    d->AddConvexPolyFilled(slab,4,withAlpha(navy,.86F));
    drawScanlines(d,{0,0},{panel-lean,size.y},4*s,withAlpha(cyan,.035F));
    const float sweep=std::fmod(time*140*s,size.y+200*s)-100*s;
    d->AddRectFilledMultiColor({0,sweep},{panel-lean*(sweep/size.y),sweep+70*s},withAlpha(cyan,0),withAlpha(cyan,0),withAlpha(cyan,.06F),withAlpha(cyan,.06F));
    d->AddLine({panel,0},{panel-lean,size.y},withAlpha(cyan,.18F),9*s);
    d->AddLine({panel,0},{panel-lean,size.y},withAlpha(cyan,.85F),2*s);
  }

}

MenuAction drawMenu(MenuMode mode,int& level,float time,bool hasCheckpoint) {
  const bool paused=mode==MenuMode::Pause;
  const auto& io=ImGui::GetIO(); const auto size=io.DisplaySize;
  auto* d=ImGui::GetBackgroundDrawList();
  const float s=std::min(size.x/1440.F,size.y/900.F),left=std::max(60*s,size.x*.06F);
  // Re-run the entrance whenever the menu reappears or switches mode.
  if(time-menu.lastDraw>.25F||mode!=menu.mode) { menu.shownAt=time; menu.selected=0; menu.mode=mode; std::fill(std::begin(menu.highlight),std::end(menu.highlight),0.F); }
  const float dt=std::clamp(time-menu.lastDraw,0.F,.1F); menu.lastDraw=time;
  const float shown=time-menu.shownAt,intro=smooth(shown/.55F);
  const Item pauseItems[5]{{"RESUME",MenuAction::Resume},{hasCheckpoint?"RETURN TO CHECKPOINT":"RETURN TO START",MenuAction::Checkpoint},
    {"RESTART LEVEL",MenuAction::Restart},{"SETTINGS",MenuAction::Settings},{"MAIN MENU",MenuAction::Title}};
  // Training replaces START while the missions are locked.
  const bool locked=!relay::Game::missionsUnlocked;
  const Item titleItems[5]{{locked?"TRAINING":"START",MenuAction::Start},{"LEVEL",MenuAction::None},{"MOTION LAB",MenuAction::Gallery},
    {"SETTINGS",MenuAction::Settings},{"QUIT",MenuAction::Quit}};
  const Item* items=paused?pauseItems:titleItems;
  constexpr int count=5;

  drawSlab(d,size,s,time,intro,paused);

  // Header strip (pause only; the title screen keeps just the wordmark): blinking status lamp
  // and session code.
  const float top=size.y*.10F;
  if(paused) {
    d->AddRectFilled({left,top},{left+9*s,top+9*s},withAlpha(amber,(std::fmod(time,1.2F)<.8F?1.F:.25F)*intro));
    drawTracked(d,15*s,{left+20*s,top-4*s},withAlpha(steel,intro),"RELAY RESEARCH  //  SESSION HELD",3*s);
  }
  float y=top+30*s;
  if(paused) {
    drawGlow(d,112*s,{left-4*s,y+14*s*(1-intro)},withAlpha(cyan,intro),cyan,"PAUSED",6*s);
    y+=118*s;
  } else {
    // Staggered two-line wordmark: white VOXEL, glowing cyan RELAY nudged right.
    drawGlow(d,132*s,{left-6*s,y+16*s*(1-intro)},withAlpha(ice,intro),cyan,"VOXEL",8*s);
    drawGlow(d,132*s,{left+46*s-30*s*(1-intro),y+112*s},withAlpha(cyan,intro),cyan,"RELAY",8*s);
    y+=262*s;
  }
  for(int i=0;i<14;++i) {
    const bool lit=std::fmod(time*6.F-float(i)*.5F,10.F)<4.F;
    drawSlanted(d,{left+float(i)*20*s,y},{left+float(i)*20*s+13*s,y+7*s},4*s,withAlpha(lit?cyan:steel,(lit?.9F:.3F)*intro),withAlpha(lit?cyan:steel,(lit?.9F:.3F)*intro));
  }
  if(paused) drawTracked(d,19*s,{left,y+20*s},withAlpha(steel,intro),"COURIER STANDING BY",2.5F*s);

  // Navigation: keyboard first; the mouse only takes over when it moves.
  if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)||ImGui::IsKeyPressed(ImGuiKey_W)) menu.selected=(menu.selected+count-1)%count;
  if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)||ImGui::IsKeyPressed(ImGuiKey_S)) menu.selected=(menu.selected+1)%count;
  if(!paused&&!locked&&(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)||ImGui::IsKeyPressed(ImGuiKey_A)||ImGui::IsKeyPressed(ImGuiKey_RightArrow)||ImGui::IsKeyPressed(ImGuiKey_D))) level=1-level;
  const bool mouseMoved=std::abs(io.MousePos.x-menu.mouse.x)+std::abs(io.MousePos.y-menu.mouse.y)>.5F; menu.mouse=io.MousePos;
  int activated=-1;
  const float firstItem=y+62*s,step=62*s,barWidth=470*s;
  for(int i=0;i<count;++i) {
    const float iy=firstItem+float(i)*step;
    const ImVec2 a(left-20*s,iy-8*s),b(left+barWidth,iy+58*s);
    const bool hover=io.MousePos.x>=a.x&&io.MousePos.x<=b.x&&io.MousePos.y>=a.y&&io.MousePos.y<=b.y;
    if(hover&&mouseMoved) menu.selected=i;
    if(hover&&ImGui::IsMouseClicked(0)) activated=i;
    auto& h=menu.highlight[i]; h+=((menu.selected==i?1.F:0.F)-h)*(1-std::exp(-18*dt));
    const float appear=smooth((shown-.15F-.07F*float(i))/.35F),x=left-40*s*(1-appear);
    // Selection: a cyan blade sweeps out from the left and inverts the label.
    if(h>.01F) {
      drawSlanted(d,{x-14*s,iy},{x-14*s+(barWidth+10*s)*smooth(h),iy+52*s},14*s,withAlpha(cyan,.95F*appear*h),withAlpha(cyan,.05F*appear*h));
      d->AddRectFilled({x-24*s,iy},{x-19*s,iy+52*s},withAlpha(amber,appear*h));
    }
    const ImU32 ink=h>.5F?withAlpha(navy,appear):withAlpha(ice,appear*.82F);
    // Pause items carry an 01-05 index; the title screen shows the labels alone.
    if(paused) {
      char index[4]; std::snprintf(index,sizeof(index),"%02d",i+1);
      drawTracked(d,16*s,{x,iy+18*s},h>.5F?withAlpha(navy,appear*.8F):withAlpha(amber,appear*.9F),index,1*s);
    }
    drawTracked(d,48*s,{x+(paused?38.F:8.F)*s+12*s*h,iy-1*s},ink,items[i].label,4*s,true);
    if(!paused&&i==1) {
      // Level selector: value on the right of the blade, arrows when focused.
      const float vx=x+230*s;
      if(locked) {
        // Missions locked: a padlock and LOCKED in place of the level, flashing when the player tries to enter.
        const float flash=std::clamp(1-(time-menu.lockedAt)/.6F,0.F,1.F),pulse=flash>0?.5F+.5F*std::sin(time*30):1.F;
        const ImU32 tag=withAlpha(h>.5F?navy:amber,appear*(.6F+.4F*pulse));
        d->AddRectFilled({vx,iy+20*s},{vx+20*s,iy+38*s},tag,2*s);
        d->PathArcTo({vx+10*s,iy+20*s},7*s,3.14159F,6.28318F,12); d->PathStroke(tag,0,3*s);
        drawTracked(d,34*s,{vx+32*s,iy+7*s},tag,"LOCKED",3*s,true);
      } else {
        drawTracked(d,34*s,{vx,iy+7*s},h>.5F?withAlpha(navy,appear):withAlpha(cyan,appear*.9F),levelNames[level],2*s,true);
        const ImU32 arrow=withAlpha(h>.5F?navy:cyan,appear*(.4F+.6F*h));
        d->AddTriangleFilled({vx-24*s,iy+26*s},{vx-12*s,iy+18*s},{vx-12*s,iy+34*s},arrow);
        const float end=vx+drawTracked(nullptr,34*s,{},0,levelNames[level],2*s,true)+12*s;
        d->AddTriangleFilled({end+12*s,iy+26*s},{end,iy+18*s},{end,iy+34*s},arrow);
      }
    }
  }
  if(ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false)) activated=menu.selected;
  MenuAction action=MenuAction::None;
  if(activated>=0) {
    if(!paused&&activated==1) { if(locked) menu.lockedAt=time; else level=1-level; }
    else action=items[activated].action;
  }

  // Key hints along the bottom of the slab.
  const float hy=size.y-76*s,hint=smooth((shown-.5F)/.4F);
  const ImU32 hk=withAlpha(cyan,hint*.85F),ht=withAlpha(steel,hint);
  float hx=left;
  // The title screen drops the select/confirm hints; only the level hint shows there.
  if(paused) {
    hx+=drawKeycap(d,s,{hx,hy},hk,"W / S")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"SELECT",2.5F*s)+28*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ENTER")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"CONFIRM",2.5F*s)+28*s;
  }
  if(!paused&&menu.selected==1) {
    if(locked) drawTracked(d,16*s,{hx,hy+5*s},withAlpha(amber,hint),"MISSIONS LOCKED  //  START WITH TRAINING",2.5F*s);
    else { hx+=drawKeycap(d,s,{hx,hy},hk,"A / D")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"LEVEL",2.5F*s); }
  }
  if(paused) { hx+=drawKeycap(d,s,{hx,hy},hk,"ESC")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"RESUME",2.5F*s); }

  if(!paused) {
    // Target reticle around the courier on its plinth, with a data tag.
    const ImVec2 c(size.x*.60F,size.y*.47F); const float r=size.y*.27F*(.85F+.15F*intro),spin=time*.35F;
    const ImU32 line=withAlpha(deepCyan,.75F*intro);
    for(int i=0;i<3;++i) { d->PathArcTo(c,r,spin+float(i)*2.094F,spin+float(i)*2.094F+1.35F,32); d->PathStroke(line,0,2.5F*s); }
    d->AddCircle(c,r*1.08F,withAlpha(deepCyan,.25F*intro),96,1.2F*s);
    for(int i=0;i<48;++i) {
      const float t=float(i)*.1309F-spin*.5F,in=i%6==0?.93F:.97F;
      d->AddLine({c.x+std::cos(t)*r*in,c.y+std::sin(t)*r*in},{c.x+std::cos(t)*r,c.y+std::sin(t)*r},withAlpha(deepCyan,.5F*intro),1.5F*s);
    }
    const ImVec2 tag(c.x+r*.80F,c.y-r*.95F);
    d->AddLine({c.x+r*.62F,c.y-r*.62F},{tag.x,tag.y+52*s},withAlpha(deepCyan,.8F*intro),1.5F*s);
    d->AddLine({tag.x,tag.y+52*s},{tag.x+210*s,tag.y+52*s},withAlpha(deepCyan,.8F*intro),1.5F*s);
    drawPanel(d,{tag.x,tag.y-6*s},{tag.x+210*s,tag.y+48*s},10*s,withAlpha(navy,.82F*intro),withAlpha(cyan,.6F*intro),1.2F*s);
    drawTracked(d,30*s,{tag.x+12*s,tag.y-2*s},withAlpha(ice,intro),"UNIT 07",3*s,true);
    char sync[32]; std::snprintf(sync,sizeof(sync),"ONLINE  //  SYNC %02d%%",96+int(std::fmod(time*2.F,4.F)));
    drawTracked(d,13*s,{tag.x+12*s,tag.y+28*s},withAlpha(amber,intro),sync,2*s);
    drawTracked(d,14*s,{size.x-160*s,size.y-70*s},withAlpha(deepCyan,hint*.8F),"BUILD 0.7",3*s);
  }
  return action;
}

void drawArrival(float time) {
  const auto size=ImGui::GetIO().DisplaySize; auto* d=ImGui::GetForegroundDrawList();
  const float s=std::min(size.x/1440.F,size.y/900.F);
  // Letterbox slides in, captions type out per beat, then everything clears for play.
  const float bars=std::min(smooth(time/.35F),smooth((4.F-time)/.45F)),h=78*s*bars;
  if(time<.55F) d->AddRectFilled({0,0},size,withAlpha(navy,.92F*(1-time/.55F)));
  d->AddRectFilled({0,0},{size.x,h},withAlpha(navy,.97F));
  d->AddRectFilled({0,size.y-h},size,withAlpha(navy,.97F));
  drawScanlines(d,{0,size.y-h},size,4*s,withAlpha(cyan,.04F*bars));
  d->AddLine({0,h},{size.x,h},withAlpha(cyan,.35F*bars),1.5F*s);
  d->AddLine({0,size.y-h},{size.x,size.y-h},withAlpha(cyan,.35F*bars),1.5F*s);
  for(int i=0;i<40;++i) {
    const bool done=float(i)/40.F<time/4.F;
    drawSlanted(d,{size.x*.5F-400*s+float(i)*20*s,h-14*s},{size.x*.5F-400*s+float(i)*20*s+14*s,h-8*s},3*s,withAlpha(done?cyan:steel,(done?.9F:.25F)*bars),withAlpha(done?cyan:steel,(done?.9F:.25F)*bars));
  }
  struct Beat { float at; const char* text; };
  constexpr Beat beats[]{{0,"POWER RESTORED"},{.65F,"OPTICS ONLINE"},{1.35F,"MOTOR CALIBRATION"},{2.25F,"SHAKING OFF THE DUST"},{3.F,"UNIT 07  READY"}};
  const Beat* beat=&beats[0]; for(const auto& b:beats) if(time>=b.at) beat=&b;
  const std::size_t count=std::min<std::size_t>(31,std::size_t(std::max(0.F,(time-beat->at)*40)));
  char typed[32]{}; std::strncpy(typed,beat->text,count);
  const float width=drawTracked(nullptr,46*s,{},0,beat->text,6*s,true);
  const ImVec2 at((size.x-width)*.5F,size.y-h*.5F-30*s);
  const float typedWidth=drawGlow(d,46*s,at,withAlpha(ice,bars),cyan,typed,6*s);
  if(std::fmod(time,.5F)<.3F) d->AddRectFilled({at.x+typedWidth+8*s,at.y+10*s},{at.x+typedWidth+22*s,at.y+48*s},withAlpha(cyan,bars));
  d->AddRectFilled({40*s,h*.5F-13*s},{48*s,h*.5F-5*s},withAlpha(amber,bars));
  drawTracked(d,16*s,{58*s,h*.5F-18*s},withAlpha(amber,bars),"REACTIVATION  //  UNIT 07",3*s);
  const float skip=drawKeycap(nullptr,s,{},0,"ENTER");
  drawKeycap(d,s,{size.x-skip-110*s,h*.5F-22*s},withAlpha(cyan,bars*.85F),"ENTER");
  drawTracked(d,16*s,{size.x-98*s,h*.5F-17*s},withAlpha(steel,bars),"SKIP",3*s);
}

void drawLevelTransition(float black,float card,float progress,int level,const char* name,float time) {
  const auto size=ImGui::GetIO().DisplaySize; auto* d=ImGui::GetForegroundDrawList();
  const float s=std::min(size.x/1440.F,size.y/900.F);
  d->AddRectFilled({0,0},size,IM_COL32(2,7,14,int(255*std::clamp(black,0.F,1.F))));
  if(card<=0) return;
  // Sector card: code, glowing name, a rule that draws outward and a load bar.
  const float rise=(1-smooth(card))*18*s,cy=size.y*.44F;
  drawScanlines(d,{0,cy-150*s},{size.x,cy+150*s},4*s,withAlpha(cyan,.03F*card));
  char code[32]; std::snprintf(code,sizeof(code),level==relay::Game::tutorialLevel?"TRAINING  //  00":"SECTOR  %02d",level+1);
  const float codeWidth=drawTracked(nullptr,20*s,{},0,code,6*s);
  d->AddRectFilled({(size.x-codeWidth)*.5F-22*s,cy-96*s+rise},{(size.x-codeWidth)*.5F-12*s,cy-86*s+rise},withAlpha(amber,card*(std::fmod(time,.8F)<.5F?1.F:.3F)));
  drawTracked(d,20*s,{(size.x-codeWidth)*.5F,cy-102*s+rise},withAlpha(amber,card),code,6*s);
  // The name types out over the first half of the hold behind a block cursor.
  const float nameWidth=drawTracked(nullptr,104*s,{},0,name,8*s,true);
  char typed[40]{}; const std::size_t length=std::strlen(name),shown=std::min(length,std::size_t(float(length)*std::clamp((progress-.2F)/.45F,0.F,1.F)+.999F));
  std::strncpy(typed,name,std::min<std::size_t>(shown,sizeof(typed)-1));
  const float typedWidth=drawGlow(d,104*s,{(size.x-nameWidth)*.5F,cy-74*s+rise},withAlpha(ice,card),cyan,typed,8*s);
  if(shown<length||std::fmod(time,.6F)<.35F) d->AddRectFilled({(size.x-nameWidth)*.5F+typedWidth+10*s,cy-56*s+rise},{(size.x-nameWidth)*.5F+typedWidth+26*s,cy+12*s+rise},withAlpha(cyan,card));
  const float rule=(nameWidth*.5F+60*s)*smooth(card);
  d->AddRectFilled({size.x*.5F-rule,cy+52*s},{size.x*.5F+rule,cy+54*s},withAlpha(cyan,.8F*card));
  for(int i=0;i<24;++i) {
    const bool done=float(i)/24.F<progress;
    const float x=size.x*.5F-240*s+float(i)*20*s;
    drawSlanted(d,{x,cy+74*s},{x+14*s,cy+81*s},3*s,withAlpha(done?cyan:steel,(done?.9F:.25F)*card),withAlpha(done?cyan:steel,(done?.9F:.25F)*card));
  }
  const char* status=progress<.35F?"SEALING SECTOR":progress<1?"LOADING TEST CHAMBER":"CHAMBER READY";
  const float statusWidth=drawTracked(nullptr,15*s,{},0,status,4*s);
  drawTracked(d,15*s,{(size.x-statusWidth)*.5F,cy+96*s},withAlpha(steel,card),status,4*s);
}

namespace {
struct SettingsState {
  int selected=0; bool capturing=false;
  float captureAt=0, lastDraw=-10, shownAt=0, noticeAt=-10;
  float highlight[KeyBindings::count+2]{};
  const char* notice="";
  ImVec2 mouse{-1,-1};
} settingsPage;
}
bool drawSettings(KeyBindings& keys,float time,int pressedKey,bool escape,bool overGame) {
  const auto& io=ImGui::GetIO(); const auto size=io.DisplaySize;
  auto* d=ImGui::GetBackgroundDrawList();
  const float s=std::min(size.x/1440.F,size.y/900.F),left=std::max(60*s,size.x*.06F);
  auto& st=settingsPage;
  if(time-st.lastDraw>.25F) { st=SettingsState{}; st.shownAt=time; }
  const float dt=std::clamp(time-st.lastDraw,0.F,.1F); st.lastDraw=time;
  const float shown=time-st.shownAt,intro=smooth(shown/.45F);
  constexpr int rows=KeyBindings::count+2;  // Actions, then reset and back.
  drawSlab(d,size,s,time,intro,overGame);
  const float top=size.y*.10F;
  d->AddRectFilled({left,top},{left+9*s,top+9*s},withAlpha(amber,(std::fmod(time,1.2F)<.8F?1.F:.25F)*intro));
  drawTracked(d,15*s,{left+20*s,top-4*s},withAlpha(steel,intro),"RELAY RESEARCH  //  SETTINGS",3*s);
  drawGlow(d,100*s,{left-4*s,top+24*s+12*s*(1-intro)},withAlpha(cyan,intro),cyan,"SETTINGS",6*s);
  drawTracked(d,18*s,{left,top+136*s},withAlpha(steel,intro),"CONTROLS  //  KEY BINDINGS",2.5F*s);

  // A pending capture takes the next key: Esc cancels; reserved keys are refused.
  bool consumed=false;
  if(st.capturing) {
    if(escape) { st.capturing=false; consumed=true; }
    else if(pressedKey>=0&&time>st.captureAt) {
      const bool ok=keys.assign(KeyAction(st.selected),pressedKey);
      if(ok) keys.save(settingsPath());
      st.notice=ok?"KEY BOUND":"THAT KEY IS RESERVED"; st.noticeAt=time;
      st.capturing=false; consumed=true;
    }
  }
  if(!st.capturing&&!consumed) {
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)||ImGui::IsKeyPressed(ImGuiKey_W)) st.selected=(st.selected+rows-1)%rows;
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)||ImGui::IsKeyPressed(ImGuiKey_S)) st.selected=(st.selected+1)%rows;
  }
  const bool mouseMoved=std::abs(io.MousePos.x-st.mouse.x)+std::abs(io.MousePos.y-st.mouse.y)>.5F; st.mouse=io.MousePos;
  int activated=-1;
  const float firstRow=top+178*s,step=46*s,rowWidth=470*s;
  for(int i=0;i<rows;++i) {
    const float iy=firstRow+float(i)*step+(i>=KeyBindings::count?14*s:0);
    const ImVec2 a(left-20*s,iy-6*s),b(left+rowWidth,iy+40*s);
    const bool hover=!st.capturing&&io.MousePos.x>=a.x&&io.MousePos.x<=b.x&&io.MousePos.y>=a.y&&io.MousePos.y<=b.y;
    if(hover&&mouseMoved) st.selected=i;
    if(hover&&ImGui::IsMouseClicked(0)) activated=i;
    auto& h=st.highlight[i]; h+=((st.selected==i?1.F:0.F)-h)*(1-std::exp(-18*dt));
    const float appear=smooth((shown-.12F-.035F*float(i))/.3F),x=left-30*s*(1-appear);
    if(h>.01F) {
      drawSlanted(d,{x-12*s,iy},{x-12*s+(rowWidth+8*s)*smooth(h),iy+36*s},10*s,withAlpha(cyan,.9F*appear*h),withAlpha(cyan,.05F*appear*h));
      d->AddRectFilled({x-20*s,iy},{x-16*s,iy+36*s},withAlpha(amber,appear*h));
    }
    const ImU32 ink=h>.5F?withAlpha(navy,appear):withAlpha(ice,appear*.85F);
    const char* label=i<KeyBindings::count?KeyBindings::actionName(KeyAction(i)):i==KeyBindings::count?"RESET TO DEFAULTS":"BACK";
    drawTracked(d,30*s,{x+10*s+8*s*h,iy+1*s},ink,label,3*s,true);
    if(i<KeyBindings::count) {
      // Current key in a cap on the right; it blinks while waiting for a new key.
      const bool waiting=st.capturing&&st.selected==i;
      const std::string name=waiting?"PRESS A KEY":KeyBindings::keyName(keys[KeyAction(i)]);
      const float w=drawTracked(nullptr,24*s,{},0,name.c_str(),2*s,true)+28*s,cx=x+rowWidth-w-20*s;
      const ImU32 cap=waiting?withAlpha(amber,appear*(std::fmod(time,.6F)<.4F?1.F:.35F)):h>.5F?withAlpha(navy,appear):withAlpha(cyan,appear*.9F);
      d->AddRectFilled({cx,iy+2*s},{cx+w,iy+34*s},withAlpha(navy,(h>.5F?.25F:.6F)*appear),4*s);
      d->AddRect({cx,iy+2*s},{cx+w,iy+34*s},cap,4*s,0,1.8F*s);
      drawTracked(d,24*s,{cx+14*s,iy+4*s},cap,name.c_str(),2*s,true);
    }
  }
  if(!st.capturing&&!consumed&&(ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false))) activated=st.selected;
  bool leave=!st.capturing&&!consumed&&escape;
  if(activated>=0&&!st.capturing) {
    if(activated<KeyBindings::count) { st.capturing=true; st.captureAt=time; st.selected=activated; }
    else if(activated==KeyBindings::count) { keys.reset(); keys.save(settingsPath()); st.notice="DEFAULTS RESTORED"; st.noticeAt=time; }
    else leave=true;
  }
  if(time-st.noticeAt<2.F) drawTracked(d,16*s,{left,firstRow+float(rows)*step+30*s},withAlpha(amber,std::min(1.F,(2.F-(time-st.noticeAt))/.4F)),st.notice,3*s);

  const float hy=size.y-76*s,hint=smooth((shown-.4F)/.4F);
  const ImU32 hk=withAlpha(cyan,hint*.85F),ht=withAlpha(steel,hint);
  float hx=left;
  if(st.capturing) {
    hx+=drawKeycap(d,s,{hx,hy},hk,"ANY KEY")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"BIND",2.5F*s)+28*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ESC")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"CANCEL",2.5F*s);
  } else {
    hx+=drawKeycap(d,s,{hx,hy},hk,"W / S")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"SELECT",2.5F*s)+28*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ENTER")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"REBIND",2.5F*s)+28*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ESC")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"BACK",2.5F*s);
  }
  return leave;
}
}
