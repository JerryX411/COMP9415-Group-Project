#include "app/RelayMenu.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
namespace relay {
MenuAction drawMenu(bool paused,int& level,float time) {
  const auto size=ImGui::GetIO().DisplaySize;
  const float s=std::min(size.x/1440.F,size.y/900.F),left=82*s,top=158*s;
  auto* draw=ImGui::GetBackgroundDrawList(); auto* font=ImGui::GetFont();
  const ImU32 ink=IM_COL32(30,42,49,255),muted=IM_COL32(91,105,110,255),blue=IM_COL32(31,110,157,255),line=IM_COL32(180,187,184,255);
  draw->AddRectFilled({0,0},{size.x*.46F,size.y},IM_COL32(235,236,228,248));
  draw->AddRectFilledMultiColor({size.x*.46F,0},{size.x*.60F,size.y},IM_COL32(235,236,228,248),IM_COL32(235,236,228,0),IM_COL32(235,236,228,0),IM_COL32(235,236,228,248));
  auto text=[&](float x,float y,float px,ImU32 c,const char* str) { draw->AddText(font,px*s,{x,y},c,str); };
  draw->AddRectFilled({left,46*s},{left+28*s,74*s},ink);
  draw->AddRectFilled({left+7*s,53*s},{left+21*s,67*s},IM_COL32(235,236,228,255));
  text(left+43*s,44*s,23,ink,"RELAY / RESEARCH SYSTEMS");
  text(left+43*s,70*s,12,muted,"AUTONOMOUS RECOVERY PROGRAMME");
  draw->AddLine({left,112*s},{left+440*s,112*s},line);
  text(left,top,15,blue,paused?"SESSION SUSPENDED":"TEST FACILITY     /     SERIES 07");
  text(left-3*s,top+30*s,91,ink,"VOXEL");
  text(left-3*s,top+110*s,91,ink,"RELAY");
  text(left,top+222*s,22,ink,paused?"Ready when you are.":"A small machine. A silent station.");
  text(left,top+254*s,16,muted,"Recover the cores. Reconnect the facility.");
  const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
  ImGui::SetNextWindowPos({left,top+301*s}); ImGui::SetNextWindowSize({444*s,285*s});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0}); ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{14*s,9*s}); ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*s,10*s});
  ImGui::PushStyleColor(ImGuiCol_Button,{.82F,.84F,.82F,1}); ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.72F,.79F,.80F,1});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.62F,.73F,.77F,1}); ImGui::PushStyleColor(ImGuiCol_Text,{.12F,.17F,.20F,1});
  ImGui::Begin("Facility navigation",nullptr,flags); ImGui::PushFont(nullptr,20*s);
  MenuAction action=MenuAction::None;
  ImGui::PushStyleColor(ImGuiCol_Button,{.10F,.34F,.47F,1}); ImGui::PushStyleColor(ImGuiCol_Text,{.96F,.97F,.94F,1});
  if(ImGui::Button(paused?"RESUME SESSION                     [ENTER]":"BEGIN REACTIVATION                 [ENTER]",{430*s,56*s})) action=paused?MenuAction::Resume:MenuAction::Start;
  ImGui::PopStyleColor(2);
  if(!paused) {
    ImGui::PushFont(nullptr,17*s);
    for(int i=0;i<2;++i) {
      if(i) ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button,level==i?ImVec4(.66F,.75F,.78F,1):ImVec4(.86F,.87F,.84F,1));
      if(ImGui::Button(i==0?"01  /  COURTYARD":"02  /  CORRIDORS",{210*s,38*s})) level=i;
      ImGui::PopStyleColor();
    }
    ImGui::PopFont();
  }
  if(ImGui::Button("MOTION LAB",{430*s,42*s})) action=MenuAction::Gallery;
  if(paused&&ImGui::Button("RETURN TO MAIN MENU",{430*s,42*s})) action=MenuAction::Title;
  if(ImGui::Button("EXIT",{430*s,34*s})) action=MenuAction::Quit;
  ImGui::PopFont(); ImGui::End(); ImGui::PopStyleColor(4); ImGui::PopStyleVar(4);
  const float labelX=size.x*.70F,labelY=size.y*.80F;
  draw->AddRectFilled({labelX-18*s,labelY-12*s},{labelX+270*s,labelY+84*s},IM_COL32(24,34,40,230));
  text(labelX,labelY,14,IM_COL32(130,191,215,255),"AUTONOMOUS COURIER");
  text(labelX,labelY+23*s,35,IM_COL32(242,242,229,255),"UNIT 07");
  draw->AddCircleFilled({labelX+230*s,labelY+36*s},4*s,IM_COL32(235,166,72,int(190+50*std::sin(time*2))));
  draw->AddLine({left,size.y-72*s},{left+440*s,size.y-72*s},line);
  text(left,size.y-53*s,14,muted,"SYSTEM READY   /   FIVE ARMOUR CELLS");
  text(size.x-260*s,46*s,15,IM_COL32(221,231,230,255),"FACILITY BUILD  /  0.6");
  return action;
}
void drawArrival(float time) {
  const auto size=ImGui::GetIO().DisplaySize; auto* d=ImGui::GetForegroundDrawList(); auto* font=ImGui::GetFont();
  const float s=std::min(size.x/1440.F,size.y/900.F);
  const float fade=std::clamp((4.F-time)/.5F,0.F,1.F);
  const auto white=IM_COL32(232,239,237,int(255*fade)),blue=IM_COL32(134,203,228,int(255*fade));
  d->AddRectFilled({0,0},{size.x,72*s*fade},IM_COL32(10,16,20,245));
  d->AddRectFilled({0,size.y-100*s*fade},size,IM_COL32(10,16,20,245));
  if(time<.55F) d->AddRectFilled({0,0},size,IM_COL32(7,12,16,int(220*(1-time/.55F))));
  d->AddText(font,18*s,{45*s,25*s},blue,"RELAY RESEARCH  /  COURIER REACTIVATION");
  const char* message=time<.65F?"UNIT 07  //  POWER RESTORED":time<1.35F?"OPTICS ONLINE":time<2.25F?"MOTOR CALIBRATION COMPLETE":"COURIER READY. RESTORE THE SIGNAL.";
  d->AddText(font,24*s,{45*s,size.y-73*s},white,message);
  d->AddText(font,15*s,{size.x-240*s,size.y-49*s},blue,"ENTER / SPACE  TO SKIP");
}
}
