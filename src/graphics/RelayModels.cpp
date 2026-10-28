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
  std::vector<glm::mat4> result;
  result.reserve(nodes.size());
  for (const auto& n : nodes)
    result.push_back((n.parent < 0 ? root : result[std::size_t(n.parent)]) * nodeTransform(n));
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
  int pack=part("backpack",body_,{0,.015F,.29F},{.40F,.43F,.22F},dark);
  part("backpack_lid",pack,{0,.23F,0},{.42F,.07F,.24F},shell);
  part("backpack_light",pack,{0,.03F,.125F},{.06F,.27F,.035F},mint,.45F);
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
  part("antenna_socket",head_,{.19F,.285F,.06F},{.11F,.07F,.11F},dark);
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

void SceneModels::appendGate(glm::vec3 position, float openness, bool unlocked, std::vector<Instance>& out) {
  gate_.nodes[doorL_].offset.x = -.65F - openness * 1.35F;
  gate_.nodes[doorR_].offset.x = .65F + openness * 1.35F;
  gate_.nodes[lamp_].colour = unlocked ? mint : amber;
  gate_.append(glm::translate(glm::mat4(1), position), out);
  appendBox(out, position + glm::vec3(0, .015F, -.9F), {2.4F, .03F, 1.5F}, unlocked ? teal : dark, .1F, 1);
}
void SceneModels::appendCore(glm::vec3 position, float time, std::vector<Instance>& out, float floorHeight) {
  position.y += std::sin(time * 2.4F) * .12F;
  const float angle = time * 1.4F;
  appendBox(out, position, {.36F, .46F, .36F}, mint, .4F, 1, angle); out.back().primitive=Primitive::Sphere;
  for(float side:{-1.F,1.F}) { appendBox(out,position+glm::vec3(side*.19F,0,0),{.10F,.36F,.40F},shell,0,1,angle); out.back().primitive=Primitive::Sphere; }
  appendBox(out, position + glm::vec3(0, .29F, 0), {.48F, .10F, .48F}, shell, 0, 1, angle);
  appendBox(out, position - glm::vec3(0, .29F, 0), {.48F, .10F, .48F}, shell, 0, 1, angle);
  appendBox(out, {position.x, floorHeight, position.z}, {.8F, .035F, .8F}, teal, .12F, 1, .785F);
}
void SceneModels::appendSwitch(glm::vec3 position, bool active, float press, std::vector<Instance>& out) {
  appendBox(out, position + glm::vec3(0, .09F, 0), {.75F, .18F, .65F}, dark, 0, 1);
  appendBox(out, position + glm::vec3(0, .46F, 0), {.48F, .62F, .4F}, shell, 0, 1); out.back().primitive=Primitive::Cylinder;
  appendBox(out, position + glm::vec3(0, .86F, 0), {.68F, .2F, .56F}, dark, 0, 1);
  appendBox(out, position + glm::vec3(0, 1.0F - press * .07F, 0), {.34F, .10F, .30F}, active ? mint : amber, .45F, 1);
  appendBox(out, position + glm::vec3(0, .52F, .215F), {.22F, .2F, .03F}, active ? teal : dark, .15F, 1);
}

void SceneModels::append(const Game& game, std::vector<Instance>& out, float wakeTime) {
  auto p = game.player(); p.motion.wakeTime=wakeTime;
  appendFacility(game,out);
  poseRobot(p);
  auto root = glm::translate(glm::mat4(1), p.position);
  root = glm::rotate(root, p.yaw, {0, 1, 0});
  const std::size_t start = out.size();
  robot_.append(root, out);
  if(p.interaction==Interaction::Core && p.interactionTime>0) {
    const auto transforms=robot_.transforms(root);
    const glm::vec3 hand(transforms[std::size_t(handR_)][3]);
    const auto delta=p.interactionTarget-hand;
    const float length=glm::length(delta);
    if(length>.01F) {
      const auto direction=delta/length;
      auto axis=glm::cross(glm::vec3(0,1,0),direction);
      auto beam=glm::translate(glm::mat4(1),(hand+p.interactionTarget)*.5F);
      if(glm::length(axis)>.001F) beam=glm::rotate(beam,std::acos(std::clamp(direction.y,-1.F,1.F)),glm::normalize(axis));
      out.push_back({glm::scale(beam,{.018F,length,.018F}),glm::vec4(mint,1),{1,.8F,.2F,1}});
      for(int i=0;i<4;++i) {
        const float t=std::fmod((.46F-p.interactionTime)*3+i*.25F,1.F);
        appendBox(out,glm::mix(p.interactionTarget,hand,t),{.05F,.05F,.05F},mint,.7F);
      }
    }
  }
  if (p.deathTime<=0 && p.health>0 && p.hurtCooldown > 0 && int(p.hurtCooldown * 15) % 2 == 0)
    for (std::size_t i = start; i < out.size(); ++i) out[i].colour = glm::vec4(1, .45F, .3F, 1);
  const float shadowY=game.shadowHeight();
  const float shadowScale=1.F/(1+std::max(0.F,p.position.y-shadowY)*.22F);
  appendBox(out,{p.position.x,shadowY+.015F,p.position.z},{.72F*shadowScale,.018F,.64F*shadowScale},{.18F,.23F,.24F}); out.back().primitive=Primitive::Cylinder;
  appendGate(game.gatePosition(), game.gateOpen(), game.gateUnlocked(), out);
  for (std::size_t i = 0; i < game.cores().size(); ++i)
    if (!game.cores()[i].collected) appendCore(game.cores()[i].position, game.elapsed() + float(i), out);
  for (const auto& sw : game.switches())
    appendSwitch(sw.position, game.trapPhase(std::size_t(sw.trap)) == TrapPhase::Disabled, sw.animation, out);
  for (std::size_t i = 0; i < game.traps().size(); ++i) {
    const auto& trap = game.traps()[i];
    const auto phase = game.trapPhase(i);
    const glm::vec3 colour = phase == TrapPhase::Active ? red : phase == TrapPhase::Warning ? amber : teal;
    const float glow = phase == TrapPhase::Active ? .7F : phase == TrapPhase::Warning ? .25F + .25F * std::sin(game.elapsed() * 18) : .1F;
    for (float x = trap.area.min.x + .5F; x < trap.area.max.x; x += 1)
      for (float z = trap.area.min.z + .5F; z < trap.area.max.z; z += 1) {
        const auto tile=trap.tileBounds(x,z);
        appendBox(out,(tile.min+tile.max)*.5F,tile.max-tile.min,dark,0,1);
        appendBox(out, {x, trap.area.min.y+.09F, z}, {.65F, .025F, .12F}, colour, glow, 1, .785F);
        appendBox(out, {x, trap.area.min.y+.095F, z}, {.12F, .025F, .65F}, colour, glow, 1, .785F);
      }
    // Cable segments make a relay's target identifiable without relying on text.
    for (const auto& sw : game.switches()) if (sw.trap == int(i)) {
      const auto end = (trap.area.min + trap.area.max) * .5F;
      const auto mid = glm::vec3(end.x, 1.025F, sw.position.z);
      appendBox(out, {(sw.position.x + mid.x) * .5F, 1.025F, mid.z}, {std::abs(mid.x - sw.position.x) + .08F, .04F, .08F}, colour, .12F, 1);
      appendBox(out, {mid.x, 1.025F, (mid.z + end.z) * .5F}, {.08F, .04F, std::abs(end.z - mid.z) + .08F}, colour, .12F, 1);
    }
  }
  for (const auto& h : game.movers()) {
    const auto centre = (h.start + h.end) * .5F;
    const auto delta = h.end - h.start;
    appendBox(out, {centre.x, 1.02F, centre.z}, {.10F, .04F, glm::length(delta)}, amber, .1F, 1, std::atan2(delta.x, delta.z));
    const auto parts=h.collisionParts();
    for(std::size_t i=0;i<parts.size();++i)
      appendBox(out,(parts[i].min+parts[i].max)*.5F,parts[i].max-parts[i].min,i==0?dark:i==1?red:amber,i==0?0:i==1?.5F:.25F,1);
  }
  for (const auto& particle : game.particles()) if (particle.remaining > 0) {
    const float size = particle.size * std::min(1.0F, particle.remaining / particle.lifetime * 2);
    appendBox(out, particle.position, glm::vec3(size), particle.colour, .35F);
  }
}

void SceneModels::appendGallery(float time,std::vector<Instance>& out,int motion) {
  appendBox(out,{0,-.13F,0},{4,.25F,4},shell,0,1);
  for(int i=-4;i<=4;++i) {
    appendBox(out,{i*.5F,.005F,0},{.008F,.01F,4},{.26F,.30F,.32F},0);
    appendBox(out,{0,.005F,i*.5F},{4,.01F,.008F},{.26F,.30F,.32F},0);
  }
  const auto p=previewPlayer(time,motion); poseRobot(p);
  auto root=glm::translate(glm::mat4(1),p.position);
  root=glm::rotate(root,p.yaw,{0,1,0}); robot_.append(root,out);
  appendBox(out,{0,.015F,0},{.7F,.02F,.6F},{.20F,.24F,.25F}); out.back().primitive=Primitive::Cylinder;
}

void SceneModels::appendFacility(const Game& game,std::vector<Instance>& out) {
  // Fixtures stay on the existing boundary walls; no new invisible obstacles.
  for(float x:{7.F,13.F,19.F,25.F}) {
    appendBox(out,{x,3.45F,3.99F},{4.4F,.22F,.045F},dark,0,1);
    appendBox(out,{x,3.45F,4.018F},{4.0F,.075F,.02F},{.76F,.89F,1},.55F,1);
    appendBox(out,{x,3.45F,28.005F},{4.0F,.075F,.02F},{.76F,.89F,1},.55F,1);
  }
  for(float z:{8.F,16.F,24.F}) {
    appendBox(out,{3.99F,2.9F,z},{.04F,1.5F,4.6F},dark,0,1);
    appendBox(out,{4.018F,2.9F,z},{.02F,1.22F,4.25F},{.18F,.29F,.34F},.06F,1);
    appendBox(out,{4.03F,2.9F,z},{.02F,1.24F,.055F},shell,0,1);
    appendBox(out,{27.99F,3.5F,z},{.035F,.10F,4.8F},mint,.35F,1);
  }
  for(int i=0;i<22;++i) {
    appendBox(out,{5.0F,1.008F,5.4F+i},{.07F,.012F,.55F},teal,.1F);
    appendBox(out,{26.8F,1.008F,5.4F+i},{.07F,.012F,.55F},teal,.1F);
  }
  // A simple, readable test-sector sign on the north wall.
  appendBox(out,{8,2.6F,4.015F},{2.65F,2.35F,.025F},shell,0,1);
  const int masks[]{0x3f,0x06,0x5b};
  const auto digit=[&](float x,int number) {
    const glm::vec2 pos[7]{{0,.5F},{.27F,.25F},{.27F,-.25F},{0,-.5F},{-.27F,-.25F},{-.27F,.25F},{0,0}};
    for(int j=0;j<7;++j) if(masks[number]&(1<<j)) {
      const bool horizontal=j==0||j==3||j==6;
      appendBox(out,{x+pos[j].x,2.8F+pos[j].y,4.039F},horizontal?glm::vec3(.43F,.085F,.018F):glm::vec3(.085F,.43F,.018F),dark,0,1);
    }
  };
  digit(7.55F,0); digit(8.45F,game.level()+1);
  appendBox(out,{8,1.85F,4.04F},{1.8F,.055F,.025F},teal,.1F);
  for(int i=0;i<5;++i) appendBox(out,{7.3F+i*.35F,1.65F,4.04F},{.21F,.12F,.02F},i==0?amber:dark,0,1);
}

void SceneModels::appendMenu(float time,std::vector<Instance>& out) {
  appendBox(out,{2,-.30F,0},{28,.3F,20},{.53F,.55F,.53F},0,1);
  for(int x=-5;x<8;++x) for(int y=0;y<4;++y)
    appendBox(out,{x*1.8F, y*1.6F+.3F,-3.0F},{1.77F,1.57F,.20F},y==0?dark:shell,0,1);
  for(float x:{-3.F,1.8F,6.6F}) {
    appendBox(out,{x,2.2F,-2.86F},{.14F,3.8F,.08F},dark,0,1);
    appendBox(out,{x,2.2F,-2.80F},{.055F,3.4F,.025F},mint,.6F,1);
  }
  for(int i=-10;i<12;++i) appendBox(out,{float(i),-.14F,1},{.015F,.018F,18},{.23F,.26F,.27F});
  const glm::vec3 centre(2.5F,0,0);
  appendBox(out,centre+glm::vec3(0,-.10F,0),{3.8F,.20F,3.8F},dark,0,1); out.back().primitive=Primitive::Cylinder;
  appendBox(out,centre+glm::vec3(0,.008F,0),{3.55F,.025F,3.55F},shell,0,1); out.back().primitive=Primitive::Cylinder;
  for(int i=0;i<40;++i) {
    const float a=i*6.2831853F/40;
    appendBox(out,centre+glm::vec3(std::sin(a)*1.84F,.02F,std::cos(a)*1.84F),{.055F,.028F,.10F},i%5?teal:amber,.25F,1,a);
  }
  Player p; p.grounded=true; p.idlePhase=time; p.motion.idleFor=time; poseRobot(p);
  auto root=glm::translate(glm::mat4(1),centre+glm::vec3(0,.03F,0));
  root=glm::rotate(root,3.55F+std::sin(time*.3F)*.08F,{0,1,0});
  robot_.append(glm::scale(root,glm::vec3(1.65F)),out);
}
}  // namespace relay
