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
    output.push_back({glm::scale(world[i], n.dimensions), glm::vec4(n.colour, 1), {1, n.emission, .25F, glm::length(n.colour)<.35F?4.F:1.F}, n.primitive});
  }
}
void appendBox(std::vector<Instance>& output, glm::vec3 p, glm::vec3 size,
               glm::vec3 colour, float emission, float metal, float angle) {
  auto m = glm::translate(glm::mat4(1), p);
  m = glm::rotate(m, angle, {0, 1, 0});
  output.push_back({glm::scale(m, size), glm::vec4(colour, 1), {metal, emission, metal > .5F ? .5F : .1F, metal>.5F?3.F:glm::length(colour)<.35F?4.F:glm::length(colour)>1.2F?1.F:5.F}});
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



// ---- Ported from the full edition: core, plate, door and portal models ----
void SceneModels::appendCore(glm::vec3 position, float time, std::vector<Instance>& out, float floorHeight) {
  position.y += std::sin(time * 2.4F) * .12F;
  const float angle = time * 1.4F;
  appendBox(out, position, {.36F, .46F, .36F}, mint, .4F, 1, angle); out.back().primitive=Primitive::Sphere;
  // Breathing halo, a faint column of light and motes spiralling up out of the core.
  const float pulse = .5F + .5F * std::sin(time * 3.1F);
  for (int i = 0; i < 7; ++i) {
    const float rise = std::fmod(time * .45F + float(i) / 7.F, 1.F), a = time * 1.6F + float(i) * 2.4F;
    const glm::vec3 mote = glm::vec3(position.x, floorHeight, position.z) + glm::vec3(std::cos(a) * (.45F - .25F * rise), .1F + 2.1F * rise, std::sin(a) * (.45F - .25F * rise));
    appendGlow(out, mote, glm::vec3(.045F), {.45F, 1, .9F}, std::sin(rise * 3.14159F) * .9F, 1.4F);
  }
  appendGlow(out, position, glm::vec3(.95F + .12F * pulse), {.30F, .95F, .85F}, .30F + .12F * pulse, .9F);
  out.push_back({glm::scale(glm::translate(glm::mat4(1), {position.x, floorHeight + 1.3F, position.z}), {.55F, 2.6F, .55F}), glm::vec4(.35F, .95F, .9F, .10F + .04F * pulse), {1, 1.8F, 0, 1}, Primitive::Cylinder, Blend::Additive});
  for(float side:{-1.F,1.F}) { appendBox(out,position+glm::vec3(side*.19F,0,0),{.10F,.36F,.40F},shell,0,1,angle); out.back().primitive=Primitive::Sphere; }
  appendBox(out, position + glm::vec3(0, .29F, 0), {.48F, .10F, .48F}, shell, 0, 1, angle);
  appendBox(out, position - glm::vec3(0, .29F, 0), {.48F, .10F, .48F}, shell, 0, 1, angle);
  appendBox(out, {position.x, floorHeight, position.z}, {.8F, .035F, .8F}, teal, .12F, 1, .785F);
}
void SceneModels::appendPlate(const PressurePlate& plate, float doorHold, float time, std::vector<Instance>& out) {
  // Floor button: a dark ring and a white cap that sinks when pressed. Amber while its door is
  // sealed, cyan (every ring lamp lit) once it has unlocked the door.
  const glm::vec3 p = plate.position;
  const bool live = doorHold > 0;
  const glm::vec3 lamp = live ? glm::vec3(.35F, 1, .9F) : glm::vec3(1, .6F, .15F);
  appendBox(out, p + glm::vec3(0, .02F, 0), {1.5F, .04F, 1.5F}, dark, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, .05F - .03F * plate.press, 0), {1.08F, .05F, 1.08F}, shell, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, .078F - .03F * plate.press, 0), {.46F, .01F, .46F}, lamp, live ? .9F : .35F + .25F * std::sin(time * 3.F), 1); out.back().primitive = Primitive::Cylinder;
  for (int i = 0; i < 12; ++i) {
    const float a = float(i) * 6.2831853F / 12;
    const bool on = live ? float(i) / 12 < doorHold / Game::doorHold : (int(time * 4.F) % 12) == i;
    appendBox(out, p + glm::vec3(std::sin(a) * .64F, .045F, std::cos(a) * .64F), {.06F, .02F, .13F}, on ? lamp : glm::vec3(.16F, .18F, .19F), on ? .85F : 0, 1, a);
  }
  appendGlow(out, p + glm::vec3(0, .15F, 0), glm::vec3(.62F, .1F, .62F), lamp, live ? .3F : .12F, 1.1F);
}

void SceneModels::appendDoor(const BlastDoor& door, float time, std::vector<Instance>& out) {
  // Split blast door: white panels with a teal band and a dark seam edge; a status strip on
  // each jamb (red sealed, cyan open) and countdown lamps along the top while it is held.
  const int axis = door.alongZ ? 2 : 0, normal = door.alongZ ? 0 : 2;
  glm::vec3 along(0); along[axis] = 1;
  const bool open = door.hold > 0 || door.open > .02F;
  const glm::vec3 status = open ? glm::vec3(.35F, 1, .9F) : glm::vec3(1, .25F, .15F);
  for (const auto& panel : door.panels()) {
    const glm::vec3 c = (panel.min + panel.max) * .5F, s = panel.max - panel.min;
    appendBox(out, c, s - glm::vec3(.01F), shell, 0, 1);
    glm::vec3 band = s; band.y = .12F; band[normal] += .03F; band[axis] -= .1F;
    appendBox(out, c + glm::vec3(0, .25F, 0), band, teal, .15F, 1);
    glm::vec3 seam = s; seam[axis] = .06F; seam[normal] += .02F; seam.y -= .1F;
    const float edge = c[axis] < door.centre[axis] ? panel.max[axis] - .03F : panel.min[axis] + .03F;
    glm::vec3 at = c; at[axis] = edge;
    appendBox(out, at, seam, dark, 0, 1);
  }
  for (float side : {-1.F, 1.F}) {
    glm::vec3 jamb = door.centre + along * (side * (door.half[axis] + .06F));
    glm::vec3 size(.14F, door.half.y * 2, .14F); size[normal] = 1.06F;
    appendBox(out, jamb, size, dark, 0, 1);
    glm::vec3 strip(.05F, door.half.y * 1.6F, .05F); strip[normal] = 1.08F;
    appendBox(out, jamb, strip, status, open ? .9F : .45F + .3F * std::sin(time * 5.F), 1);
  }
  const float held = std::clamp(door.hold / Game::doorHold, 0.F, 1.F);
  for (int i = 0; i < 8; ++i) {
    const float t = (float(i) + .5F) / 8.F;
    glm::vec3 lamp = door.centre + glm::vec3(0, door.half.y + .02F, 0) + along * ((t - .5F) * door.half[axis] * 2);
    glm::vec3 size(.16F, .04F, .16F); size[normal] = .3F;
    const bool on = open && t < held;
    appendBox(out, lamp, size, on ? glm::vec3(.35F, 1, .9F) : glm::vec3(.16F, .18F, .19F), on ? .9F : 0, 1);
  }
}

void SceneModels::appendPortal(const PortalLink& link, float time, std::vector<Instance>& out) {
  // A small round portal at each end, built up off the floor: a dark pedestal with a metal bevel
  // and a ring of lamps, three curved fins leaning in over it, and two rings of light hovering
  // above that turn against each other. Asleep it is dark with one amber lamp circling and the
  // rings still; awake it glows amber and waits for its pair; linked, both ends glow cyan, the
  // rings spin and bob, a twisting double helix of light climbs out of a bright disc inside a soft
  // beam, and motes are drawn up through it.
  for (int end = 0; end < 2; ++end) {
    const bool on = end == 0 ? link.fromOn : link.toOn, linked = link.linked();
    const glm::vec3 p = end == 0 ? link.from : link.to;
    const glm::vec3 colour = linked ? glm::vec3(.3F, .95F, 1) : glm::vec3(1, .62F, .2F), off(.16F, .18F, .19F);
    const float flare = link.pulse, seed = float(end) * 1.7F;
    // Pedestal and bevel.
    appendBox(out, p + glm::vec3(0, .07F, 0), {1.8F, .14F, 1.8F}, dark, 0, 1); out.back().primitive = Primitive::Cylinder;
    appendBox(out, p + glm::vec3(0, .15F, 0), {1.55F, .04F, 1.55F}, {.42F, .46F, .5F}, 0, 1); out.back().primitive = Primitive::Cylinder;
    appendBox(out, p + glm::vec3(0, .17F, 0), {1.2F, .02F, 1.2F}, on ? colour * .35F : glm::vec3(.06F, .07F, .08F), on ? std::min(.98F, .45F + .35F * flare) : 0, 1); out.back().primitive = Primitive::Cylinder;
    for (int i = 0; i < 18; ++i) {
      const float a = float(i) * .349F;
      const bool lit = on || int(std::fmod(time * 9.F, 18.F)) == i;
      const float k = on ? .6F + .4F * std::sin(time * (linked ? 4.F : 2.F) + float(i) * .35F) : 1.F;
      appendBox(out, p + glm::vec3(std::sin(a) * .83F, .1F, std::cos(a) * .83F), {.07F, .05F, .12F}, lit ? colour : off, lit ? std::min(.98F, .45F + .45F * k + flare) : 0, 1, a);
    }
    // Three fins: each a curved claw of three segments rising from the rim and leaning in, with a
    // light strip on its inner face.
    for (int f = 0; f < 3; ++f) {
      const float a = float(f) * 2.094F + .5F;
      const glm::vec3 out1(std::cos(a), 0, std::sin(a));
      const glm::vec3 pts[4]{p + out1 * .82F + glm::vec3(0, .14F, 0), p + out1 * .84F + glm::vec3(0, .55F, 0), p + out1 * .74F + glm::vec3(0, .95F, 0), p + out1 * .56F + glm::vec3(0, 1.22F, 0)};
      for (int k = 0; k < 3; ++k) {
        out.push_back({between(pts[k], pts[k + 1], .13F - .025F * float(k)), glm::vec4(.36F, .4F, .44F, 1), {1, 0, .45F, 1}, Primitive::Box, Blend::Opaque});
        const glm::vec3 inner = -out1 * .07F;
        out.push_back({between(pts[k] + inner, pts[k + 1] + inner, .035F), glm::vec4(on ? colour : off, 1), {0, on ? 2.F + flare : 1.05F, 0, 1}, Primitive::Box, on ? Blend::Additive : Blend::Opaque});
      }
      appendGlow(out, pts[3] - out1 * .05F, glm::vec3(.07F), on ? colour : glm::vec3(.4F, .42F, .44F), on ? .9F : .25F, 1.6F);
    }
    // Two hovering rings turning against each other (still while asleep), bobbing when linked.
    for (int r = 0; r < 2; ++r) {
      const float radius = r == 0 ? .62F : .46F, spin = on ? time * (r == 0 ? 1.4F : -2.1F) * (linked ? 1.6F : .6F) : 0;
      const float height = (r == 0 ? .55F : 1.05F) + (linked ? .06F * std::sin(time * 2.2F + float(r) * 1.7F + seed) : 0);
      for (int i = 0; i < 14; ++i) {
        if (!on && i % 2) continue;
        const float a0 = float(i) * .4488F + spin, a1 = a0 + .33F;
        out.push_back({between(p + glm::vec3(std::cos(a0) * radius, height, std::sin(a0) * radius), p + glm::vec3(std::cos(a1) * radius, height, std::sin(a1) * radius), .045F),
                       glm::vec4(on ? colour : off, on ? .9F : 1), {0, on ? 2.2F + flare : 1.02F, 0, 1}, Primitive::Box, on ? Blend::Additive : Blend::Opaque});
      }
    }
    if (!on) continue;
    appendGlow(out, p + glm::vec3(0, .2F, 0), glm::vec3(.7F, .06F, .7F), colour, .3F + .35F * flare, 1.5F);
    if (!linked) continue;
    // Linked: a bright disc, a double helix climbing out of it inside a soft beam, rising motes.
    appendGlow(out, p + glm::vec3(0, .21F, 0), glm::vec3(.5F, .03F, .5F), {.85F, 1, 1}, .55F + .3F * std::sin(time * 3.F + seed), 1.9F);
    for (int strand = 0; strand < 2; ++strand)
      for (int k = 0; k < 14; ++k) {
        const float h0 = .2F + float(k) * .1F, h1 = h0 + .1F, r = .32F - .012F * float(k);
        const float a0 = time * 3.2F + float(strand) * 3.14159F + h0 * 4.F, a1 = time * 3.2F + float(strand) * 3.14159F + h1 * 4.F;
        out.push_back({between(p + glm::vec3(std::cos(a0) * r, h0, std::sin(a0) * r), p + glm::vec3(std::cos(a1) * r, h1, std::sin(a1) * r), .04F), glm::vec4(colour, 1.F - float(k) / 16.F), {0, 2.3F, 0, 1}, Primitive::Box, Blend::Additive});
      }
    for (int i = 0; i < 8; ++i) {
      const float life = std::fmod(time * .8F + float(i) * .125F + seed, 1.F), a = float(i) * .785F + time;
      appendGlow(out, p + glm::vec3(std::cos(a) * .4F * (1 - life), .2F + 1.5F * life, std::sin(a) * .4F * (1 - life)), glm::vec3(.05F), colour, std::sin(life * 3.14159F), 1.8F);
    }
    out.push_back({between(p + glm::vec3(0, .18F, 0), p + glm::vec3(0, 1.6F, 0), 1.1F), glm::vec4(colour, .08F + .1F * flare), {1, 2, 0, 1}, Primitive::Cylinder, Blend::Additive});
  }
}

// Electric floor plate (full edition): a dark tile with a glowing cross, teal while safe, amber
// and flickering in its warning second, red while live. Live plates crackle: arcs leap between
// points across the plate and up into the air, with hot flashes where they land. The arcs are
// drawn from the clock (not particles), so a yard of plates never fills the particle pool.
void appendTrapPlate(const FloorTrap& trap, TrapPhase phase, float time, int seed, std::vector<Instance>& out) {
  const glm::vec3 dark(.065F, .085F, .10F), teal(.12F, .39F, .57F), amber(1.0F, .57F, .13F), red(.95F, .22F, .14F);
  const glm::vec3 colour = phase == TrapPhase::Active ? red : phase == TrapPhase::Warning ? amber : teal;
  const float glow = phase == TrapPhase::Active ? .7F : phase == TrapPhase::Warning ? .25F + .25F * std::sin(time * 18) : .1F;
  for (float x = trap.area.min.x + .5F; x < trap.area.max.x; x += 1)
    for (float z = trap.area.min.z + .5F; z < trap.area.max.z; z += 1) {
      const auto tile = trap.tileBounds(x, z);
      appendBox(out, (tile.min + tile.max) * .5F, tile.max - tile.min, dark, 0, 1);
      appendBox(out, {x, trap.area.min.y + .09F, z}, {.65F, .025F, .12F}, colour, glow, 1, .785F);
      appendBox(out, {x, trap.area.min.y + .095F, z}, {.12F, .025F, .65F}, colour, glow, 1, .785F);
    }
  if (phase != TrapPhase::Active && phase != TrapPhase::Warning) return;
  const auto hash = [](float n) { const float h = std::sin(n * 12.9898F) * 43758.5453F; return h - std::floor(h); };
  const glm::vec3 lo = trap.area.min, hi = trap.area.max;
  const float cells = (hi.x - lo.x) * (hi.z - lo.z), y = lo.y + .12F;
  const float deal = std::floor(time * 9.F);  // the arcs are re-dealt nine times a second
  if (phase == TrapPhase::Warning) {
    // Warning: the plate fizzes, a few amber embers hopping off it.
    const int embers = std::min(6, 1 + int(cells / 4));
    for (int i = 0; i < embers; ++i) {
      const float k = float(seed * 31 + i);
      const glm::vec3 at(glm::mix(lo.x, hi.x, hash(deal + k * 1.3F)), y + .15F * hash(deal * 1.9F + k), glm::mix(lo.z, hi.z, hash(deal * 2.7F + k * .7F)));
      appendGlow(out, at, glm::vec3(.1F), {1, .66F, .24F}, .95F * hash(deal + k * 4.1F), 2.2F);
    }
    return;
  }
  const int arcs = std::min(10, 2 + int(cells / 3));
  for (int i = 0; i < arcs; ++i) {
    const float k = float(seed * 53 + i);
    glm::vec3 from(glm::mix(lo.x, hi.x, hash(deal + k * 1.7F)), y, glm::mix(lo.z, hi.z, hash(deal * 1.3F + k * 2.9F)));
    const float a = hash(deal * 2.1F + k) * 6.2831853F, length = .6F + .9F * hash(deal * 3.3F + k), up = hash(deal * 4.7F + k);
    const glm::vec3 to = from + glm::vec3(std::cos(a) * length, .5F * up * up, std::sin(a) * length);
    // Three jagged segments, a bright core inside a softer sheath.
    glm::vec3 p = from;
    for (int s = 1; s <= 3; ++s) {
      glm::vec3 q = glm::mix(from, to, float(s) / 3.F);
      if (s < 3) q += glm::vec3(hash(deal + k + float(s)) - .5F, (hash(deal * 1.1F + k + float(s)) - .3F) * .4F, hash(deal * 1.7F + k - float(s)) - .5F) * .35F;
      out.push_back({between(p, q, .05F), glm::vec4(1, .85F, .6F, 1), {0, 3.2F, 0, 1}, Primitive::Box, Blend::Additive});
      out.push_back({between(p, q, .16F), glm::vec4(1, .45F, .18F, .5F), {1, 2.4F, 0, 1}, Primitive::Box, Blend::Additive});
      p = q;
    }
    appendGlow(out, from, glm::vec3(.5F), {1, .35F, .12F}, .6F, 2.F);
    appendGlow(out, to, glm::vec3(.18F), {1, .65F, .3F}, .9F, 2.4F);
  }
}

void SceneModels::appendCheckpoint(const Checkpoint& c, float time, std::vector<Instance>& out) {
  // A slim beacon at the side of the run: dark base, white post and a lamp ring. Amber and slow
  // while waiting, mint once it holds the respawn point, with a soft column of light and a pulse
  // ring when it switches on.
  const glm::vec3 p = c.position, lamp = c.active ? glm::vec3(.35F, 1, .85F) : glm::vec3(1, .62F, .2F);
  appendBox(out, p + glm::vec3(0, .06F, 0), {.9F, .12F, .9F}, dark, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, .7F, 0), {.16F, 1.2F, .16F}, shell, 0, 1); out.back().primitive = Primitive::Cylinder;
  const float glow = c.active ? .9F : .35F + .25F * std::sin(time * 3.F);
  appendBox(out, p + glm::vec3(0, 1.36F, 0), {.34F, .12F, .34F}, lamp, glow, 1); out.back().primitive = Primitive::Cylinder;
  appendGlow(out, p + glm::vec3(0, 1.36F, 0), glm::vec3(.5F), lamp, c.active ? .45F : .2F, 1.4F);
  if (!c.active) return;
  out.push_back({glm::scale(glm::translate(glm::mat4(1), p + glm::vec3(0, 1.6F, 0)), {.5F, 3.F, .5F}), glm::vec4(lamp, .1F), {1, 1.8F, 0, 1}, Primitive::Cylinder, Blend::Additive});
  if (c.activeAge < .6F) {
    const float k = c.activeAge / .6F;
    appendGlow(out, p + glm::vec3(0, .1F, 0), glm::vec3(.6F + 2.2F * k, .06F, .6F + 2.2F * k), lamp, .6F * (1 - k), 2.F);
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
 if(p.motion.hitAge<.18F) {
  const float k=1-p.motion.hitAge/.18F;
  appendGlow(out,p.position+glm::vec3(0,.85F,0),glm::vec3(1.1F+.6F*(1-k)),{1,.62F,.28F},.55F*k,2.F);
  appendGlow(out,p.position+glm::vec3(0,.85F,0),glm::vec3(.45F),{1,.95F,.85F},.8F*k,2.6F);
 }
 if(p.backGlow>.01F&&p.deathTime<=0) {
  const auto transforms=robot_.transforms(root);
  const glm::vec3 back(transforms[std::size_t(pack_)]*glm::vec4(0,.03F,.2F,1));
  appendGlow(out,back,glm::vec3(.30F,.40F,.26F),{.5F,.85F,1},.36F*p.backGlow,1.3F);
 }
 for(const auto& particle:game.particles()) if(particle.remaining>0) {
  const float fade=std::min(1.F,particle.remaining/.18F);
  if(particle.kind==ParticleKind::Spark||particle.kind==ParticleKind::Ray) {
   // A coloured streak with a white-hot core, trailing back along the motion, in a soft halo.
   const glm::vec3 tail=particle.position-(particle.kind==ParticleKind::Ray?particle.extent*fade:particle.velocity*.09F*fade);
   out.push_back({between(particle.position,tail,particle.size*1.7F*fade),glm::vec4(particle.colour,fade),{0,2.9F,0,1},Primitive::Flat,Blend::Additive});
   out.push_back({between(particle.position,glm::mix(particle.position,tail,.6F),particle.size*.7F*fade),glm::vec4(glm::mix(particle.colour,glm::vec3(1),.7F),fade),{0,3.2F,0,1},Primitive::Flat,Blend::Additive});
   appendGlow(out,particle.position,glm::vec3(particle.size*6),particle.colour,.45F*fade,1.4F);
  } else if(particle.kind==ParticleKind::Dust) {
   // Ground dust: a soft grey puff that swells and thins away.
   const float age=1-particle.remaining/particle.lifetime,r=particle.size+particle.growth*age;
   out.push_back({glm::scale(glm::translate(glm::mat4(1),particle.position),glm::vec3(r)),glm::vec4(particle.colour,.72F*(1-age)*(1-age)),{1,2.1F,.6F,1},Primitive::Blob,Blend::Alpha});
  } else {
   const float heat=std::clamp(particle.remaining/particle.lifetime,0.F,1.F);
   appendBox(out,particle.position,glm::vec3(particle.size*1.35F*fade),glm::mix(particle.colour,glm::vec3(1,.7F,.35F),.5F*heat),.75F*heat,.5F,particle.seed+(particle.lifetime-particle.remaining)*8);
  }
 }
 for(std::size_t i=0;i<game.cores().size();++i) {
  const auto& c=game.cores()[i];
  if(!c.collected) appendCore(c.position,game.elapsed()+float(i),out,c.position.y-.8F);
 }
 for(const auto& pad:game.jumpPads()) {
  appendBox(out,pad.position+glm::vec3(0,.012F,0),{1.8F,.024F,1.8F},dark,0,.6F);
  appendBox(out,pad.position+glm::vec3(0,.03F,0),{1.5F,.024F,1.5F},{.07F,.32F,.4F},.1F+pad.pulse*.36F);
  // Surface 6 paints antialiased concentric rings in the shader, without extra geometry.
  out.back().material.w=6.F;
  if(pad.pulse>0)appendGlow(out,pad.position+glm::vec3(0,.1F,0),{1.8F,.16F,1.8F},mint,.18F*pad.pulse,.4F);
 }
 const auto g=game.gatePosition();
 const glm::vec3 lamp=game.gateUnlocked()?glm::vec3(.3F,.7F,.4F):glm::vec3(.8F,.35F,.25F);
 for(float side:{-1.F,1.F}) {
  appendBox(out,g+glm::vec3(side*1.8F,1.5F,0),{.4F,3,.6F},{.88F,.88F,.83F});
  appendBox(out,g+glm::vec3(side*(.8F+game.gateOpen()*1.6F),1.4F,0),{1.6F,2.8F,.4F},{.35F,.43F,.48F},0,1);
 }
 appendBox(out,g+glm::vec3(0,3,0),{4,.4F,.6F},{.88F,.88F,.83F});
 appendBox(out,g+glm::vec3(0,3,.32F),{1.2F,.16F,.06F},lamp,.8F);
 appendBox(out,g+glm::vec3(0,.015F,-.9F),{3,.03F,1.3F},lamp);
 for(const auto& sw:game.switches()) appendSwitch(sw.position,game.trapPhase(sw.trap)==TrapPhase::Disabled,sw.animation,out);
 for(std::size_t i=0;i<game.traps().size();++i) appendTrapPlate(game.traps()[i],game.trapPhase(i),game.elapsed(),int(i),out);
 for(const auto& plate:game.plates()) appendPlate(plate,game.doors()[std::size_t(plate.door)].hold,game.elapsed(),out);
 for(const auto& door:game.doors()) appendDoor(door,game.elapsed(),out);
 for(const auto& link:game.portals()) appendPortal(link,game.elapsed(),out);
 for(const auto& c:game.checkpoints()) appendCheckpoint(c,game.elapsed(),out);
 {
  // Over the void floor around the courier: glowing crimson embers and dark ash flakes drift up,
  // sway and fade, like the full edition's crimson tiles. Drawn from the clock (no particles).
  const float time=game.elapsed();
  const glm::vec3 me=game.player().position;
  for(int i=0;i<64;++i) {
   const float h=std::fmod(float(i)*.618034F,1.F),k=std::fmod(float(i)*.754878F+.31F,1.F);
   const float speed=.22F+.2F*k,life=std::fmod(time*speed+h,1.F),fade=std::sin(life*3.14159F);
   // Each one rises from its own spot on a 24 x 24 patch that follows the courier in whole cells.
   const float cx=std::floor(me.x)-12.F+24.F*h,cz=std::floor(me.z)-12.F+24.F*std::fmod(k+h*.37F,1.F);
   if(game.world().cell(int(std::floor(cx)),0,int(std::floor(cz)))!=LevelData::Void||game.world().cell(int(std::floor(cx)),1,int(std::floor(cz)))) continue;
   const float sway=std::sin(time*(1.3F+k)+float(i))*.18F*life;
   const glm::vec3 at(cx+sway,1.06F+1.9F*life,cz+sway*.6F);
   if(i%3==2) appendBox(out,at,glm::vec3(.08F+.05F*k,.012F,.06F+.04F*h),{.05F,.01F,.015F},0,0,time*(2.F+2.F*k)+float(i));
   else {
    const float flick=.7F+.3F*std::sin(time*23.F+float(i)*3.1F);
    appendGlow(out,at,glm::vec3((.08F+.05F*k)*(1.2F-.6F*life)),i%4==0?glm::vec3(1,.3F,.25F):glm::vec3(.9F,.04F,.1F),fade*flick,2.F);
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
