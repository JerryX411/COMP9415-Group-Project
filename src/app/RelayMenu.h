#pragma once
namespace relay {
enum class MenuAction { None,Start,Resume,Gallery,Title,Quit };
MenuAction drawMenu(bool paused,int& level,float time);
void drawArrival(float time);
}
