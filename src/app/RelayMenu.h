#pragma once
#include "imgui.h"
#include "core/KeyBindings.h"
namespace relay {
enum class MenuAction { None,Start,Resume,Gallery,Title,Quit,Checkpoint,Restart,Settings };
// The title screen or the pause menu (Esc).
enum class MenuMode { Title,Pause };
MenuAction drawMenu(MenuMode mode,int& level,float time,bool hasCheckpoint=false);
void drawArrival(float time);
// Control settings: rebind keys. pressedKey is the first key pressed this frame (or -1);
// escape cancels a capture or leaves. Returns true when the player leaves.
bool drawSettings(KeyBindings& keys,float time,int pressedKey,bool escape,bool overGame);
// Blackout between levels: black is the cover, card the title card's opacity.
void drawLevelTransition(float black,float card,float progress,int level,const char* name,float time);

// Holographic interface palette shared by menus and HUD.
namespace palette {
inline constexpr ImU32 navy=IM_COL32(4,16,30,255), glass=IM_COL32(6,24,44,255);
inline constexpr ImU32 cyan=IM_COL32(0,224,255,255), deepCyan=IM_COL32(0,150,210,255);
inline constexpr ImU32 ice=IM_COL32(214,248,255,255), steel=IM_COL32(112,162,192,255);
inline constexpr ImU32 amber=IM_COL32(255,176,32,255), danger=IM_COL32(255,64,96,255);
inline constexpr ImU32 mint=IM_COL32(60,255,200,255);
}
ImU32 withAlpha(ImU32 colour,float alpha);
// Display face: a condensed bold system font when available, else the body font.
void setDisplayFont(ImFont* font);
// Rasterises the printable glyphs at every UI size up front (call inside a frame).
void prewarmFonts(float scale);
ImFont* displayFont();
// Tracked capitals; display text uses the display face. Returns the drawn width;
// a null draw list only measures.
float drawTracked(ImDrawList* d,float size,ImVec2 p,ImU32 colour,const char* text,float tracking,bool display=false);
// Display text with a soft halo in the glow colour.
float drawGlow(ImDrawList* d,float size,ImVec2 p,ImU32 colour,ImU32 glow,const char* text,float tracking);
float drawKeycap(ImDrawList* d,float scale,ImVec2 p,ImU32 colour,const char* key);
// Glass panel with the top-left and bottom-right corners cut, plus an edge line.
void drawPanel(ImDrawList* d,ImVec2 a,ImVec2 b,float cut,ImU32 fill,ImU32 edge,float thickness);
void drawBrackets(ImDrawList* d,ImVec2 a,ImVec2 b,float length,ImU32 colour,float thickness);
void drawScanlines(ImDrawList* d,ImVec2 a,ImVec2 b,float spacing,ImU32 colour);
// Parallelogram leaning right by slant; used for bars and highlights.
void drawSlanted(ImDrawList* d,ImVec2 a,ImVec2 b,float slant,ImU32 left,ImU32 right);
}
