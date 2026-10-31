#pragma once
#include "imgui.h"
#include "core/KeyBindings.h"
namespace relay {
enum class MenuAction { None,Start,Resume,Gallery,Title,Quit,Checkpoint,Restart,Settings,Practice };
// The title screen or the pause menu (Esc).
enum class MenuMode { Title,Pause };
MenuAction drawMenu(MenuMode mode,int& level,float time,bool hasCheckpoint=false);
// The highlighted row of the title/pause menu and of the settings page (for menu sounds).
int menuSelection();
int settingsSelection();
void drawArrival(float time);
// Control settings: rebind keys. pressedKey is the first key pressed this frame (or -1);
// escape cancels a capture or leaves. Returns true when the player leaves.
bool drawSettings(KeyBindings& keys,float time,int pressedKey,bool escape,bool overGame);
// Blackout between levels: black is the cover, card the title card's opacity.
void drawLevelTransition(float black,float card,float progress,int level,float time);

// Holographic interface palette shared by menus and HUD.
namespace palette {
inline constexpr ImU32 navy=IM_COL32(4,16,30,255), glass=IM_COL32(6,24,44,255);
inline constexpr ImU32 cyan=IM_COL32(0,224,255,255), deepCyan=IM_COL32(0,150,210,255);
inline constexpr ImU32 ice=IM_COL32(214,248,255,255), steel=IM_COL32(112,162,192,255);
inline constexpr ImU32 amber=IM_COL32(255,176,32,255), danger=IM_COL32(255,64,96,255);
inline constexpr ImU32 mint=IM_COL32(60,255,200,255);
// Acrylic: light, faintly cool grey tints laid over the blurred scene (surface for large panels,
// slate for cards and tags), graphite text (graphiteSoft for secondary), shade for the soft shadows
// under them, glowCyan, the bright cyan that lights their edges, sheen and titles, and deepTeal for
// text that glows.
inline constexpr ImU32 surface=IM_COL32(218,227,232,255), slate=IM_COL32(238,244,247,255), shade=IM_COL32(4,14,30,255);
inline constexpr ImU32 glowCyan=IM_COL32(40,222,255,255), deepTeal=IM_COL32(22,112,138,255);
inline constexpr ImU32 graphite=IM_COL32(64,70,80,255), graphiteSoft=IM_COL32(112,118,128,255);
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
// Acrylic panel with the top-left and bottom-right corners cut: the fill's colour is the tint and
// its alpha the panel's opacity. The edge colour marks a short tick along the top edge, and thin
// graphite brackets sit in the two square corners.
void drawPanel(ImDrawList* d,ImVec2 a,ImVec2 b,float cut,ImU32 fill,ImU32 edge,float thickness);
void drawBrackets(ImDrawList* d,ImVec2 a,ImVec2 b,float length,ImU32 colour,float thickness);
void drawScanlines(ImDrawList* d,ImVec2 a,ImVec2 b,float spacing,ImU32 colour);
// Parallelogram leaning right by slant; used for bars and highlights.
void drawSlanted(ImDrawList* d,ImVec2 a,ImVec2 b,float slant,ImU32 left,ImU32 right);
// Surface finish shared by the panels: shades the vertices drawn since `start` from a lighter top
// to a deeper bottom (keeping their alpha), lays a dot-matrix texture with a slow diagonal sheen
// over the shape (corners cut by `cut`, right edge leaning in by `lean` at the bottom), and adds a
// thin highlight along the top edge.
void finishSurface(ImDrawList* d,int start,ImVec2 a,ImVec2 b,ImU32 fill,float cut=0,float lean=0);
// Acrylic backdrop: a blurred copy of the 3D scene, captured each frame just before the UI draws
// (AppUi::render); 0 until the first capture.
void setBackdrop(ImTextureID texture);
// Acrylic surfaces: a soft shadow, the blurred scene behind (backdrop scales it; 0 for none, as over
// the level transition's black), a light grey tint over it (lighter at the top), fine grain with a
// slow cyan sheen, a glowing cyan top edge, a faint cyan hairline and halo. opacity fades the whole
// surface. The outline must be convex.
void drawAcrylicPoly(ImDrawList* d,const ImVec2* points,int count,float opacity,ImU32 tint=palette::slate,float backdrop=1);
void drawAcrylicRect(ImDrawList* d,ImVec2 a,ImVec2 b,float rounding,float opacity,ImU32 tint=palette::slate,float backdrop=1);
// Juice, in the manner of a deck-builder's cards. transformSince turns and scales everything drawn
// into d since vertex `start` about c, then moves it by offset. punch is a scale that overshoots
// by `strength` and settles to 1; elasticIn grows from 0 to 1 with an overshoot over `duration`.
void transformSince(ImDrawList* d,int start,ImVec2 c,float angle,float scale,ImVec2 offset={0,0});
float punch(float age,float strength,float frequency=30,float decay=10);
float elasticIn(float age,float duration=.34F);
}
