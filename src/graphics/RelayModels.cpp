#include "graphics/RelayModels.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

namespace relay {
namespace {
const glm::vec3 shell(.91F, .90F, .85F), dark(.065F, .085F, .10F);
const glm::vec3 teal(.12F, .39F, .57F), mint(.23F, .72F, 1.0F);
const glm::vec3 amber(1.0F, .57F, .13F), red(.95F, .22F, .14F);
// Unit primitive stretched from one point to another (sparks, beams, ring arcs).
glm::mat4 between(glm::vec3 from, glm::vec3 to, float width) {
  const auto delta = to - from;
  const float length = glm::length(delta);
  auto m = glm::translate(glm::mat4(1), (from + to) * .5F);
  if (length > .0001F) {
    const auto direction = delta / length;
    const auto axis = glm::cross(glm::vec3(0, 1, 0), direction);
    if (glm::length(axis) > .0001F) m = glm::rotate(m, std::acos(std::clamp(direction.y, -1.F, 1.F)), glm::normalize(axis));
    else if (direction.y < 0) m = glm::rotate(m, 3.14159265F, {1, 0, 0});
  }
  return glm::scale(m, {width, std::max(length, width), width});
}

glm::mat4 nodeTransform(const ModelNode& n) {
  auto m = glm::translate(glm::mat4(1), n.offset);
  m = glm::rotate(m, n.rotation.x, {1, 0, 0});
  m = glm::rotate(m, n.rotation.y, {0, 1, 0});
  return glm::rotate(m, n.rotation.z, {0, 0, 1});
}
}

int ArticulatedModel::add(ModelNode n) {
  if (n.parent >= int(nodes.size()) || n.parent < -1) throw std::invalid_argument("Model parents must precede their children");
  nodes.push_back(std::move(n)); return int(nodes.size()) - 1;
}
std::vector<glm::mat4> ArticulatedModel::transforms(const glm::mat4& root) const {
  const auto scaled = glm::scale(root, glm::vec3(scale));
  std::vector<glm::mat4> result;
  result.reserve(nodes.size());
  for (const auto& n : nodes)
    result.push_back((n.parent < 0 ? scaled : result[std::size_t(n.parent)]) * nodeTransform(n));
  return result;
}
void ArticulatedModel::append(const glm::mat4& root, std::vector<Instance>& output) const {
  const auto world = transforms(root);
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    const auto& n = nodes[i];
    if (!n.visible) continue;
    output.push_back({glm::scale(world[i], n.dimensions), glm::vec4(n.colour, 1), {1, n.emission, .25F, 1}, n.primitive});
  }
}
void appendBox(std::vector<Instance>& output, glm::vec3 p, glm::vec3 size,
               glm::vec3 colour, float emission, float metal, float angle) {
  auto m = glm::translate(glm::mat4(1), p);
  m = glm::rotate(m, angle, {0, 1, 0});
  output.push_back({glm::scale(m, size), glm::vec4(colour, 1), {metal, emission, metal > .5F ? .5F : .1F, 1}});
}
void appendGlow(std::vector<Instance>& output, glm::vec3 p, glm::vec3 size, glm::vec3 colour, float alpha, float intensity) {
  // Soft edges hide facets on small glows, so they use the low-poly sphere.
  const Primitive mesh = std::max({size.x, size.y, size.z}) < .6F ? Primitive::Blob : Primitive::Sphere;
  output.push_back({glm::scale(glm::translate(glm::mat4(1), p), size), glm::vec4(colour, alpha), {1, 1 + intensity, 0, 1}, mesh, Blend::Additive});
}

SceneModels::SceneModels() {
  auto part = [&](const char* name, int parent, glm::vec3 p, glm::vec3 dimensions, glm::vec3 colour, float glow = 0, bool visible = true) {
    const std::string label(name);
    const bool rounded=label=="head"||label=="torso"||label.find("shoulder")!=std::string::npos||label.find("elbow")!=std::string::npos||label.find("hip")!=std::string::npos||label.find("knee")!=std::string::npos||label=="forearm"||label=="ear_cover"||label=="ear_mount"||label=="reactor_frame"||label=="power_cell";
    const bool cylinder=label=="upper_arm"||label=="thigh"||label=="shin"||label=="wrist"||label.find("ankle")!=std::string::npos||label=="antenna";
    return robot_.add({name, parent, p, {}, dimensions, colour, glow, visible,rounded?Primitive::Sphere:cylinder?Primitive::Cylinder:Primitive::Box});
  };
  pelvis_=part("pelvis_root",-1,{0,.64F,0},{1,1,1},dark,0,false);
  body_=part("torso",pelvis_,{0,.41F,0},{.56F,.58F,.38F},shell);
  part("waist",body_,{0,-.32F,0},{.46F,.13F,.34F},dark);
  part("belt_buckle",body_,{0,-.32F,-.19F},{.14F,.09F,.04F},amber);
  part("chest_recess",body_,{0,.015F,-.205F},{.47F,.44F,.055F},dark);
  part("left_chest_armour",body_,{-.18F,.035F,-.24F},{.12F,.36F,.065F},teal);
  part("right_chest_armour",body_,{.18F,.035F,-.24F},{.12F,.36F,.065F},teal);
  part("reactor_frame",body_,{0,.04F,-.25F},{.25F,.29F,.075F},shell);
  part("power_cell",body_,{0,.04F,-.295F},{.16F,.20F,.035F},mint,.6F);
  part("reactor_separator",body_,{0,.04F,-.32F},{.025F,.20F,.018F},dark);
  for(float x:{-.22F,.22F}) for(float y:{-.21F,.22F})
    part("chest_fastener",body_,{x,y,-.245F},{.035F,.035F,.035F},amber);
  for(int i=0;i<4;++i) part("abdomen_vent",body_,{-.105F+i*.07F,-.21F,-.22F},{.035F,.07F,.025F},dark);
  const int pack=pack_=part("backpack",body_,{0,.015F,.29F},{.40F,.43F,.22F},dark);
  part("backpack_lid",pack,{0,.23F,0},{.42F,.07F,.24F},shell);
  packLight_=part("backpack_light",pack,{0,.03F,.125F},{.06F,.27F,.035F},mint,.45F);
  for(float x:{-.125F,.125F}) {
    for(int i=0;i<4;++i) part("cooling_fin",pack,{x,-.13F+i*.075F,.125F},{.13F,.025F,.045F},shell);
    part("pack_mount",pack,{x,-.25F,-.06F},{.07F,.09F,.20F},teal);
  }
  head_=part("head",body_,{0,.47F,-.01F},{.59F,.42F,.47F},shell);
  part("helmet_crown",head_,{0,.23F,.01F},{.55F,.06F,.43F},teal);
  part("helmet_chin",head_,{0,-.23F,0},{.44F,.075F,.43F},dark);
  part("visor_frame",head_,{0,-.015F,-.255F},{.53F,.26F,.07F},dark);
  part("visor_glass",head_,{0,-.005F,-.30F},{.46F,.19F,.025F},{.035F,.14F,.19F});
  eyeL_=part("left_eye",head_,{-.125F,.01F,-.32F},{.105F,.065F,.025F},mint,.8F);
  eyeR_=part("right_eye",head_,{.125F,.01F,-.32F},{.105F,.065F,.025F},mint,.8F);
  browL_=part("left_brow",head_,{-.125F,.095F,-.325F},{.14F,.025F,.025F},shell);
  browR_=part("right_brow",head_,{.125F,.095F,-.325F},{.14F,.025F,.025F},shell);
  for(int eye=0;eye<2;++eye) for(int half=0;half<2;++half) {
    smileEyes_[eye*2+half]=part("happy_eye",head_,{(eye==0?-.125F:.125F)+(half==0?-.027F:.027F),.02F,-.337F},{.08F,.026F,.024F},mint,.8F,false);
  }
  for(float x:{-.32F,.32F}) {
    part("ear_mount",head_,{x,0,.015F},{.07F,.21F,.23F},dark);
    part("ear_cover",head_,{x*1.1F,0,.015F},{.04F,.13F,.16F},teal);
  }
  for(int i=0;i<3;++i) part("chin_vent",head_,{-.06F+i*.06F,-.17F,-.245F},{.025F,.035F,.025F},dark);
  antennaSocket_=part("antenna_socket",head_,{.19F,.285F,.06F},{.11F,.07F,.11F},dark);
  antenna_=part("antenna_joint",head_,{.19F,.285F,.06F},{1,1,1},dark,0,false);
  part("antenna",antenna_,{0,.065F,0},{.035F,.10F,.035F},shell);
  part("antenna_lamp",antenna_,{0,.14F,0},{.07F,.06F,.07F},amber,.5F);
  shoulderL_=part("left_shoulder",body_,{-.39F,.14F,0},{.19F,.22F,.25F},teal);
  shoulderR_=part("right_shoulder",body_,{.39F,.14F,0},{.19F,.22F,.25F},teal);
  for(int shoulder:{shoulderL_,shoulderR_}) {
    bool left=shoulder==shoulderL_;
    part("shoulder_cap",shoulder,{left?-.10F:.10F,.015F,0},{.04F,.16F,.19F},shell);
    part("upper_arm",shoulder,{0,-.14F,0},{.14F,.21F,.17F},shell);
    int elbow=part(left?"left_elbow":"right_elbow",shoulder,{0,-.25F,0},{.16F,.14F,.18F},dark);
    if(left) elbowL_=elbow; else elbowR_=elbow;
    part("forearm",elbow,{0,-.10F,0},{.17F,.17F,.21F},shell);
    part("forearm_panel",elbow,{0,-.10F,-.12F},{.10F,.13F,.035F},teal);
    part("wrist",elbow,{0,-.21F,0},{.14F,.06F,.16F},dark);
    const int hand=part("hand",elbow,{0,-.25F,-.015F},{.19F,.11F,.23F},dark);
    if(left) handL_=hand; else handR_=hand;
    for(int i=0;i<3;++i) fingers_[(left?0:3)+i]=part("finger",hand,{-.06F+i*.06F,-.06F,-.04F},{.04F,.06F,.10F},shell);
  }
  hipL_=part("left_hip",pelvis_,{-.17F,0,0},{.20F,.16F,.23F},dark);
  hipR_=part("right_hip",pelvis_,{.17F,0,0},{.20F,.16F,.23F},dark);
  for(int hip:{hipL_,hipR_}) {
    const bool left=hip==hipL_;
    part("thigh",hip,{0,-.12F,0},{.18F,.23F,.21F},shell);
    int knee=part(left?"left_knee":"right_knee",hip,{0,-.25F,0},{.20F,.13F,.23F},dark);
    part("knee_pad",knee,{0,0,-.13F},{.17F,.14F,.07F},amber);
    part("shin",knee,{0,-.12F,0},{.18F,.24F,.21F},shell);
    part("shin_panel",knee,{0,-.12F,-.12F},{.08F,.17F,.025F},teal);
    int ankle=part(left?"left_ankle":"right_ankle",knee,{0,-.25F,0},{.14F,.10F,.16F},dark);
    part("boot",ankle,{0,-.055F,-.07F},{.25F,.16F,.38F},dark);
    part("toe_cap",ankle,{0,-.035F,-.23F},{.23F,.11F,.09F},shell);
    part("boot_sole",ankle,{0,-.14F,-.07F},{.26F,.03F,.39F},teal);
    if(left) { kneeL_=knee; ankleL_=ankle; } else { kneeR_=knee; ankleR_=ankle; }
  }
  // White ceramic shells, recessed optics and exposed mechanical linkages.
  for(auto& node:robot_.nodes) {
    const auto& name=node.name;
    if(name=="head") node.dimensions={.62F,.44F,.50F};
    if(name=="helmet_crown") { node.colour=shell; node.dimensions={.36F,.035F,.24F}; node.offset.y=.214F; }
    if(name=="helmet_chin") { node.dimensions={.36F,.04F,.32F}; node.offset.y=-.20F; }
    if(name=="torso") node.dimensions={.58F,.58F,.40F};
    if(name=="left_chest_armour"||name=="right_chest_armour") { node.colour=shell; node.primitive=Primitive::Sphere; node.dimensions={.14F,.39F,.10F}; }
    if(name=="reactor_frame") { node.dimensions={.23F,.23F,.08F}; node.colour=dark; }
    if(name=="power_cell") { node.dimensions={.12F,.12F,.035F}; node.colour=mint; }
    if(name=="reactor_separator") node.visible=false;
    if(name=="shoulder_cap"||name=="ear_cover") node.colour=shell;
    if(name=="left_shoulder"||name=="right_shoulder") { node.colour=shell; node.dimensions={.20F,.21F,.24F}; }
    if(name=="upper_arm"||name=="thigh") { node.colour=dark; node.dimensions.x*=.6F; node.dimensions.z*=.6F; }
    if(name=="forearm_panel"||name=="shin_panel") node.colour=shell;
    if(name=="knee_pad") { node.colour=shell; node.dimensions={.15F,.12F,.06F}; }
    if(name=="boot_sole") node.colour=dark;
    if(name=="chest_fastener") { node.colour=dark; node.primitive=Primitive::Sphere; }
    if(name=="backpack") { node.dimensions={.34F,.38F,.17F}; node.offset.z=.25F; }
    if(name=="backpack_lid") node.dimensions={.36F,.045F,.18F};
    if(name=="cooling_fin") { node.dimensions.x=.10F; node.colour={.35F,.38F,.38F}; }
    if(name=="antenna_lamp") node.colour=mint;
  }
  for(float x:{-.125F,.125F}) {
    const int lens=part("optic_bezel",head_,{x,.01F,-.312F},{.15F,.15F,.04F},{.35F,.40F,.42F});
    robot_.nodes[lens].primitive=Primitive::Sphere;
  }
  robot_.nodes[eyeL_].offset.z=robot_.nodes[eyeR_].offset.z=-.342F;
  robot_.nodes[eyeL_].primitive=robot_.nodes[eyeR_].primitive=Primitive::Sphere;
  for(int eye:smileEyes_) robot_.nodes[eye].offset.z=-.365F;
  for(int shoulder:{shoulderL_,shoulderR_}) {
    const float side=shoulder==shoulderL_?-1.F:1.F;
    const int joint=part("shoulder_bearing",shoulder,{side*.10F,0,0},{.035F,.13F,.13F},dark);
    robot_.nodes[joint].primitive=Primitive::Sphere;
    part("shoulder_identification",shoulder,{0,.065F,-.115F},{.11F,.045F,.025F},shoulder==shoulderL_?amber:teal);
  }
  for(int hip:{hipL_,hipR_}) {
    const int piston=part("thigh_piston",hip,{.055F,-.12F,-.025F},{.035F,.20F,.035F},{.54F,.58F,.58F});
    robot_.nodes[piston].primitive=Primitive::Cylinder;
  }
  // Small serial-mark bars are geometry, so the character has no external texture dependency.
  for(int i=0;i<7;++i) part("serial_bar",body_,{-.08F+i*.025F,-.17F,-.245F},{i%2?.008F:.014F,.045F,.008F},dark);
  // Cuter proportions: a bigger, rounder head on a slightly smaller body, a glossy face
  // screen with tall eyes and catch-lights, a small LED smile, cheek lights, a crown
  // stripe, round ears, a ball-tipped antenna, mitten hands and round boots.
  const glm::vec3 screen(.035F,.05F,.07F),blush(1,.45F,.58F);
  for(auto& node:robot_.nodes) {
    const auto& name=node.name;
    if(name=="head") { node.dimensions={.70F,.53F,.57F}; node.offset.y=.44F; }
    if(name=="helmet_crown") { node.dimensions={.09F,.045F,.46F}; node.offset={0,.252F,.01F}; node.colour=teal; }
    if(name=="helmet_chin"||name=="chin_vent"||name=="optic_bezel"||name=="left_brow"||name=="right_brow") node.visible=false;
    if(name=="visor_frame") { node.primitive=Primitive::Sphere; node.dimensions={.58F,.38F,.16F}; node.offset={0,-.02F,-.215F}; node.colour=screen; }
    if(name=="visor_glass") { node.primitive=Primitive::Sphere; node.dimensions={.17F,.045F,.02F}; node.offset={-.13F,.115F,-.29F}; node.colour={.55F,.65F,.72F}; node.emission=.25F; }
    if(name=="ear_mount") { node.offset.x=node.offset.x<0?-.345F:.345F; node.dimensions={.07F,.22F,.22F}; }
    if(name=="ear_cover") { node.offset.x=node.offset.x<0?-.375F:.375F; node.dimensions={.05F,.17F,.17F}; node.colour=teal; }
    if(name=="antenna_socket") node.offset.y=.30F;
    if(name=="antenna_joint") node.offset.y=.30F;
    if(name=="antenna") node.dimensions={.03F,.12F,.03F};
    if(name=="antenna_lamp") { node.primitive=Primitive::Sphere; node.dimensions={.10F,.10F,.10F}; node.offset.y=.15F; }
    if(name=="torso") node.dimensions={.54F,.54F,.42F};
    if(name=="hand") { node.primitive=Primitive::Sphere; node.dimensions={.17F,.15F,.20F}; node.colour=shell; }
    if(name=="finger") { node.primitive=Primitive::Sphere; node.dimensions={.05F,.07F,.08F}; node.colour={.80F,.80F,.76F}; }
    if(name=="boot") { node.primitive=Primitive::Sphere; node.dimensions={.25F,.18F,.38F}; node.colour={.10F,.14F,.20F}; }
    if(name=="toe_cap") { node.primitive=Primitive::Sphere; node.dimensions={.21F,.12F,.13F}; node.offset.z=-.215F; }
  }
  for(int eye:{eyeL_,eyeR_}) { auto& e=robot_.nodes[std::size_t(eye)]; e.dimensions={.13F,.15F,.03F}; e.offset={eye==eyeL_?-.12F:.12F,.015F,-.296F}; }
  for(int i=0;i<4;++i) { auto& e=robot_.nodes[std::size_t(smileEyes_[i])]; e.offset={(i<2?-.12F:.12F)+(i%2==0?-.032F:.032F),.03F,-.30F}; e.dimensions={.10F,.032F,.03F}; }
  for(int i=0;i<2;++i) {
    const float x=i==0?-.12F:.12F;
    glints_[std::size_t(i)]=part("eye_glint",head_,{x+.032F,.06F,-.315F},{.04F,.04F,.02F},{1,1,1},.95F);
    cheeks_[std::size_t(i)]=part("cheek",head_,{i==0?-.205F:.205F,-.075F,-.268F},{.075F,.035F,.02F},blush,.4F);
  }
  mouth_[0]=part("mouth",head_,{0,-.105F,-.302F},{.05F,.017F,.02F},mint,.75F);
  mouth_[1]=part("mouth",head_,{-.042F,-.097F,-.300F},{.042F,.017F,.02F},mint,.75F);
  mouth_[2]=part("mouth",head_,{.042F,-.097F,-.300F},{.042F,.017F,.02F},mint,.75F);
  for(int i:{glints_[0],glints_[1],cheeks_[0],cheeks_[1],mouth_[0],mouth_[1],mouth_[2]}) robot_.nodes[std::size_t(i)].primitive=Primitive::Sphere;
  robot_.nodes[std::size_t(mouth_[1])].rotation.z=-.45F; robot_.nodes[std::size_t(mouth_[2])].rotation.z=.45F;
  robot_.scale=Game::modelScale;
  bindPose_=robot_.nodes;

  auto gatePart = [&](const char* name, int parent, glm::vec3 p, glm::vec3 size, glm::vec3 colour, float glow = 0) {
    return gate_.add({name, parent, p, {}, size, colour, glow, true});
  };
  int frame = gate_.add({"gate_root", -1, {}, {}, {1, 1, 1}, shell, 0, false});
  gatePart("left_post", frame, {-1.5F, 1.5F, 0}, {.4F, 3, .5F}, shell);
  gatePart("right_post", frame, {1.5F, 1.5F, 0}, {.4F, 3, .5F}, shell);
  gatePart("lintel", frame, {0, 3, 0}, {3.4F, .4F, .5F}, dark);
  lamp_ = gatePart("status_lamp", frame, {0, 3.02F, .28F}, {1.1F, .12F, .05F}, amber, .7F);
  doorL_ = gatePart("left_panel", frame, {-.65F, 1.35F, 0}, {1.28F, 2.7F, .26F}, shell);
  doorR_ = gatePart("right_panel", frame, {.65F, 1.35F, 0}, {1.28F, 2.7F, .26F}, shell);
  for (int panel : {doorL_, doorR_}) {
    gatePart(panel == doorL_ ? "left_panel_stripe" : "right_panel_stripe", panel, {0, .4F, .15F}, {.85F, .08F, .04F}, shell);
    gatePart(panel == doorL_ ? "left_panel_handle" : "right_panel_handle", panel, {panel == doorL_ ? .42F : -.42F, -.05F, .17F}, {.07F, .46F, .09F}, dark);
  }
}



void SceneModels::appendSwitch(glm::vec3 position, bool active, float press, std::vector<Instance>& out) {
  appendBox(out, position + glm::vec3(0, .09F, 0), {.75F, .18F, .65F}, dark, 0, 1);
  appendBox(out, position + glm::vec3(0, .46F, 0), {.48F, .62F, .4F}, shell, 0, 1); out.back().primitive=Primitive::Cylinder;
  appendBox(out, position + glm::vec3(0, .86F, 0), {.68F, .2F, .56F}, dark, 0, 1);
  appendBox(out, position + glm::vec3(0, 1.0F - press * .07F, 0), {.34F, .10F, .30F}, active ? mint : amber, .45F, 1);
  appendBox(out, position + glm::vec3(0, .52F, .215F), {.22F, .2F, .03F}, active ? teal : dark, .15F, 1);
}















void SceneModels::append(const Game& game,std::vector<Instance>& out,float) {
 const auto& p=game.player(); poseRobot(p);
 auto root=glm::translate(glm::mat4(1),p.position+glm::vec3(0,bodyHop(p),0));
 root=glm::scale(glm::rotate(root,p.yaw,{0,1,0}),bodyScale(p));
 robot_.append(root,out);
 for(const auto& particle:game.particles()) if(particle.remaining>0) {
  const float fade=std::min(1.F,particle.remaining/.18F);
  if(particle.kind==ParticleKind::Spark||particle.kind==ParticleKind::Ray) {
   out.push_back({between(particle.position,particle.position-(particle.kind==ParticleKind::Ray?particle.extent:particle.velocity*.065F),particle.size*fade),glm::vec4(particle.colour,fade),{0,1.6F,0,1},Primitive::Flat,Blend::Additive});
   appendGlow(out,particle.position,glm::vec3(particle.size*3),particle.colour,.22F*fade,.5F);
  } else {
   appendBox(out,particle.position,glm::vec3(particle.size*fade),particle.colour,0,.5F,particle.seed+(particle.lifetime-particle.remaining)*8);
  }
 }
 for(const auto& c:game.cores())if(!c.collected) {
  appendBox(out,c.position-glm::vec3(0,.65F,0),{.65F,.1F,.65F},{.28F,.32F,.36F});
  appendBox(out,c.position,{.42F,.55F,.42F},{.25F,.65F,.85F},.12F,0);
 }
 for(const auto& pad:game.jumpPads()) {
  appendBox(out,pad.position+glm::vec3(0,.012F,0),{1.8F,.024F,1.8F},dark,0,.6F);
  appendBox(out,pad.position+glm::vec3(0,.03F,0),{1.5F,.024F,1.5F},{.07F,.32F,.4F},.15F);
  for(float z:{-.35F,.25F}) for(float side:{-1.F,1.F})
   appendBox(out,pad.position+glm::vec3(side*.23F,.05F,z),{.65F,.03F,.13F},mint,.5F+pad.pulse*.5F,0,side*.65F);
  if(pad.pulse>0)appendGlow(out,pad.position+glm::vec3(0,.1F,0),{1.8F,.16F,1.8F},mint,.18F*pad.pulse,.4F);
 }
 const auto g=game.gatePosition();
 const glm::vec3 lamp=game.gateUnlocked()?glm::vec3(.3F,.7F,.4F):glm::vec3(.8F,.35F,.25F);
 for(float side:{-1.F,1.F}) {
  appendBox(out,g+glm::vec3(side*1.8F,1.5F,0),{.4F,3,.6F},{.45F,.48F,.5F});
  appendBox(out,g+glm::vec3(side*(.8F+game.gateOpen()*1.6F),1.4F,0),{1.6F,2.8F,.4F},{.32F,.36F,.4F});
 }
 appendBox(out,g+glm::vec3(0,3,0),{4,.4F,.6F},{.45F,.48F,.5F});
 appendBox(out,g+glm::vec3(0,3,.32F),{1.2F,.16F,.06F},lamp,.1F);
 appendBox(out,g+glm::vec3(0,.015F,-.9F),{3,.03F,1.3F},lamp);
 for(const auto& sw:game.switches()) appendSwitch(sw.position,game.trapPhase(sw.trap)==TrapPhase::Disabled,sw.animation,out);
 for(std::size_t i=0;i<game.traps().size();++i) {
  const auto& t=game.traps()[i]; const auto phase=game.trapPhase(i);
  const glm::vec3 colour=phase==TrapPhase::Active?red:phase==TrapPhase::Warning?amber:teal;
  for(float x=t.area.min.x+.5F;x<t.area.max.x;x+=1) for(float z=t.area.min.z+.5F;z<t.area.max.z;z+=1) {
   appendBox(out,{x,1.04F,z},{.92F,.08F,.92F},dark,0,1);
   appendBox(out,{x,1.085F,z},{.8F,.025F,.07F},colour,.2F,1);
   if(phase==TrapPhase::Active) for(float dx:{-.23F,.23F}) for(float dz:{-.23F,.23F}) {
    // Stacked, tapering metal voxels form readable retractable spikes.
    for(int k=0;k<4;++k) { float w=.22F-float(k)*.045F; appendBox(out,{x+dx,1.15F+.15F*k,z+dz},{w,.15F,w},{.68F,.72F,.76F},0,1); }
   }
  }
 }
 for(const auto& h:game.movers()) {
  const auto parts=h.collisionParts();
  for(std::size_t i=0;i<parts.size();++i) appendBox(out,(parts[i].min+parts[i].max)*.5F,parts[i].max-parts[i].min,i==0?dark:i==1?red:amber,i==0?0:.2F,1);
 }
}





void SceneModels::appendMenu(float time,std::vector<Instance>& out,glm::vec2 look) {
  // A quiet white test chamber: graphite floor plates, ceramic wall panels
  // and a single shaft of light falling on the plinth.
  // Flat slabs and hairline seams: the chamber reads as panels, not bricks.
  const glm::vec3 panel(.95F,.95F,.93F),graphite(.34F,.36F,.37F),seam(.12F,.14F,.15F);
  const auto slab=[&](glm::vec3 p,glm::vec3 size,glm::vec3 colour,float glow=0) { appendBox(out,p,size,colour,glow,1); out.back().primitive=Primitive::Flat; };
  slab({2,-.15F,6},{60,.3F,24},graphite);
  slab({2,6,-3.35F},{60,14,.3F},panel);
  for(int i=-9;i<=12;++i) {
    slab({float(i)*2-1,.002F,6},{.035F,.006F,24},seam*1.6F);
    slab({float(i)*2-1,6,-3.198F},{.03F,14,.006F},seam*3.F);
  }
  for(int i=-3;i<=8;++i) slab({2,.002F,float(i)*2-1},{60,.006F,.035F},seam*1.6F);
  for(int i=1;i<7;++i) slab({2,float(i)*2,-3.198F},{60,.03F,.006F},seam*3.F);
  // A column of dark non-reflective panels and a few off-white ones break the grid.
  for(int y=0;y<6;++y) slab({-9,1+float(y)*2,-3.19F},{1.97F,1.97F,.02F},{.20F,.22F,.23F});
  for(int y=0;y<3;++y) slab({9,1+float(y)*2,-3.19F},{1.97F,1.97F,.02F},{.20F,.22F,.23F});
  for(auto cell:{glm::vec2(5,4),glm::vec2(-5,0),glm::vec2(13,3)}) slab({cell.x,1+cell.y*2,-3.19F},{1.9F,1.9F,.02F},{.86F,.87F,.86F});
  slab({3,.035F,-3.18F},{60,.05F,.04F},{.86F,.94F,1},.95F);
  slab({9.F,4.6F,-3.17F},{.16F,.16F,.03F},{1,.55F,.12F},.9F);
  // Light shaft from above the frame to the plinth; soft edges come from the shader.
  const glm::vec3 centre(2.5F,0,0);
  out.push_back({between({-3.2F,7.6F,-2.9F},centre+glm::vec3(.2F,.2F,.2F),2.5F),glm::vec4(.85F,.93F,1,.12F),{1,2,0,1},Primitive::Cylinder,Blend::Additive});
  for(int i=0;i<28;++i) {
    const float h=std::fmod(float(i)*.618034F,1.F),k=std::fmod(float(i)*.754878F+.31F,1.F);
    const float rise=std::fmod(time*.035F*(.6F+k)+h,1.F);
    const glm::vec3 mote(centre.x-4.5F+8.F*h+.25F*std::sin(time*.3F+float(i)),.3F+5.6F*rise,-2.4F+4.6F*k);
    appendGlow(out,mote,glm::vec3(.03F+.015F*k),{.92F,.96F,1},.5F*std::sin(rise*3.14159F),1.F);
  }
  // Plinth: dark base, white top, a ring of blue status lights and a soft pool.
  appendBox(out,centre+glm::vec3(0,.11F,0),{3.7F,.22F,3.7F},seam,0,1); out.back().primitive=Primitive::Cylinder;
  appendBox(out,centre+glm::vec3(0,.235F,0),{3.4F,.03F,3.4F},panel,0,1); out.back().primitive=Primitive::Cylinder;
  for(int i=0;i<48;++i) {
    const float a=float(i)*6.2831853F/48,on=.55F+.45F*std::sin(time*1.6F-a*2);
    appendBox(out,centre+glm::vec3(std::sin(a)*1.78F,.225F,std::cos(a)*1.78F),{.05F,.03F,.12F},{.30F,.72F,1},.45F+.5F*on,1,a);
  }
  appendGlow(out,centre+glm::vec3(0,.26F,0),{3.3F,.05F,3.3F},{.35F,.75F,1},.22F,.8F);
  // The courier watches the cursor and waves now and then.
  const float dt=menuLookTime_<0?1.F:std::clamp(time-menuLookTime_,0.F,.1F); menuLookTime_=time;
  menuLook_+=(look-menuLook_)*(1-std::exp(-5.F*dt));
  const auto smooth=[](float x) { x=std::clamp(x,0.F,1.F); return x*x*(3-2*x); };
  const float cycle=std::fmod(time,18.F)-5;
  Player p; p.grounded=true; p.idlePhase=time; p.motion.idleFor=time;
  p.motion.lookYaw=std::clamp(menuLook_.x,-1.F,1.F)*.5F; p.motion.lookPitch=-std::clamp(menuLook_.y,-1.F,1.F)*.22F-.04F;
  p.motion.wave=smooth(cycle/.35F)*(1-smooth((cycle-1.9F)/.4F));
  poseRobot(p);
  auto root=glm::translate(glm::mat4(1),centre+glm::vec3(0,.25F,0));
  root=glm::rotate(root,2.78F+std::sin(time*.3F)*.05F,{0,1,0});
  robot_.append(glm::scale(root,glm::vec3(1.55F/Game::modelScale)),out);
}
}  // namespace relay
