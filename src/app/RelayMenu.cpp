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
MenuAction drawMenu(MenuMode mode,int& level,float,bool){
 const bool pause=mode==MenuMode::Pause;auto* d=ImGui::GetBackgroundDrawList();auto size=ImGui::GetIO().DisplaySize;float s=scale();
 if(mode!=lastMode){selected=0;lastMode=mode;}navigation(selected,4);
 const float x=size.x*.5F-290*s,top=size.y*.5F-235*s;
 center(d,top,58*s,ink,pause?"Paused":"Core Explorer Lite");
 center(d,top+78*s,26*s,muted,pause?"Game paused":"Collect cores and reach the exit");
 const std::string levelLabel=std::string("Level: ")+(level==0?"Tutorial Yard":"Obstacle Yard")+"   < >";
 const char* labels[]{pause?"Resume":"Start game",pause?"Restart":levelLabel.c_str(),"Settings",pause?"Main menu":"Quit"};
 int activated=-1;auto mouse=ImGui::GetIO().MousePos;bool moved=std::abs(mouse.x-lastMouse.x)+std::abs(mouse.y-lastMouse.y)>.5F;lastMouse=mouse;
 for(int i=0;i<4;++i){ImVec2 a{x,top+(120+i*65)*s},b{x+580*s,a.y+54*s};
  if(moved&&mouse.x>=a.x&&mouse.x<=b.x&&mouse.y>=a.y&&mouse.y<=b.y)selected=i;
  if(button(d,a,b,labels[i],selected==i,s))activated=i;
 }
 if(!pause&&selected==1&&(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)||ImGui::IsKeyPressed(ImGuiKey_RightArrow)||ImGui::IsKeyPressed(ImGuiKey_A)||ImGui::IsKeyPressed(ImGuiKey_D)))level=1-level;
 if(enter())activated=selected;
 center(d,top+425*s,23*s,pause?ink:muted,"W / S or arrows to select   |   Enter to confirm");
 if(activated==0)return pause?MenuAction::Resume:MenuAction::Start;
 if(activated==1){if(pause)return MenuAction::Restart;level=1-level;}
 if(activated==2){capture=-1;return MenuAction::Settings;}
 if(activated==3)return pause?MenuAction::Title:MenuAction::Quit;
 return MenuAction::None;
}
bool drawSettings(KeyBindings& keys,float,int pressedKey,bool escape,bool){
 auto* d=ImGui::GetBackgroundDrawList();auto size=ImGui::GetIO().DisplaySize;float s=scale();
 float x=size.x*.5F-400*s,top=50*s;
 center(d,top,48*s,ink,"Controls");
 center(d,top+60*s,25*s,muted,"Select an action to change its key");
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
