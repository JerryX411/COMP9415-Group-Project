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
constexpr ImU32 background=IM_COL32(229,231,234,255),panel=IM_COL32(247,248,249,255),ink=IM_COL32(43,48,54,255),muted=IM_COL32(100,107,115,255),border=IM_COL32(194,199,205,255),accent=IM_COL32(61,104,147,255);
float scale(){auto v=ImGui::GetIO().DisplaySize;return std::min(v.x/1440.F,v.y/900.F);}
void text(ImDrawList* d,float size,ImVec2 pos,ImU32 c,const char* t){d->AddText(face?face:ImGui::GetFont(),size,pos,c,t);}
void center(ImDrawList* d,float y,float size,ImU32 c,const char* t){auto* f=face?face:ImGui::GetFont();float w=f->CalcTextSizeA(size,1e6F,0,t).x;text(d,size,{(ImGui::GetIO().DisplaySize.x-w)*.5F,y},c,t);}
bool button(ImDrawList* d,ImVec2 a,ImVec2 b,const char* label,bool focus,float s){
 auto m=ImGui::GetIO().MousePos;bool hover=m.x>=a.x&&m.x<=b.x&&m.y>=a.y&&m.y<=b.y;
 d->AddRectFilled(a,b,focus||hover?IM_COL32(216,226,237,255):panel,3*s);
 d->AddRect(a,b,focus?accent:border,3*s,0,focus?2*s:1*s);
 text(d,25*s,{a.x+20*s,a.y+14*s},ink,label);
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
 auto* f=face?face:ImGui::GetFont();if(d)d->AddText(f,size,p,c,t);return f->CalcTextSizeA(size,1e6F,0,t).x;
}
void drawAcrylicRect(ImDrawList* d,ImVec2 a,ImVec2 b,float,float,ImU32,float){d->AddRectFilled(a,b,panel,3);d->AddRect(a,b,border,3);}
void drawSlanted(ImDrawList* d,ImVec2 a,ImVec2 b,float,ImU32 left,ImU32){d->AddRectFilled(a,b,left,1);}
int menuSelection(){return selected;} int settingsSelection(){return setting;}
MenuAction drawMenu(MenuMode mode,int& level,float,bool){
 const bool pause=mode==MenuMode::Pause;auto* d=ImGui::GetBackgroundDrawList();auto size=ImGui::GetIO().DisplaySize;float s=scale();
 if(mode!=lastMode){selected=0;lastMode=mode;}navigation(selected,4);
 if(!pause)d->AddRectFilled({0,0},size,background);
 else d->AddRectFilled({0,0},size,IM_COL32(30,35,40,155));
 const float x=size.x*.5F-230*s,top=size.y*.5F-235*s;
 if(pause){d->AddRectFilled({x-36*s,top-45*s},{x+496*s,top+410*s},background,5*s);}
 center(d,top,44*s,ink,pause?"Paused":"Core Explorer Lite");
 center(d,top+65*s,20*s,muted,pause?"Game paused":"Basic movement playground");
 const std::string levelLabel=std::string("Level: ")+(level==0?"Test Yard":"Obstacle Yard")+"   < >";
 const char* labels[]{pause?"Resume":"Start game",pause?"Restart":levelLabel.c_str(),"Settings",pause?"Main menu":"Quit"};
 int activated=-1;auto mouse=ImGui::GetIO().MousePos;bool moved=std::abs(mouse.x-lastMouse.x)+std::abs(mouse.y-lastMouse.y)>.5F;lastMouse=mouse;
 for(int i=0;i<4;++i){ImVec2 a{x,top+(120+i*65)*s},b{x+460*s,a.y+54*s};
  if(moved&&mouse.x>=a.x&&mouse.x<=b.x&&mouse.y>=a.y&&mouse.y<=b.y)selected=i;
  if(button(d,a,b,labels[i],selected==i,s))activated=i;
 }
 if(!pause&&selected==1&&(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)||ImGui::IsKeyPressed(ImGuiKey_RightArrow)||ImGui::IsKeyPressed(ImGuiKey_A)||ImGui::IsKeyPressed(ImGuiKey_D)))level=1-level;
 if(enter())activated=selected;
 center(d,top+405*s,17*s,pause?ink:muted,"W / S or arrows to select   |   Enter to confirm");
 if(activated==0)return pause?MenuAction::Resume:MenuAction::Start;
 if(activated==1){if(pause)return MenuAction::Restart;level=1-level;}
 if(activated==2){capture=-1;return MenuAction::Settings;}
 if(activated==3)return pause?MenuAction::Title:MenuAction::Quit;
 return MenuAction::None;
}
bool drawSettings(KeyBindings& keys,float,int pressedKey,bool escape,bool){
 auto* d=ImGui::GetBackgroundDrawList();auto size=ImGui::GetIO().DisplaySize;float s=scale();d->AddRectFilled({0,0},size,background);
 float x=size.x*.5F-300*s,top=50*s;center(d,top,38*s,ink,"Controls");
 center(d,top+53*s,18*s,muted,"Select an action to change its key");
 bool consumed=capture>=0;
 if(capture>=0){if(escape)capture=-1;else if(pressedKey>=0&&keys.assign(KeyAction(capture),pressedKey)){keys.save(settingsPath());capture=-1;}}
 else navigation(setting,KeyBindings::count+2);
 int activated=-1;
 for(int i=0;i<KeyBindings::count+2;++i){
  std::string label=i<KeyBindings::count?KeyBindings::actionName(KeyAction(i)):i==KeyBindings::count?"Reset defaults":"Back";
  if(i<KeyBindings::count)label+="   :   "+std::string(capture==i?"Press a key (Esc cancels)":KeyBindings::keyName(keys[KeyAction(i)]));
  ImVec2 a{x,top+(108+i*58)*s},b{x+600*s,a.y+48*s};
  if(button(d,a,b,label.c_str(),i==setting,s)&&capture<0&&!consumed){activated=i;setting=i;}
 }
 if(!consumed&&enter())activated=setting;
 if(activated>=0){if(activated<KeyBindings::count)capture=activated;else if(activated==KeyBindings::count){keys.reset();keys.save(settingsPath());}else return true;}
 return escape&&!consumed;
}
} // namespace relay
