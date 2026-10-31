#include "app/RelayMenu.h"
#include "core/RelayGame.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>
namespace relay {
namespace {
using namespace palette;
float smooth(float x) { x=std::clamp(x,0.F,1.F); return x*x*(3-2*x); }
ImFont* display=nullptr;
struct Item { const char* label; MenuAction action; };
struct MenuState {
  int selected=0; MenuMode mode=MenuMode::Title; float lockedAt=-10;
  float lastDraw=-10, shownAt=0;
  float highlight[6]{};
  int lastSelected=0; float selectedAt=-10;  // The selected label pops when the choice moves.
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
  // A bold body face (the rounded one doubles as both) only gets the hairline too.
  const float weight=std::max(.6F,size*(useDisplay||font==display?.012F:.035F));
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
    // Acrylic key: a near-white cap (the colour only sets its opacity) with a graphite legend.
    const float a=float((c>>IM_COL32_A_SHIFT)&0xFF)/255.F;
    drawAcrylicRect(d,p,{p.x+w,p.y+29*s},5*s,a,IM_COL32(250,251,252,255));
    drawTracked(d,16*s,{p.x+9*s,p.y+5*s},withAlpha(graphite,a),key,1.5F*s);
  }
  return w;
}
void drawPanel(ImDrawList* d,ImVec2 a,ImVec2 b,float cut,ImU32 fill,ImU32 edge,float thickness) {
  const ImVec2 points[6]{{a.x+cut,a.y},{b.x,a.y},{b.x,b.y-cut},{b.x-cut,b.y},{a.x,b.y},{a.x,a.y+cut}};
  const float alpha=float((fill>>IM_COL32_A_SHIFT)&0xFF)/255.F;
  drawAcrylicPoly(d,points,6,alpha,fill|IM_COL32_A_MASK);
  // A short tick in the edge colour along the top, just past the cut corner.
  if(thickness>0) d->AddRectFilled({a.x+cut*1.3F,a.y},{a.x+cut*1.3F+std::min(cut*4.F,(b.x-a.x)*.3F),a.y+thickness*1.8F},edge);
  // Tech brackets in the two square corners, light and thin.
  const ImU32 bracket=withAlpha(glowCyan,.8F*alpha);
  const float l=cut*.9F,t=std::max(1.F,thickness*.8F);
  d->AddLine({b.x-l,a.y+t},{b.x-t,a.y+t},bracket,t); d->AddLine({b.x-t,a.y+t},{b.x-t,a.y+l},bracket,t);
  d->AddLine({a.x+t,b.y-l},{a.x+t,b.y-t},bracket,t); d->AddLine({a.x+t,b.y-t},{a.x+l,b.y-t},bracket,t);
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
void transformSince(ImDrawList* d,int start,ImVec2 c,float angle,float scale,ImVec2 offset) {
  const float co=std::cos(angle)*scale,si=std::sin(angle)*scale;
  for(int i=start;i<d->VtxBuffer.Size;++i) {
    ImVec2& v=d->VtxBuffer[i].pos; const float x=v.x-c.x,y=v.y-c.y;
    v={c.x+x*co-y*si+offset.x,c.y+x*si+y*co+offset.y};
  }
}
float punch(float age,float strength,float frequency,float decay) { return age<0?1:1+strength*std::exp(-decay*age)*std::cos(age*frequency); }
float elasticIn(float age,float duration) {
  if(age<=0) return 0;
  const float k=std::min(1.F,age/duration);
  return 1-std::exp(-7.F*k)*std::cos(k*12.F);
}
void finishSurface(ImDrawList* d,int start,ImVec2 a,ImVec2 b,ImU32 fill,float cut,float lean) {
  const float alpha=float((fill>>IM_COL32_A_SHIFT)&0xFF)/255.F;
  if(alpha<=.01F) return;
  const auto mix=[](ImU32 x,ImU32 y,float k) {
    ImU32 out=0;
    for(int shift:{IM_COL32_R_SHIFT,IM_COL32_G_SHIFT,IM_COL32_B_SHIFT}) {
      const float l=float((x>>shift)&0xFF),r=float((y>>shift)&0xFF);
      out|=ImU32(l+(r-l)*k)<<shift;
    }
    return out;
  };
  // Lighter at the top, sinking toward a deep teal at the bottom (keeps every surface cyan-toned).
  const ImU32 top=mix(fill,IM_COL32(255,255,255,255),.2F),bottom=mix(fill,IM_COL32(16,84,106,255),.38F);
  const float w=std::max(1.F,b.x-a.x),h=std::max(1.F,b.y-a.y);
  for(int i=start;i<d->VtxBuffer.Size;++i) {
    auto& v=d->VtxBuffer[i];
    v.col=mix(top,bottom,std::clamp((v.pos.y-a.y)/h,0.F,1.F))|(v.col&IM_COL32_A_MASK);
  }
  // Dot matrix (staggered rows, about 2400 dots at most), one spacing in from the edges; a sheen
  // band runs diagonally through it every few seconds.
  const float spacing=std::max(5.F,std::sqrt(w*h/2400.F)),dot=std::max(1.5F,spacing*.28F);
  const float span=w+h*.6F,band=std::fmod(float(ImGui::GetTime())*.3F,1.8F)*span-.4F*span,bandWidth=.14F*span;
  int row=0;
  for(float y=a.y+spacing;y<b.y-spacing*.6F;y+=spacing,++row) {
    const float right=b.x-lean*(y-a.y)/h-spacing*.6F;
    for(float x=a.x+spacing+(row%2?spacing*.5F:0);x<right;x+=spacing) {
      if(x-a.x+y-a.y<cut+spacing||right-x+b.y-y<cut+spacing) continue;
      const float k=(x-a.x+(y-a.y)*.6F-band)/bandWidth,sheen=std::exp(-k*k);
      d->AddRectFilled({x,y},{x+dot,y+dot},withAlpha(IM_COL32(255,255,255,255),alpha*(.13F+.3F*sheen)));
    }
  }
  d->AddLine({a.x+cut,a.y+1},{b.x-1,a.y+1},withAlpha(IM_COL32(255,255,255,255),.3F*alpha),1.5F);
}
namespace { ImTextureID backdropTexture=0; }
void setBackdrop(ImTextureID texture) { backdropTexture=texture; }
void drawAcrylicPoly(ImDrawList* d,const ImVec2* points,int count,float opacity,ImU32 tint,float backdrop) {
  opacity=std::clamp(opacity,0.F,1.F);
  if(opacity<=.01F||count<3) return;
  ImVec2 a=points[0],b=points[0];
  for(int i=1;i<count;++i) { a.x=std::min(a.x,points[i].x); a.y=std::min(a.y,points[i].y); b.x=std::max(b.x,points[i].x); b.y=std::max(b.y,points[i].y); }
  const float w=std::max(1.F,b.x-a.x),h=std::max(1.F,b.y-a.y);
  const ImU32 white=IM_COL32(255,255,255,255);
  std::vector<ImVec2> shadow(points,points+count);
  for(auto& p:shadow) p.y+=std::clamp(h*.05F,2.F,6.F);
  d->AddConvexPolyFilled(shadow.data(),count,withAlpha(shade,.1F*opacity));
  // The blurred scene, mapped to the screen (the capture is bottom-up).
  const auto size=ImGui::GetIO().DisplaySize;
  const bool blur=backdropTexture!=0&&backdrop>0&&size.x>0&&size.y>0;
  if(blur) {
    d->PushTexture(ImTextureRef(backdropTexture));
    d->PrimReserve((count-2)*3,count);
    const unsigned base=d->_VtxCurrentIdx;
    const ImU32 col=withAlpha(white,opacity*backdrop);
    for(int i=0;i<count;++i) d->PrimWriteVtx(points[i],{points[i].x/size.x,1-points[i].y/size.y},col);
    for(int i=1;i<count-1;++i) { d->PrimWriteIdx(ImDrawIdx(base)); d->PrimWriteIdx(ImDrawIdx(base+unsigned(i))); d->PrimWriteIdx(ImDrawIdx(base+unsigned(i)+1)); }
    d->PopTexture();
  }
  // The tint, lighter at the top and settling to a cooler grey at the bottom.
  const auto mix=[](ImU32 x,ImU32 y,float k) {
    ImU32 out=0;
    for(int shift:{IM_COL32_R_SHIFT,IM_COL32_G_SHIFT,IM_COL32_B_SHIFT}) {
      const float l=float((x>>shift)&0xFF),r=float((y>>shift)&0xFF);
      out|=ImU32(l+(r-l)*k)<<shift;
    }
    return out;
  };
  const float tintAlpha=(blur?.68F:.95F)*opacity*float((tint>>IM_COL32_A_SHIFT)&0xFF)/255.F;
  const int start=d->VtxBuffer.Size;
  d->AddConvexPolyFilled(points,count,withAlpha(tint,tintAlpha));
  const ImU32 top=mix(tint,white,.45F),bottom=mix(tint,IM_COL32(160,166,176,255),.3F);
  for(int i=start;i<d->VtxBuffer.Size;++i) {
    auto& v=d->VtxBuffer[i];
    v.col=mix(top,bottom,std::clamp((v.pos.y-a.y)/h,0.F,1.F))|(v.col&IM_COL32_A_MASK);
  }
  // Grain: fixed light and dark specks in the panel's own frame (so they ride along with it),
  // the light ones catching a slow diagonal sheen.
  const auto hash=[](float n) { const float v=std::sin(n*12.9898F)*43758.5453F; return v-std::floor(v); };
  const auto inside=[&](ImVec2 p) {
    float sign=0;
    for(int i=0;i<count;++i) {
      const ImVec2 p0=points[i],p1=points[(i+1)%count];
      const float cross=(p1.x-p0.x)*(p.y-p0.y)-(p1.y-p0.y)*(p.x-p0.x);
      if(cross!=0) { if(sign==0) sign=cross; else if((cross>0)!=(sign>0)) return false; }
    }
    return true;
  };
  const int grains=int(std::min(2600.F,w*h/36.F));
  const float span=w+h*.6F,band=std::fmod(float(ImGui::GetTime())*.25F,1.8F)*span-.4F*span;
  for(int i=0;i<grains;++i) {
    const ImVec2 p(a.x+hash(float(i)*1.37F+.11F)*w,a.y+hash(float(i)*2.71F+.37F)*h);
    if(!inside(p)) continue;
    const bool light=i%2==0;
    const float k=(p.x-a.x+(p.y-a.y)*.6F-band)/(.14F*span),sheen=light?std::exp(-k*k):0;
    // Light specks turn bright cyan where the sheen passes.
    d->AddRectFilled(p,{p.x+1.3F,p.y+1.3F},withAlpha(light?(sheen>.3F?glowCyan:white):graphite,opacity*(light?.22F+.55F*sheen:.07F)));
  }
  // A glowing cyan top edge (a bright core inside a soft bloom), a darker bottom edge, a faint
  // cyan hairline round the sheet and a faint cyan halo outside it.
  d->AddPolyline(points,count,withAlpha(glowCyan,.07F*opacity),ImDrawFlags_Closed,6.F);
  for(int i=0;i<count;++i) {
    const ImVec2 p0=points[i],p1=points[(i+1)%count];
    if(std::abs(p0.y-a.y)<.5F&&std::abs(p1.y-a.y)<.5F) {
      d->AddLine({p0.x,p0.y+1.5F},{p1.x,p1.y+1.5F},withAlpha(glowCyan,.2F*opacity),5.F);
      d->AddLine({p0.x,p0.y+1},{p1.x,p1.y+1},withAlpha(IM_COL32(170,245,255,255),.95F*opacity),1.5F);
    }
    if(std::abs(p0.y-b.y)<.5F&&std::abs(p1.y-b.y)<.5F) d->AddLine({p0.x,p0.y-1},{p1.x,p1.y-1},withAlpha(graphite,.1F*opacity),1.F);
  }
  d->AddPolyline(points,count,withAlpha(glowCyan,.3F*opacity),ImDrawFlags_Closed,1.F);
}
void drawAcrylicRect(ImDrawList* d,ImVec2 a,ImVec2 b,float rounding,float opacity,ImU32 tint,float backdrop) {
  d->PathRect(a,b,rounding);
  std::vector<ImVec2> points(d->_Path.Data,d->_Path.Data+d->_Path.Size);
  d->PathClear();
  drawAcrylicPoly(d,points.data(),int(points.size()),opacity,tint,backdrop);
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

// Acrylic control slab (faint scanlines and a sweep for the tech feel) with a slanted, bright right
// edge (title, pause and settings).
void drawSlab(ImDrawList* d,ImVec2 size,float s,float time,float intro,bool dim) {
  if(dim) d->AddRectFilled({0,0},size,withAlpha(IM_COL32(20,24,30,255),.3F));
  const float panel=std::max(.44F*size.x,660*s)*(.6F+.4F*intro),lean=130*s;
  {
    const ImVec2 slab[4]{{0,0},{panel,0},{panel-lean,size.y},{0,size.y}};
    drawAcrylicPoly(d,slab,4,1,surface);
    drawScanlines(d,{0,0},{panel-lean,size.y},4*s,withAlpha(graphite,.025F));
    const float sweep=std::fmod(time*140*s,size.y+200*s)-100*s;
    const ImU32 white=IM_COL32(255,255,255,255);
    d->AddRectFilledMultiColor({0,sweep},{panel-lean*(sweep/size.y),sweep+70*s},withAlpha(white,0),withAlpha(white,0),withAlpha(white,.16F),withAlpha(white,.16F));
    d->AddLine({panel,0},{panel-lean,size.y},withAlpha(glowCyan,.16F),10*s);
    d->AddLine({panel,0},{panel-lean,size.y},withAlpha(IM_COL32(170,245,255,255),.95F),2*s);
  }

}

int menuSelection() { return menu.selected; }
MenuAction drawMenu(MenuMode mode,int& level,float time,bool hasCheckpoint) {
  const bool paused=mode==MenuMode::Pause;
  const auto& io=ImGui::GetIO(); const auto size=io.DisplaySize;
  auto* d=ImGui::GetBackgroundDrawList();
  const float s=std::min(size.x/1440.F,size.y/900.F),left=std::max(60*s,size.x*.06F);
  // Re-run the entrance whenever the menu reappears or switches mode.
  if(time-menu.lastDraw>.25F||mode!=menu.mode) { menu.shownAt=time; menu.selected=menu.lastSelected=0; menu.selectedAt=-10; menu.mode=mode; std::fill(std::begin(menu.highlight),std::end(menu.highlight),0.F); }
  const float dt=std::clamp(time-menu.lastDraw,0.F,.1F); menu.lastDraw=time;
  const float shown=time-menu.shownAt,intro=smooth(shown/.55F);
  // One word each: REBOOT returns to the last checkpoint (or the start) as a reboot would.
  const Item pauseItems[5]{{"RESUME",MenuAction::Resume},{"REBOOT",MenuAction::Checkpoint},
    {"RESTART",MenuAction::Restart},{"SETTINGS",MenuAction::Settings},{"MENU",MenuAction::Title}};
  (void)hasCheckpoint;
  // Training replaces START while the missions are locked.
  const bool locked=!relay::Game::missionsUnlocked;
  // PRACTICE opens the practice field; LEVEL (the mission selector) sits on row levelRow.
  const Item titleItems[6]{{locked?"TRAINING":"START",MenuAction::Start},{"PRACTICE",MenuAction::Practice},{"LEVEL",MenuAction::None},{"LAB",MenuAction::Gallery},
    {"SETTINGS",MenuAction::Settings},{"QUIT",MenuAction::Quit}};
  const Item* items=paused?pauseItems:titleItems;
  const int count=paused?5:6;
  constexpr int levelRow=2;

  drawSlab(d,size,s,time,intro,paused);

  // No header strip, subtitle, item numbers or key hints: both menus show just the wordmark (or
  // PAUSED) and the list.
  const float top=size.y*.10F;
  float y=top+30*s;
  if(paused) {
    const int start=d->VtxBuffer.Size;
    const float w=drawGlow(d,112*s,{left-4*s,y+14*s*(1-intro)},withAlpha(deepTeal,intro),glowCyan,"PAUSED",6*s);
    transformSince(d,start,{left+w*.5F,y+60*s},.015F*std::sin(time*1.1F),1,{0,3*s*std::sin(time*1.4F)});
    y+=118*s;
  } else {
    // Staggered two-line wordmark: graphite VOXEL, muted teal RELAY nudged right, both with a soft
    // white glow (embossed on the acrylic). Each word sways and
    // bobs gently on its own beat, like a card held in the hand.
    const int voxel=d->VtxBuffer.Size;
    const float vw=drawGlow(d,132*s,{left-6*s,y+16*s*(1-intro)},withAlpha(graphite,intro),glowCyan,"VOXEL",8*s);
    transformSince(d,voxel,{left+vw*.5F,y+80*s},.022F*std::sin(time*1.1F),1,{0,4*s*std::sin(time*1.4F)});
    const int relayWord=d->VtxBuffer.Size;
    const float rw=drawGlow(d,132*s,{left+46*s-30*s*(1-intro),y+112*s},withAlpha(IM_COL32(0,176,210,255),intro),glowCyan,"RELAY",8*s);
    transformSince(d,relayWord,{left+46*s+rw*.5F,y+176*s},-.022F*std::sin(time*1.1F+.9F),1,{0,4*s*std::sin(time*1.4F+1.2F)});
    y+=262*s;
  }
  for(int i=0;i<14;++i) {
    const bool lit=std::fmod(time*6.F-float(i)*.5F,10.F)<4.F;
    const ImU32 dash=withAlpha(lit?glowCyan:graphite,(lit?.9F:.15F)*intro);
    drawSlanted(d,{left+float(i)*20*s,y},{left+float(i)*20*s+13*s,y+7*s},4*s,dash,dash);
  }

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
    // Selection: a white blade sweeps out from the left behind the label.
    if(h>.01F) {
      drawSlanted(d,{x-14*s,iy},{x-14*s+(barWidth+10*s)*smooth(h),iy+52*s},14*s,withAlpha(IM_COL32(255,255,255,255),.85F*appear*h),withAlpha(IM_COL32(255,255,255,255),.05F*appear*h));
      // A glowing cyan underline runs along the blade.
      drawSlanted(d,{x-17*s,iy+46*s},{x-17*s+(barWidth+10*s)*smooth(h),iy+56*s},2*s,withAlpha(glowCyan,.18F*appear*h),withAlpha(glowCyan,0));
      drawSlanted(d,{x-16*s,iy+49*s},{x-16*s+(barWidth+10*s)*smooth(h),iy+53*s},1*s,withAlpha(glowCyan,.95F*appear*h),withAlpha(glowCyan,0));
      d->AddRectFilled({x-24*s,iy},{x-19*s,iy+52*s},withAlpha(amber,appear*h));
    }
    const ImU32 ink=withAlpha(graphite,appear*(h>.5F?1.F:.75F));
    const int labelStart=d->VtxBuffer.Size;
    drawTracked(d,48*s,{x+8*s+12*s*h,iy-1*s},ink,items[i].label,4*s,true);
    if(!paused&&i==levelRow) {
      // Level selector: value on the right of the blade, arrows when focused.
      const float vx=x+230*s;
      if(locked) {
        // Missions locked: a padlock and LOCKED in place of the level, flashing when the player tries to enter.
        const float flash=std::clamp(1-(time-menu.lockedAt)/.6F,0.F,1.F),pulse=flash>0?.5F+.5F*std::sin(time*30):1.F;
        const ImU32 tag=withAlpha(IM_COL32(200,120,20,255),appear*(.6F+.4F*pulse));
        d->AddRectFilled({vx,iy+20*s},{vx+20*s,iy+38*s},tag,2*s);
        d->PathArcTo({vx+10*s,iy+20*s},7*s,3.14159F,6.28318F,12); d->PathStroke(tag,0,3*s);
        drawTracked(d,34*s,{vx+32*s,iy+7*s},tag,"LOCKED",3*s,true);
      } else {
        drawTracked(d,34*s,{vx,iy+7*s},withAlpha(graphite,appear*.9F),levelNames[level],2*s,true);
        const ImU32 arrow=withAlpha(graphite,appear*(.4F+.6F*h));
        d->AddTriangleFilled({vx-24*s,iy+26*s},{vx-12*s,iy+18*s},{vx-12*s,iy+34*s},arrow);
        const float end=vx+drawTracked(nullptr,34*s,{},0,levelNames[level],2*s,true)+12*s;
        d->AddTriangleFilled({end+12*s,iy+26*s},{end,iy+18*s},{end,iy+34*s},arrow);
      }
    }
    // The newly selected label pops: a punch with a small twist that settles.
    if(menu.selected==i) {
      const float age=time-menu.selectedAt;
      if(age<.6F) transformSince(d,labelStart,{x+8*s,iy+26*s},-.06F*std::exp(-9.F*age)*std::sin(age*24),punch(age,.16F,28,11));
    }
  }
  if(menu.selected!=menu.lastSelected) { menu.lastSelected=menu.selected; menu.selectedAt=time; }
  if(ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false)) activated=menu.selected;
  MenuAction action=MenuAction::None;
  if(activated>=0) {
    if(!paused&&activated==levelRow) { if(locked) menu.lockedAt=time; else level=1-level; }
    else action=items[activated].action;
  }

  // Key hints along the bottom of the slab.
  const float hy=size.y-76*s,hint=smooth((shown-.5F)/.4F);
  const ImU32 hk=withAlpha(cyan,hint),ht=withAlpha(graphiteSoft,hint);
  float hx=left;
  if(!paused&&menu.selected==levelRow) {
    if(locked) drawTracked(d,16*s,{hx,hy+5*s},withAlpha(amber,hint),"MISSIONS LOCKED  //  START WITH TRAINING",2.5F*s);
    else { hx+=drawKeycap(d,s,{hx,hy},hk,"A / D")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"LEVEL",2.5F*s); }
  }

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
    drawPanel(d,{tag.x,tag.y-6*s},{tag.x+210*s,tag.y+48*s},10*s,withAlpha(slate,intro),withAlpha(cyan,intro),1.2F*s);
    drawTracked(d,30*s,{tag.x+12*s,tag.y-2*s},withAlpha(graphite,intro),"UNIT 07",3*s,true);
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
  // Five boot beats as pips instead of captions: each pops in as its beat starts (with a ring that
  // snaps outward) and the current one pulses. Skip shows as ENTER and a fast-forward glyph.
  constexpr float beats[]{0,.65F,1.35F,2.25F,3.F};
  const ImVec2 row(size.x*.5F,size.y-h*.5F);
  for(int i=0;i<5;++i) {
    const ImVec2 c(row.x+(float(i)-2)*54*s,row.y);
    const float age=time-beats[i];
    const bool current=age>=0&&(i==4||time<beats[i+1]);
    const int start=d->VtxBuffer.Size;
    ImVec2 hex[6]; for(int k=0;k<6;++k) { const float t=float(k)*1.0472F+.5236F; hex[k]={c.x+std::cos(t)*14*s,c.y+std::sin(t)*14*s}; }
    if(age>=0) {
      d->AddCircleFilled(c,24*s,withAlpha(cyan,(current?.18F+.12F*std::sin(time*8):.1F)*bars),24);
      d->AddConvexPolyFilled(hex,6,withAlpha(i==4?mint:cyan,bars));
      transformSince(d,start,c,.4F*std::exp(-7.F*age)*std::sin(age*20),punch(age,.7F,24,8));
      if(age<.5F) d->AddCircle(c,(16+60*age)*s,withAlpha(ice,(1-age*2)*bars),6,3*s);
    } else d->AddPolyline(hex,6,withAlpha(steel,.5F*bars),ImDrawFlags_Closed,2*s);
  }
  d->AddRectFilled({40*s,h*.5F-4*s},{48*s,h*.5F+4*s},withAlpha(amber,bars*(std::fmod(time,1.F)<.6F?1.F:.3F)));
  const float skip=drawKeycap(nullptr,s,{},0,"ENTER"),fx=size.x-60*s,fy=h*.5F;
  drawKeycap(d,s,{fx-skip-18*s,fy-15*s},withAlpha(cyan,bars*.85F),"ENTER");
  for(int k=0;k<2;++k) d->AddTriangleFilled({fx+float(k)*12*s,fy-8*s},{fx+float(k)*12*s+12*s,fy},{fx+float(k)*12*s,fy+8*s},withAlpha(steel,bars));
}

void drawLevelTransition(float black,float card,float progress,int level,float time) {
  const auto size=ImGui::GetIO().DisplaySize; auto* d=ImGui::GetForegroundDrawList();
  const float s=std::min(size.x/1440.F,size.y/900.F);
  d->AddRectFilled({0,0},size,IM_COL32(2,7,14,int(255*std::clamp(black,0.F,1.F))));
  if(card<=0) return;
  // Wordless sector card, dealt like a playing card: it flips open, punches in from a settling
  // twist and then sways, inside a slowly turning ring of ticks. Training shows a beacon target, a
  // mission a core with one pip per sector number. The segmented load bar fills beneath it, each
  // segment popping as it lights.
  const bool training=level==relay::Game::tutorialLevel,practice=level==relay::Game::practiceLevel;
  const ImU32 accent=training?mint:cyan;
  const float a=std::clamp(card,0.F,1.F),age=progress*2.4F-.42F;
  const ImVec2 c(size.x*.5F,size.y*.45F);
  const float r=230*s,spin=time*.4F;
  for(int i=0;i<60;++i) {
    const float t=float(i)*.10472F+spin,in=i%5==0?.92F:.96F;
    d->AddLine({c.x+std::cos(t)*r*in,c.y+std::sin(t)*r*in},{c.x+std::cos(t)*r,c.y+std::sin(t)*r},withAlpha(accent,(i%5==0?.6F:.25F)*a),2*s);
  }
  for(int i=0;i<3;++i) { d->PathArcTo(c,r*1.07F,-spin*1.3F+float(i)*2.094F,-spin*1.3F+float(i)*2.094F+.9F,24); d->PathStroke(withAlpha(accent,.45F*a),0,2.5F*s); }
  const int start=d->VtxBuffer.Size;
  const ImVec2 ca(c.x-105*s,c.y-145*s),cb(c.x+105*s,c.y+145*s),ia(ca.x+16*s,ca.y+16*s),ib(cb.x-16*s,cb.y-16*s);
  drawAcrylicRect(d,ca,cb,18*s,a,slate,0);
  drawAcrylicRect(d,ia,ib,12*s,a,IM_COL32(214,218,224,255),0);
  for(int k=0;k<4;++k) { const ImVec2 p(k&1?ib.x-14*s:ia.x+6*s,k&2?ib.y-14*s:ia.y+6*s); d->AddRectFilled(p,{p.x+8*s,p.y+8*s},withAlpha(k==0?amber:accent,a)); }
  const ImVec2 e(c.x,c.y-16*s);
  const float glow=.5F+.5F*std::sin(time*3);
  if(practice) {
    // Practice field: three speed-pad chevrons stacked upward.
    d->AddCircleFilled(e,62*s,withAlpha(accent,.12F*glow*a),32);
    for(int k=0;k<3;++k) {
      const float y=e.y+30*s-float(k)*26*s;
      const ImVec2 pts[3]{{e.x-34*s,y+12*s},{e.x,y-12*s},{e.x+34*s,y+12*s}};
      d->AddPolyline(pts,3,withAlpha(accent,(.5F+.25F*float(k))*a),0,8*s);
    }
  } else if(training) {
    d->AddCircleFilled(e,62*s,withAlpha(accent,.12F*glow*a),32);
    for(int k=3;k>=1;--k) d->AddCircle(e,float(k)*19*s,withAlpha(accent,(.35F+.2F*float(3-k))*a),40,4.5F*s);
    d->AddCircleFilled(e,9*s,withAlpha(graphite,a),24);
  } else {
    ImVec2 hex[6]; for(int k=0;k<6;++k) { const float t=float(k)*1.0472F+.5236F; hex[k]={e.x+std::cos(t)*44*s,e.y+std::sin(t)*44*s}; }
    d->AddCircleFilled(e,62*s,withAlpha(accent,.12F*glow*a),32);
    d->AddConvexPolyFilled(hex,6,withAlpha(accent,a));
    for(auto& p:hex) p={e.x+(p.x-e.x)*.5F,e.y+(p.y-e.y)*.5F};
    d->AddConvexPolyFilled(hex,6,withAlpha(IM_COL32(255,255,255,255),a));
    for(int i=0;i<=level;++i) d->AddCircleFilled({c.x+(float(i)-float(level)*.5F)*24*s,c.y+86*s},7*s,withAlpha(graphite,a),16);
  }
  // Flip open (squash along x), then punch, twist and sway.
  const float flip=std::max(.03F,std::sin(std::clamp(age/.2F,0.F,1.F)*1.5708F));
  for(int i=start;i<d->VtxBuffer.Size;++i) d->VtxBuffer[i].pos.x=c.x+(d->VtxBuffer[i].pos.x-c.x)*flip;
  transformSince(d,start,c,-.14F*std::exp(-5.F*std::max(0.F,age))*std::cos(age*10)+.015F*std::sin(time*1.3F),punch(age,.22F,14,6),{0,4*s*std::sin(time*1.6F)});
  for(int i=0;i<24;++i) {
    const bool done=float(i)/24.F<progress;
    const float x=c.x-240*s+float(i)*20*s,y=c.y+r+34*s,lit=(progress-float(i)/24.F)*2.4F;
    const int segment=d->VtxBuffer.Size;
    const ImU32 col=withAlpha(done?accent:steel,(done?.9F:.25F)*a);
    drawSlanted(d,{x,y},{x+14*s,y+7*s},3*s,col,col);
    if(done&&lit<.4F) transformSince(d,segment,{x+7*s,y+3.5F*s},0,punch(lit,.9F,26,9));
  }
}

namespace {
struct SettingsState {
  int selected=0; bool capturing=false;
  float captureAt=0, lastDraw=-10, shownAt=0, noticeAt=-10;
  float highlight[KeyBindings::count+2]{};
  int lastSelected=0; float selectedAt=-10;
  const char* notice="";
  ImVec2 mouse{-1,-1};
} settingsPage;
}
int settingsSelection() { return settingsPage.selected; }
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
  drawTracked(d,15*s,{left+20*s,top-4*s},withAlpha(graphiteSoft,intro),"RELAY RESEARCH  //  SETTINGS",3*s);
  drawGlow(d,100*s,{left-4*s,top+24*s+12*s*(1-intro)},withAlpha(deepTeal,intro),glowCyan,"SETTINGS",6*s);
  drawTracked(d,18*s,{left,top+136*s},withAlpha(graphiteSoft,intro),"CONTROLS  //  KEY BINDINGS",2.5F*s);

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
      drawSlanted(d,{x-12*s,iy},{x-12*s+(rowWidth+8*s)*smooth(h),iy+36*s},10*s,withAlpha(IM_COL32(255,255,255,255),.85F*appear*h),withAlpha(IM_COL32(255,255,255,255),.05F*appear*h));
      drawSlanted(d,{x-14*s,iy+33*s},{x-14*s+(rowWidth+8*s)*smooth(h),iy+37*s},1*s,withAlpha(glowCyan,.95F*appear*h),withAlpha(glowCyan,0));
      d->AddRectFilled({x-20*s,iy},{x-16*s,iy+36*s},withAlpha(amber,appear*h));
    }
    const ImU32 ink=withAlpha(graphite,appear*(h>.5F?1.F:.75F));
    const char* label=i<KeyBindings::count?KeyBindings::actionName(KeyAction(i)):i==KeyBindings::count?"RESET TO DEFAULTS":"BACK";
    const int labelStart=d->VtxBuffer.Size;
    drawTracked(d,30*s,{x+10*s+8*s*h,iy+1*s},ink,label,3*s,true);
    if(st.selected==i&&time-st.selectedAt<.6F) {
      const float age=time-st.selectedAt;
      transformSince(d,labelStart,{x+10*s,iy+18*s},-.05F*std::exp(-9.F*age)*std::sin(age*24),punch(age,.14F,28,11));
    }
    if(i<KeyBindings::count) {
      // Current key in a cap on the right; it blinks while waiting for a new key.
      const bool waiting=st.capturing&&st.selected==i;
      const std::string name=waiting?"PRESS A KEY":KeyBindings::keyName(keys[KeyAction(i)]);
      const float w=drawTracked(nullptr,24*s,{},0,name.c_str(),2*s,true)+28*s,cx=x+rowWidth-w-20*s;
      const ImU32 cap=waiting?withAlpha(amber,appear*(std::fmod(time,.6F)<.4F?1.F:.35F)):withAlpha(graphite,appear);
      drawAcrylicRect(d,{cx,iy+2*s},{cx+w,iy+34*s},5*s,appear,IM_COL32(250,251,252,255));
      drawTracked(d,24*s,{cx+14*s,iy+4*s},cap,name.c_str(),2*s,true);
    }
  }
  if(st.selected!=st.lastSelected) { st.lastSelected=st.selected; st.selectedAt=time; }
  if(!st.capturing&&!consumed&&(ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false))) activated=st.selected;
  bool leave=!st.capturing&&!consumed&&escape;
  if(activated>=0&&!st.capturing) {
    if(activated<KeyBindings::count) { st.capturing=true; st.captureAt=time; st.selected=activated; }
    else if(activated==KeyBindings::count) { keys.reset(); keys.save(settingsPath()); st.notice="DEFAULTS RESTORED"; st.noticeAt=time; }
    else leave=true;
  }
  if(time-st.noticeAt<2.F) drawTracked(d,16*s,{left,firstRow+float(rows)*step+30*s},withAlpha(amber,std::min(1.F,(2.F-(time-st.noticeAt))/.4F)),st.notice,3*s);

  const float hy=size.y-76*s,hint=smooth((shown-.4F)/.4F);
  const ImU32 hk=withAlpha(cyan,hint),ht=withAlpha(graphiteSoft,hint);
  float hx=left;
  if(st.capturing) {
    hx+=drawKeycap(d,s,{hx,hy},hk,"ANY KEY")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"BIND",1.5F*s)+16*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ESC")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"CANCEL",1.5F*s);
  } else {
    hx+=drawKeycap(d,s,{hx,hy},hk,"W / S")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"SELECT",1.5F*s)+16*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ENTER")+10*s; hx+=drawTracked(d,16*s,{hx,hy+5*s},ht,"REBIND",1.5F*s)+16*s;
    hx+=drawKeycap(d,s,{hx,hy},hk,"ESC")+10*s; drawTracked(d,16*s,{hx,hy+5*s},ht,"BACK",1.5F*s);
  }
  return leave;
}
}
