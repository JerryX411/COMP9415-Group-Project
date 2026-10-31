#include "graphics/RelayRenderer.h"
#include "core/AssetPath.h"
#include "graphics/OpenGL.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <glm/gtc/matrix_transform.hpp>

namespace relay {
namespace {
struct Vertex { glm::vec3 p, n; glm::vec2 uv; glm::vec3 tangent, colour; };
const std::array<glm::ivec3, 6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
void face(std::vector<Vertex>& vertices, glm::vec3 centre, glm::vec3 normal, glm::vec3 colour) {
  const auto tangent = std::abs(normal.y) > .5F ? glm::vec3(1,0,0) : glm::normalize(glm::cross(glm::vec3(0,1,0), normal));
  const auto bitangent = glm::cross(normal, tangent);
  const glm::vec2 corners[4]{{0,0},{1,0},{1,1},{0,1}};
  for (int index : {0,1,2,0,2,3}) {
    auto uv = corners[index];
    vertices.push_back({centre + normal * .5F + tangent * (uv.x-.5F) + bitangent * (uv.y-.5F), normal, uv, tangent, colour});
  }
}
void meshAttributes(unsigned vao, unsigned vertices, unsigned instances, std::size_t base=0) {
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vertices);
  const std::size_t offsets[]{offsetof(Vertex,p),offsetof(Vertex,n),offsetof(Vertex,uv),offsetof(Vertex,tangent),offsetof(Vertex,colour)};
  for (unsigned i=0; i<5; ++i) {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, i==2?2:3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsets[i]));
  }
  glBindBuffer(GL_ARRAY_BUFFER, instances);
  for (unsigned i=0; i<4; ++i) {
    glEnableVertexAttribArray(5+i);
    glVertexAttribPointer(5+i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(base+i*sizeof(glm::vec4)));
    glVertexAttribDivisor(5+i,1);
  }
  for (unsigned i=0; i<2; ++i) {
    glEnableVertexAttribArray(9+i);
    glVertexAttribPointer(9+i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(base+(i==0?offsetof(Instance,colour):offsetof(Instance,material))));
    glVertexAttribDivisor(9+i,1);
  }
  glBindVertexArray(0);
}
unsigned texture(const std::vector<unsigned char>& data) {
  unsigned id; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,64,64,0,GL_RGB,GL_UNSIGNED_BYTE,data.data());
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glGenerateMipmap(GL_TEXTURE_2D); return id;
}
// Shared bevelled primitive: six face panels, twelve edge strips and eight corners.
// Voxel terrain stays cubic; articulated parts gain a readable machined silhouette.
std::vector<Vertex> bevelledBox() {
  std::vector<Vertex> result;
  const float a=.42F,b=.5F;
  auto triangle=[&](glm::vec3 p,glm::vec3 q,glm::vec3 r,glm::vec3 n) {
    if(glm::dot(glm::cross(q-p,r-p),n)<0) std::swap(q,r);
    const auto tangent=std::abs(n.y)>.95F?glm::vec3(1,0,0):glm::normalize(glm::cross(glm::vec3(0,1,0),n));
    const auto bitangent=glm::cross(n,tangent);
    for(auto v:{p,q,r}) result.push_back({v,n,{glm::dot(v,tangent)+.5F,glm::dot(v,bitangent)+.5F},tangent,{1,1,1}});
  };
  auto quad=[&](glm::vec3 p,glm::vec3 q,glm::vec3 r,glm::vec3 s,glm::vec3 n) { triangle(p,q,r,n); triangle(p,r,s,n); };
  for(auto dir:directions) {
    const glm::vec3 n(dir),t=std::abs(n.y)>.5F?glm::vec3(1,0,0):glm::normalize(glm::cross(glm::vec3(0,1,0),n)),v=glm::cross(n,t);
    quad(n*b-t*a-v*a,n*b+t*a-v*a,n*b+t*a+v*a,n*b-t*a+v*a,n);
  }
  for(int axis=0;axis<3;++axis) for(float s:{-1.F,1.F}) for(float t:{-1.F,1.F}) {
    const int j=(axis+1)%3,k=(axis+2)%3;
    glm::vec3 p(0),q(0),r(0),v(0),n(0);
    p[axis]=-a;q[axis]=a;r[axis]=a;v[axis]=-a;
    p[j]=q[j]=s*b;p[k]=q[k]=t*a;r[j]=v[j]=s*a;r[k]=v[k]=t*b;n[j]=s;n[k]=t;
    quad(p,q,r,v,glm::normalize(n));
  }
  for(float x:{-1.F,1.F}) for(float y:{-1.F,1.F}) for(float z:{-1.F,1.F})
    triangle({x*b,y*a,z*a},{x*a,y*b,z*a},{x*a,y*a,z*b},glm::normalize(glm::vec3(x,y,z)));
  return result;
}
std::vector<Vertex> roundPrimitive(bool cylinder,int slices=24,int stacks=14) {
  std::vector<Vertex> v;
  constexpr float pi=3.14159265F;
  auto emit=[&](glm::vec3 p,glm::vec3 n,glm::vec2 uv) {
    auto t=glm::vec3(-std::sin(uv.x*2*pi),0,std::cos(uv.x*2*pi));
    v.push_back({p,n,uv,t,{1,1,1}});
  };
  auto point=[&](float u,float w) {
    const float theta=u*2*pi, phi=w*pi;
    glm::vec3 n=cylinder?glm::vec3(std::cos(theta),0,std::sin(theta)):glm::vec3(std::sin(phi)*std::cos(theta),std::cos(phi),std::sin(phi)*std::sin(theta));
    auto p=n*.5F; if(cylinder) p.y=.5F-w;
    return std::pair<glm::vec3,glm::vec3>{p,n};
  };
  const int rows=cylinder?1:stacks;
  for(int y=0;y<rows;++y) for(int x=0;x<slices;++x) {
    const glm::vec2 uv[4]{{float(x)/slices,float(y)/rows},{float(x+1)/slices,float(y)/rows},{float(x+1)/slices,float(y+1)/rows},{float(x)/slices,float(y+1)/rows}};
    for(int i:{0,1,2,0,2,3}) { const auto [p,n]=point(uv[i].x,uv[i].y); emit(p,n,uv[i]); }
  }
  if(cylinder) for(float side:{-1.F,1.F}) for(int i=0;i<slices;++i) {
    const float a=i*2*pi/slices,b=(i+1)*2*pi/slices;
    glm::vec3 p{0,side*.5F,0},q{std::cos(a)*.5F,side*.5F,std::sin(a)*.5F},r{std::cos(b)*.5F,side*.5F,std::sin(b)*.5F};
    if(side>0) std::swap(q,r);
    for(auto pt:{p,q,r}) v.push_back({pt,{0,side,0},{pt.x+.5F,pt.z+.5F},{1,0,0},{1,1,1}});
  }
  return v;
}
float heightAt(int x, int y) {
  x = (x+64)%64; y = (y+64)%64;
  return (x<1 || y<1 ? -.055F : .0F) + .0015F*std::sin(float(x)*.8F)*std::sin(float(y)*.6F);
}
}

Renderer::Renderer() : shader_(craft::core::assetPath("shaders/relay.vert"), craft::core::assetPath("shaders/relay.frag")) {
  glGenVertexArrays(1,&cubeVao_); glGenVertexArrays(1,&terrainVao_);
  glGenBuffers(1,&cubeBuffer_); glGenBuffers(1,&instanceBuffer_);
  glGenBuffers(1,&terrainBuffer_); glGenBuffers(1,&terrainInstance_);
  auto cube=bevelledBox();
  meshStart_[0]=0; meshCount_[0]=int(cube.size());
  for(int i=1;i<3;++i) { auto mesh=roundPrimitive(i==2); meshStart_[i]=int(cube.size()); meshCount_[i]=int(mesh.size()); cube.insert(cube.end(),mesh.begin(),mesh.end()); }
  std::vector<Vertex> flat; for(auto n:directions) face(flat,glm::vec3(0),glm::vec3(n),{1,1,1});
  meshStart_[3]=int(cube.size()); meshCount_[3]=int(flat.size()); cube.insert(cube.end(),flat.begin(),flat.end());
  auto blob=roundPrimitive(false,10,6);  // 120 triangles instead of 672.
  meshStart_[4]=int(cube.size()); meshCount_[4]=int(blob.size()); cube.insert(cube.end(),blob.begin(),blob.end());
  modelVertices_=int(cube.size());
  glBindBuffer(GL_ARRAY_BUFFER,cubeBuffer_);
  glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(cube.size()*sizeof(Vertex)),cube.data(),GL_STATIC_DRAW);
  const Instance identity;
  glBindBuffer(GL_ARRAY_BUFFER,terrainInstance_); glBufferData(GL_ARRAY_BUFFER,sizeof(Instance),&identity,GL_STATIC_DRAW);
  meshAttributes(cubeVao_,cubeBuffer_,instanceBuffer_);
  meshAttributes(terrainVao_,terrainBuffer_,terrainInstance_);
  std::vector<unsigned char> colour(64*64*3), normals(colour.size());
  for (int y=0; y<64; ++y) for(int x=0; x<64; ++x) {
    const int i=(y*64+x)*3;
    const int value = x<1||y<1 ? 156 : 242+(x*13+y*7)%5;
    for(int c=0;c<3;++c) colour[std::size_t(i+c)] = static_cast<unsigned char>(value);
    const auto n=glm::normalize(glm::vec3(-(heightAt(x+1,y)-heightAt(x-1,y))*5, -(heightAt(x,y+1)-heightAt(x,y-1))*5,1));
    for(int c=0;c<3;++c) normals[std::size_t(i+c)] = static_cast<unsigned char>((n[c]*.5F+.5F)*255);
  }
  albedo_=texture(colour); normal_=texture(normals);
  instances_.reserve(1024);
}
Renderer::~Renderer() {
  glDeleteVertexArrays(1,&cubeVao_); glDeleteVertexArrays(1,&terrainVao_);
  for (unsigned b : {cubeBuffer_,instanceBuffer_,terrainBuffer_,terrainInstance_}) glDeleteBuffers(1,&b);
  glDeleteTextures(1,&albedo_); glDeleteTextures(1,&normal_);
}
void Renderer::rebuildTerrain(const Game& game, bool optimised, bool whitebox) {
  std::vector<Vertex> vertices;
  const auto& world=game.world();
  stats_.voxels=world.solidCount(); stats_.totalFaces=stats_.voxels*6; stats_.visibleFaces=0;
  vertices.reserve(std::size_t(stats_.totalFaces)*6);
  for(int y=0;y<VoxelWorld::size;++y) for(int z=0;z<VoxelWorld::size;++z) for(int x=0;x<VoxelWorld::size;++x) {
    const auto material=world.cell(x,y,z); if(!material) continue;
    glm::vec3 colour = y==0 ? glm::vec3(.65F,.71F,.72F) : glm::vec3(.83F,.86F,.85F);
    if (!whitebox) colour = y==0 ? glm::vec3(.36F,.39F,.40F) : glm::vec3(.84F,.83F,.78F);
    if (material==3) colour = whitebox ? glm::vec3(.72F,.73F,.64F) : glm::vec3(.63F,.65F,.63F);
    for(auto n: directions) {
      const bool exposed = world.cell(x+n.x,y+n.y,z+n.z)==0;
      if(exposed) ++stats_.visibleFaces;
      if(!optimised || exposed) {
        auto c=colour;
        if(!whitebox) {
          if(y==0&&((x/4+z/4)%2)) c*=.83F;
          if(y>0&&y==1&&material==2) c={.18F,.21F,.23F};
          if(y>0&&material==2&&(x%7==0||z%7==0)) c*=.8F;
        }
        face(vertices,glm::vec3(x+.5F,y+.5F,z+.5F),glm::vec3(n),c);
      }
    }
  }
  terrainVertices_=int(vertices.size());
  glBindBuffer(GL_ARRAY_BUFFER,terrainBuffer_);
  glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex)),vertices.data(),GL_STATIC_DRAW);
}
void Renderer::prepare(const Game& game, const RenderOptions& options, float galleryTime) {
  sceneGame_=&game; menuTime_=galleryTime;
  if(revision_!=game.revision() || lastOptimised_!=options.optimised || lastWhitebox_!=options.whitebox) {
    rebuildTerrain(game,options.optimised,options.whitebox);
    revision_=game.revision(); lastOptimised_=options.optimised; lastWhitebox_=options.whitebox;
  }
  instances_.clear();
  if(options.menu) models_.appendMenu(galleryTime,instances_,options.menuLook);
  else if(options.gallery) models_.appendGallery(galleryTime,instances_,options.motionPreview);
  else models_.append(game,instances_,options.wakeTime);
  playerPosition_=game.player().position;
  lightCount_=0; if(!options.menu&&!options.gallery) gatherLights(game);
  const auto group=[](const Instance& i) { return int(i.blend)*primitives+int(i.primitive); };
  std::stable_sort(instances_.begin(),instances_.end(),[&](const auto& a,const auto& b) { return group(a)<group(b); });
  instanceCount_.fill(0); for(const auto& i:instances_) ++instanceCount_[std::size_t(group(i))];
  instanceStart_[0]=0; for(int i=1;i<groups;++i) instanceStart_[i]=instanceStart_[i-1]+instanceCount_[i-1];
  glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer_);
  glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(instances_.size()*sizeof(Instance)),instances_.data(),GL_STREAM_DRAW);
  stats_.instances=int(instances_.size());
  stats_.submittedTriangles=options.gallery||options.menu?0:terrainVertices_/3;
  stats_.drawCalls=options.gallery||options.menu?0:1;
  for(int i=0;i<groups;++i) if(instanceCount_[i]) { stats_.submittedTriangles+=instanceCount_[i]*meshCount_[i%primitives]/3; ++stats_.drawCalls; }
}
void Renderer::draw(int width, int height, const RenderOptions& options, const FollowCamera& camera) {
  const float aspect=float(width)/float(height);
  glm::vec3 eye=camera.eye();
  glm::vec3 fog(.28F,.34F,.38F); float fogDensity=.006F;
  if(options.customView) {
    eye=options.viewEye;
    viewProjection_=glm::perspective(glm::radians(50.F),aspect,.1F,300.F)*glm::lookAt(eye,options.viewTarget,{0,1,0});
    fogDensity=.003F;
  } else if(options.menu) {
    // A slow, low dolly keeps the chamber alive; the courier sits on the right third.
    const float t=menuTime_;
    const glm::vec3 target(1.05F+.10F*std::sin(t*.11F),1.55F,.4F);
    eye=target+glm::vec3(-3.4F+.35F*std::sin(t*.07F),.55F+.12F*std::sin(t*.09F),10.6F);
    // Keep the courier's screen position stable across window shapes.
    const float fov=glm::radians(aspect<1.45F?44.F:36.F);
    viewProjection_=glm::perspective(fov,aspect,.1F,120.F)*glm::lookAt(eye,target,{0,1,0});
    fog={.88F,.90F,.91F}; fogDensity=.022F;
  } else if(options.gallery||options.overview) {
    // The overview frames the whole room (the training ground is about twice a mission's size).
    const Bounds room=sceneGame_?sceneGame_->room():Bounds{{4,1,4},{28,4,28}};
    const float k=std::max(room.max.x-room.min.x,room.max.z-room.min.z)/24.F;
    const glm::vec3 target=options.gallery?glm::vec3(0,options.motionPreview==9?4.F:1.6F,0):glm::vec3((room.min.x+room.max.x)*.5F,0,(room.min.z+room.max.z)*.5F);
    eye=target+(options.gallery?glm::vec3(3,2.3F,8):glm::vec3(24,30,28)*k);
    const float halfHeight=options.gallery?std::max(options.motionPreview==9?5.6F:2.75F,3.3F/aspect):std::max(17.6F,21.7F/aspect)*k;
    viewProjection_=glm::ortho(-halfHeight*aspect,halfHeight*aspect,-halfHeight,halfHeight,.1F,300.F)*glm::lookAt(eye,target,{0,1,0});
  } else if(options.wakeTime>=0) {
    const float t=options.wakeTime;
    auto ease=[](float x) { x=std::clamp(x,0.F,1.F); return x*x*(3-2*x); };
    const float orbit=ease((t-2.25F)/1.75F),yaw=glm::mix(3.55F,camera.yaw(),orbit);
    const float pitch=glm::mix(.07F,camera.pitch(),orbit),distance=glm::mix(2.9F,camera.distance(),orbit);
    const auto target=glm::mix(playerPosition_+glm::vec3(0,1.10F,0),camera.target(),orbit);
    eye=target+glm::vec3(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch))*distance;
    // Use the collision-safe follow-camera endpoint for a continuous handoff.
    eye=glm::mix(eye,camera.eye(),ease((t-3.6F)/.4F));
    const auto ray=eye-target;
    const float length=glm::length(ray);
    if(sceneGame_&&length>.01F) for(float d=.15F;d<=length;d+=.08F) {
      const auto point=target+ray*(d/length);
      if(sceneGame_->collidesWithScene({point-glm::vec3(.20F),point+glm::vec3(.20F)})) {
        eye=target+ray*(std::max(.1F,d-.15F)/length); break;
      }
    }
    viewProjection_=glm::perspective(glm::radians(58.F),aspect,.06F,100.F)*glm::lookAt(eye,target,{0,1,0});
  } else {
    viewProjection_=glm::perspective(glm::radians(camera.fov()),aspect,.06F,100.F)*glm::lookAt(eye,camera.target(),camera.up());
  }
  glViewport(0,0,width,height);
  if(options.menu) glClearColor(fog.r,fog.g,fog.b,1); else glClearColor(.22F,.27F,.30F,1);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glDisable(GL_BLEND);
  shader_.use(); shader_.setMat4("uViewProjection",viewProjection_); shader_.setVec3("uEye",eye);
  shader_.setInt("uAlbedo",0); shader_.setInt("uNormal",1); shader_.setInt("uNormalMapping",options.normalMapping?1:0);
  shader_.setVec3("uFogColour",fog); shader_.setFloat("uFogDensity",fogDensity);
  // The camera may pass the map edge (see FollowCamera): cut away the boundary wall it is behind.
  const bool cutaway=!options.gallery&&!options.menu&&sceneGame_;
  shader_.setInt("uCutaway",cutaway?1:0);
  if(cutaway) { const Bounds room=sceneGame_->room(); shader_.setVec4("uRoom",glm::vec4(room.min.x,room.min.z,room.max.x,room.max.z)); }
  shader_.setInt("uLightCount",lightCount_);
  for(int i=0;i<lightCount_;++i) {
    const std::string index="["+std::to_string(i)+"]";
    shader_.setVec4("uLightPosition"+index,lightPosition_[std::size_t(i)]); shader_.setVec4("uLightColour"+index,lightColour_[std::size_t(i)]);
  }
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,albedo_);
  glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,normal_);
  if(!options.gallery&&!options.menu) { glBindVertexArray(terrainVao_); glDrawArraysInstanced(GL_TRIANGLES,0,terrainVertices_,1); }
  for(int i=0;i<groups;++i) if(instanceCount_[i]) {
    // Soft effects test against the scene but never hide each other.
    const auto blend=Blend(i/primitives);
    if(blend!=Blend::Opaque) { glEnable(GL_BLEND); glDepthMask(GL_FALSE); glBlendFunc(GL_SRC_ALPHA,blend==Blend::Additive?GL_ONE:GL_ONE_MINUS_SRC_ALPHA); }
    meshAttributes(cubeVao_,cubeBuffer_,instanceBuffer_,std::size_t(instanceStart_[i])*sizeof(Instance));
    glBindVertexArray(cubeVao_); glDrawArraysInstanced(GL_TRIANGLES,meshStart_[i%primitives],meshCount_[i%primitives],instanceCount_[i]);
  }
  glDepthMask(GL_TRUE); glDisable(GL_BLEND);
  glBindVertexArray(0); glActiveTexture(GL_TEXTURE0);
}
void Renderer::gatherLights(const Game& game) {
  // Candidate lights carry (position, radius) and (colour, intensity); the strongest are kept.
  struct Light { glm::vec4 position, colour; float weight; };
  std::vector<Light> lights;
  const auto add=[&](glm::vec3 p,float radius,glm::vec3 colour,float intensity) { if(intensity>.03F) lights.push_back({{p,radius},{colour,intensity},intensity*radius}); };
  for(const auto& p:game.particles()) if(p.remaining>0) {
    const float life=p.remaining/p.lifetime,age=p.lifetime-p.remaining;
    if(p.kind==ParticleKind::Glow&&p.size>=.3F) add(p.position,1.5F+p.size*3.5F,p.colour,2.2F*life);
    if(p.kind==ParticleKind::Fireball) add(p.position,2.5F+p.size*(1+p.growth*std::sqrt(age))*2.F,p.colour,2.6F*life);
    if(p.kind==ParticleKind::Arc) add(p.position+p.extent*.5F,2.4F,p.colour,1.8F*life);
    if(p.kind==ParticleKind::Pillar) add(p.position+glm::vec3(0,1.2F,0),4.5F,p.colour,2.4F*life);
    if(p.kind==ParticleKind::Shell) add(p.position,2.F+p.growth*1.5F,p.colour,3.2F*life*life);
  }
  for(const auto& core:game.cores()) if(!core.collected) add(core.position,2.6F,{.35F,1,.88F},.55F+.15F*std::sin(game.elapsed()*3.1F));
  for(std::size_t i=0;i<game.traps().size();++i) if(game.trapPhase(i)==TrapPhase::Active) add((game.traps()[i].area.min+game.traps()[i].area.max)*.5F+glm::vec3(0,.35F,0),3.4F,{1,.35F,.15F},.9F);
  if(game.gateUnlocked()) add(game.gatePosition()+glm::vec3(0,1.5F,-.6F),3.5F,{.35F,1,.88F},.6F);
  for(const auto& checkpoint:game.checkpoints()) add(checkpoint.position+glm::vec3(0,1.2F,0),checkpoint.active?3.F:1.8F,checkpoint.active?glm::vec3(.35F,1,.9F):glm::vec3(1,.6F,.15F),checkpoint.active?.7F:.25F);
  for(const auto& pad:game.speedPads()) if(pad.flash>.05F) add((pad.area.min+pad.area.max)*.5F+glm::vec3(0,.4F,0),3.5F,{.45F,.85F,1},1.6F*pad.flash);
  for(const auto& tile:game.drainTiles()) add((tile.area.min+tile.area.max)*.5F+glm::vec3(0,.4F,0),2.6F,{.8F,.06F,.1F},.35F*crimsonFlicker(game.elapsed(),tile.area.min.x+tile.area.min.z)+1.4F*tile.flash);
  for(const auto& item:game.pickups()) if(item.respawn<=0&&!item.taken) add(item.position+glm::vec3(0,.9F,0),2.2F,pickupColour(item.kind),.5F);
  for(const auto& link:game.portals()) for(int end=0;end<2;++end) if(end==0?link.fromOn:link.toOn)
    add((end==0?link.from:link.to)+glm::vec3(0,.5F,0),2.4F,link.linked()?glm::vec3(.35F,1,.9F):glm::vec3(1,.62F,.2F),.45F+1.2F*link.pulse);
  for(const auto& door:game.doors()) add(door.centre+glm::vec3(0,.8F,0),2.4F,door.hold>0?glm::vec3(.35F,1,.9F):glm::vec3(1,.3F,.15F),.35F);
  std::sort(lights.begin(),lights.end(),[](const Light& a,const Light& b) { return a.weight>b.weight; });
  lightCount_=std::min<int>(maxLights,int(lights.size()));
  for(int i=0;i<lightCount_;++i) { lightPosition_[std::size_t(i)]=lights[std::size_t(i)].position; lightColour_[std::size_t(i)]=lights[std::size_t(i)].colour; }
}
glm::vec2 Renderer::project(glm::vec3 point,float width,float height) const {
  auto p=viewProjection_*glm::vec4(point,1);
  if(p.w<=0) return {-10000,-10000};
  p/=p.w;
  if(std::abs(p.x)>1.1F||std::abs(p.y)>1.1F||p.z>1) return {-10000,-10000};
  return {(p.x*.5F+.5F)*width,(.5F-p.y*.5F)*height};
}
}
