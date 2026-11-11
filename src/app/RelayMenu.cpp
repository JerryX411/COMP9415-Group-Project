#include "app/RelayMenu.h"
#include <algorithm>
#include <string>
#include <cmath>
namespace relay {
namespace {
ImFont* face=nullptr;
int selected=0,setting=0,capture=-1;
MenuMode lastMode=MenuMode::Title;
ImVec2 lastMouse{-1000,-1000};
constexpr ImU32 ink=IM_COL32(242,247,252,255),muted=IM_COL32(188,205,218,255),accent=IM_COL32(100,220,255,255);
float scale(){auto v=ImGui::GetIO().DisplaySize;return std::min(v.x/1440.F,v.y/900.F);}
void text(ImDrawList* d,float size,ImVec2 pos,ImU32 c,const char* t){auto* font=face?face:ImGui::GetFont();d->AddText(font,size,{pos.x+1,pos.y+2},IM_COL32(0,0,0,150),t);d->AddText(font,size,pos,c,t);}
void center(ImDrawList* d,float y,float size,ImU32 c,const char* t){auto* f=face?face:ImGui::GetFont();float w=f->CalcTextSizeA(size,1e6F,0,t).x;text(d,size,{(ImGui::GetIO().DisplaySize.x-w)*.5F,y},c,t);}
bool button(ImDrawList* d,ImVec2 a,ImVec2 b,const char* label,bool focus,float s){
 auto m=ImGui::GetIO().MousePos;bool hover=m.x>=a.x&&m.x<=b.x&&m.y>=a.y&&m.y<=b.y;
 text(d,32*s,{a.x+20*s,a.y+8*s},focus||hover?accent:ink,label);
 return hover&&ImGui::IsMouseClicked(0);
}
void navigation(int& row,int count){
 if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)||ImGui::IsKeyPressed(ImGuiKey_W))row=(row+count-1)%count;
 if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)||ImGui::IsKeyPressed(ImGuiKey_S))row=(row+1)%count;
}
bool enter(){return ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false);}
}
void setDisplayFont(ImFont* f){face=f;}
ImFont* displayFont(){return face;}
void prewarmFonts(float){}
ImU32 withAlpha(ImU32 c,float a){return (c&~IM_COL32_A_MASK)|(ImU32(255*std::clamp(a,0.F,1.F))<<IM_COL32_A_SHIFT);}
float drawTracked(ImDrawList* d,float size,ImVec2 p,ImU32 c,const char* t,float,bool){
 auto* f=face?face:ImGui::GetFont();if(d)text(d,size,p,c,t);return f->CalcTextSizeA(size,1e6F,0,t).x;
}
int menuSelection(){return selected;} int settingsSelection(){return setting;}
void drawLevelTransition(float black,float card,const char* kicker,const char* name){
 auto* d=ImGui::GetForegroundDrawList();const auto size=ImGui::GetIO().DisplaySize;const float s=scale();
 d->AddRectFilled({0,0},size,withAlpha(IM_COL32(3,8,15,255),black));
 if(card<=0)return;
 const float rise=(1-card)*10*s,y=size.y*.5F-40*s+rise;
 const std::string number=kicker;
 auto* f=face?face:ImGui::GetFont();
 const float nw=f->CalcTextSizeA(22*s,1e6F,0,number.c_str()).x,tw=f->CalcTextSizeA(52*s,1e6F,0,name).x;
 d->AddText(f,22*s,{(size.x-nw)*.5F,y},withAlpha(muted,card),number.c_str());
 d->AddText(f,52*s,{(size.x-tw)*.5F,y+30*s},withAlpha(ink,card),name);
 const float half=tw*.5F*std::min(1.F,card*1.3F);
 d->AddLine({size.x*.5F-half,y+96*s},{size.x*.5F+half,y+96*s},withAlpha(accent,card),2*s);
}
MenuAction drawMenu(MenuMode mode,int& level,float,bool){
 // Title: the name over five centred rows (Tutorial, Level 1, Level 2, Settings, Quit); picking a
 // level starts it. Pause: a dark veil over the game with four centred rows.
 const bool pause=mode==MenuMode::Pause;auto* d=ImGui::GetBackgroundDrawList();auto size=ImGui::GetIO().DisplaySize;float s=scale();
 const int count=pause?4:5;
 if(mode!=lastMode){selected=pause?0:std::clamp(level,0,2);lastMode=mode;}navigation(selected,count);
 const float top=size.y*.5F-(pause?235.F:250.F)*s;
 if(pause) d->AddRectFilled({0,0},size,IM_COL32(2,6,12,175));
 center(d,top+20*s,58*s,ink,pause?"Paused":"Core Explorer Lite");
 const char* titleLabels[]{"Tutorial","Level 1","Level 2","Settings","Quit"};
 const char* pauseLabels[]{"Resume","Restart","Settings","Main menu"};
 const char* const* labels=pause?pauseLabels:titleLabels;
 int activated=-1;auto mouse=ImGui::GetIO().MousePos;bool moved=std::abs(mouse.x-lastMouse.x)+std::abs(mouse.y-lastMouse.y)>.5F;lastMouse=mouse;
 auto* font=face?face:ImGui::GetFont();
 for(int i=0;i<count;++i){
  const float w=font->CalcTextSizeA(32*s,1e6F,0,labels[i]).x;
  const ImVec2 a{size.x*.5F-w*.5F-20*s,top+(115+i*65)*s},b{a.x+w+40*s,a.y+54*s};
  if(moved&&mouse.x>=a.x&&mouse.x<=b.x&&mouse.y>=a.y&&mouse.y<=b.y)selected=i;
  if(button(d,a,b,labels[i],selected==i,s))activated=i;
 }
 if(enter())activated=selected;
 if(activated<0)return MenuAction::None;
 if(pause){
  if(activated==0)return MenuAction::Resume;
  if(activated==1)return MenuAction::Restart;
  if(activated==2){capture=-1;return MenuAction::Settings;}
  return MenuAction::Title;
 }
 if(activated<=2){level=activated;return MenuAction::Start;}
 if(activated==3){capture=-1;return MenuAction::Settings;}
 return MenuAction::Quit;
}
bool drawSettings(KeyBindings& keys,float,int pressedKey,bool escape,bool){
 auto* d=ImGui::GetBackgroundDrawList();auto size=ImGui::GetIO().DisplaySize;float s=scale();
 float x=size.x*.5F-400*s,top=50*s;
 center(d,top,48*s,ink,"Controls");
 center(d,top+60*s,25*s,muted,"Select an action to change its key");
 center(d,top+(108+(KeyBindings::count+2)*58)*s,22*s,muted,"V  SWITCH CAMERA: FOLLOW / GLOBAL");
 bool consumed=capture>=0;
 if(capture>=0){if(escape)capture=-1;else if(pressedKey>=0&&keys.assign(KeyAction(capture),pressedKey)){keys.save(settingsPath());capture=-1;}}
 else navigation(setting,KeyBindings::count+2);
 int activated=-1;
 for(int i=0;i<KeyBindings::count+2;++i){
  std::string label=i<KeyBindings::count?KeyBindings::actionName(KeyAction(i)):i==KeyBindings::count?"Reset defaults":"Back";
  if(i<KeyBindings::count)label+="   :   "+std::string(capture==i?"Press a key (Esc cancels)":KeyBindings::keyName(keys[KeyAction(i)]));
  ImVec2 a{x,top+(108+i*58)*s},b{x+800*s,a.y+48*s};
  if(button(d,a,b,label.c_str(),i==setting,s)&&capture<0&&!consumed){activated=i;setting=i;}
 }
 if(!consumed&&enter())activated=setting;
 if(activated>=0){if(activated<KeyBindings::count)capture=activated;else if(activated==KeyBindings::count){keys.reset();keys.save(settingsPath());}else return true;}
 return escape&&!consumed;
}
} // namespace relay
