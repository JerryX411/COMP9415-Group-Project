#include "app/Minimap.h"

#include "core/RelayGame.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace craft::app {
namespace {

constexpr int sides = 64;
constexpr float worldRadius = 14.F;
constexpr ImU32 frame = IM_COL32(15, 18, 22, 255);
constexpr ImU32 empty = IM_COL32(213, 221, 229, 255);
constexpr ImU32 floor = IM_COL32(241, 244, 247, 255);
constexpr ImU32 wall = IM_COL32(164, 176, 187, 255);
constexpr ImU32 coreColour = IM_COL32(0, 156, 171, 255);
constexpr ImU32 controlColour = IM_COL32(194, 108, 25, 255);
constexpr ImU32 playerColour = IM_COL32(20, 108, 240, 255);

ImVec2 plus(ImVec2 a, ImVec2 b) { return {a.x+b.x,a.y+b.y}; }
ImVec2 times(ImVec2 v, float s) { return {v.x*s,v.y*s}; }
float lengthSquared(ImVec2 v) { return v.x*v.x+v.y*v.y; }

using Polygon = std::array<ImVec2,sides+4>;

const std::array<ImVec2,sides>& unitCircle() {
  static const auto points=[] {
    std::array<ImVec2,sides> result{};
    for (int i=0;i<sides;++i) {
      const float angle=6.283185307F*float(i)/sides;
      result[i]={std::cos(angle),std::sin(angle)};
    }
    return result;
  }();
  return points;
}

// Clip the circle polygon to a tile's four edges. ImGui's clip rectangle alone
// cannot keep squares at the edge from spilling outside a circular map.
int clipEdge(const Polygon& from,int count,Polygon& to,int axis,float edge,bool minimum) {
  if (!count) return 0;
  const auto distance=[&](ImVec2 p) {
    const float delta=(axis==0?p.x:p.y)-edge;
    return minimum?delta:-delta;
  };
  int written=0;
  ImVec2 prev=from[count-1];
  float prevDist=distance(prev);
  for(int i=0;i<count;++i) {
    const auto next=from[i];
    const float nextDist=distance(next);
    if ((prevDist>=0)!=(nextDist>=0)) {
      const float t=prevDist/(prevDist-nextDist);
      to[written++]={prev.x+(next.x-prev.x)*t,prev.y+(next.y-prev.y)*t};
    }
    if(nextDist>=0)to[written++]=next;
    prev=next; prevDist=nextDist;
  }
  return written;
}

void drawTile(ImDrawList& draw,ImVec2 centre,float radius,const Polygon& disk,
              ImVec2 lo,ImVec2 hi,ImU32 colour) {
  const float x=std::max(std::abs(lo.x-centre.x),std::abs(hi.x-centre.x));
  const float y=std::max(std::abs(lo.y-centre.y),std::abs(hi.y-centre.y));
  // Use the circle polygon's inscribed radius for the fast interior case.
  const float inside=radius*.9987F;
  if(x*x+y*y<=inside*inside) { draw.AddRectFilled(lo,hi,colour); return; }
  const ImVec2 nearest{std::clamp(centre.x,lo.x,hi.x)-centre.x,
                       std::clamp(centre.y,lo.y,hi.y)-centre.y};
  if(lengthSquared(nearest)>=radius*radius)return;
  Polygon a{},b{};
  int n=clipEdge(disk,sides,a,0,lo.x,true);
  n=clipEdge(a,n,b,0,hi.x,false);
  n=clipEdge(b,n,a,1,lo.y,true);
  n=clipEdge(a,n,b,1,hi.y,false);
  if(n>=3)draw.AddConvexPolyFilled(b.data(),n,colour);
}

void diamond(ImDrawList& d,ImVec2 p,float r,ImU32 colour) {
  const ImVec2 a{p.x,p.y-r},b{p.x+r,p.y},c{p.x,p.y+r},e{p.x-r,p.y};
  d.AddQuadFilled(a,b,c,e,colour);
  d.AddQuad(a,b,c,e,frame);
}

void directionCue(ImDrawList& d,ImVec2 centre,ImVec2 delta,float distance,float s,ImU32 colour) {
  const float len2=lengthSquared(delta);
  if(len2<.0001F)return;
  const auto forward=times(delta,1.F/std::sqrt(len2));
  const ImVec2 side{-forward.y,forward.x};
  const auto tip=plus(centre,times(forward,distance));
  const auto base=plus(tip,times(forward,-8*s));
  const auto a=plus(base,times(side,4*s)),b=plus(base,times(side,-4*s));
  d.AddTriangleFilled(tip,a,b,colour);
  d.AddTriangle(tip,a,b,frame);
}

// Paint a soft sector behind the map's item markers.
void drawHeadingFan(ImDrawList& draw, ImVec2 centre, float yaw, float scale) {
  constexpr float pi = 3.14159265F;
  constexpr float halfAngle = pi / 6.F;
  const float radius = 42.F * scale;
  // Robot yaw 0 faces world -Z, which is up on the map.
  const float angle = -pi / 2.F - yaw;
  const int firstVertex = draw.VtxBuffer.Size;
  draw.PathLineTo(centre);
  draw.PathArcTo(centre, radius, angle - halfAngle, angle + halfAngle, 20);
  draw.PathFillConvex(playerColour);

  // Fade from the dot to the arc without replacing the antialiasing alpha.
  for (int i = firstVertex; i < draw.VtxBuffer.Size; ++i) {
    auto& vertex = draw.VtxBuffer[i];
    const ImVec2 offset{vertex.pos.x - centre.x, vertex.pos.y - centre.y};
    const float opacity = .72F * std::max(0.F, 1.F - std::sqrt(lengthSquared(offset)) / radius);
    const auto alpha = static_cast<ImU32>((vertex.col >> IM_COL32_A_SHIFT) * opacity);
    vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
  }
}

} // namespace

bool MinimapCircle::overlaps(ImVec2 point,float padding) const {
  const float r=radius+padding;
  return radius>0 && lengthSquared({point.x-centre.x,point.y-centre.y})<r*r;
}

MinimapCircle drawHeliosMinimap(ImDrawList& draw,const relay::Game& game,
                               ImVec2 display,float scale) {
  if(scale<=0)return {};
  const float radius=106.F*scale;
  const ImVec2 centre{display.x-48.F*scale-radius,(132.F+106.F)*scale};
  const auto& player=game.player();
  const auto& world=game.world();
  const int layer=std::clamp(int(std::floor(game.shadowHeight()+.03F)),1,relay::VoxelWorld::size-1);
  const float unit=(radius-1.4F*scale)/worldRadius;
  const float rim=radius-1.4F*scale;
  const auto delta=[&](glm::vec3 p) {
    return ImVec2{(p.x-player.position.x)*unit,(p.z-player.position.z)*unit};
  };
  const auto at=[&](float x,float z) { return plus(centre,delta({x,0,z})); };
  const auto sameFloor=[&](float height) { return std::abs(height-float(layer))<.55F; };
  const auto fits=[&](ImVec2 v) { const float r=radius-11.F*scale; return lengthSquared(v)<=r*r; };

  draw.AddCircleFilled(centre,radius,empty,sides);
  Polygon disk{};
  for(int i=0;i<sides;++i)disk[i]=plus(centre,times(unitCircle()[i],rim));
  const int x0=std::max(0,int(std::floor(player.position.x-worldRadius)));
  const int x1=std::min(relay::VoxelWorld::size,int(std::ceil(player.position.x+worldRadius)));
  const int z0=std::max(0,int(std::floor(player.position.z-worldRadius)));
  const int z1=std::min(relay::VoxelWorld::size,int(std::ceil(player.position.z+worldRadius)));
  // Show one horizontal slice. Empty space below an elevated platform stays muted.
  // Adjacent equal-colour tiles are drawn as row strips to avoid internal seams.
  const auto terrain=[&](int x,int z)->ImU32 {
    if(world.cell(x,layer,z))return wall;
    return world.cell(x,layer-1,z)?floor:empty;
  };
  // Internal tile edges are shared, so anti-aliased fringes would create seams.
  const auto previousFlags=draw.Flags;
  draw.Flags &= ~ImDrawListFlags_AntiAliasedFill;
  for(int z=z0;z<z1;++z)for(int x=x0;x<x1;) {
    const auto colour=terrain(x,z);
    const int begin=x++;
    while(x<x1&&terrain(x,z)==colour)++x;
    if(colour!=empty)drawTile(draw,centre,rim,disk,at(float(begin),float(z)),at(float(x),float(z+1)),colour);
  }
  // Dynamic door panels use the same bounds as gameplay collision.
  for(const auto& door:game.doors())for(const auto& panel:door.panels()) {
    if(panel.min.y>float(layer)+.7F||panel.max.y<=float(layer)+.15F)continue;
    drawTile(draw,centre,rim,disk,at(panel.min.x,panel.min.z),at(panel.max.x,panel.max.z),IM_COL32(109,124,137,255));
  }

  draw.Flags=previousFlags;
  drawHeadingFan(draw, centre, player.yaw, scale);

  bool nearbyCore=false;
  ImVec2 nearestCore{};
  float coreDistance=0;
  for(const auto& core:game.cores()) {
    if(core.collected)continue;
    const auto offset=delta(core.position);
    const float d2=lengthSquared(offset);
    if(coreDistance==0||d2<coreDistance) { nearestCore=offset; coreDistance=d2; }
    // A core model floats .8 world units above its supporting floor.
    if(!sameFloor(core.position.y-.8F)||!fits(offset))continue;
    nearbyCore=true;
    diamond(draw,plus(centre,offset),4.9F*scale,coreColour);
  }
  const auto control=[&](glm::vec3 position) {
    const auto offset=delta(position);
    if(!sameFloor(position.y)||!fits(offset))return;
    const auto p=plus(centre,offset);
    const float r=3.6F*scale;
    draw.AddRectFilled({p.x-r,p.y-r},{p.x+r,p.y+r},controlColour,scale);
    draw.AddRect({p.x-r,p.y-r},{p.x+r,p.y+r},frame,scale);
  };
  for(const auto& plate:game.plates())control(plate.position);
  for(const auto& sw:game.switches())control(sw.position);
  for(const auto& portal:game.portals())for(const auto endpoint:{portal.from,portal.to}) {
    const auto offset=delta(endpoint);
    if(!sameFloor(endpoint.y)||!fits(offset))continue;
    const auto p=plus(centre,offset);
    const float r=4.7F*scale;
    draw.AddCircle(p,r,frame,16,2.8F*scale);
    draw.AddCircle(p,r,portal.linked()?IM_COL32(96,53,216,255):IM_COL32(151,105,221,255),16,1.65F*scale);
  }

  const auto gate=game.gatePosition();
  const auto exitOffset=delta(gate);
  const auto exitColour=game.gateUnlocked()?IM_COL32(25,166,91,255):IM_COL32(244,178,41,255);
  if(sameFloor(gate.y)&&fits(exitOffset)) {
    const auto p=plus(centre,exitOffset);
    draw.AddRectFilled({p.x-4*scale,p.y-5*scale},{p.x+4*scale,p.y+5*scale},exitColour,scale);
    draw.AddRect({p.x-4*scale,p.y-5*scale},{p.x+4*scale,p.y+5*scale},frame,scale);
    draw.AddCircleFilled({p.x+1.8F*scale,p.y},.9F*scale,frame,8);
  } else {
    directionCue(draw,centre,exitOffset,radius-5*scale,scale,exitColour);
  }
  if(!nearbyCore&&coreDistance>0)
    directionCue(draw,centre,nearestCore,radius-15*scale,scale,coreColour);

  draw.AddCircleFilled(centre,6.2F*scale,IM_COL32(255,255,255,255),20);
  draw.AddCircleFilled(centre,4.7F*scale,playerColour,20);
  draw.AddCircle(centre,radius,frame,sides,1.45F*scale);
  return {centre,radius};
}

} // namespace craft::app
