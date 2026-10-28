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
void appendParticle(const Particle& p, std::vector<Instance>& out) {
  const float life = p.remaining / p.lifetime, age = p.lifetime - p.remaining;
  switch (p.kind) {
    case ParticleKind::Debris: {
      // Lit chunks tumble on a per-particle axis; confetti is flat and flutters.
      const glm::vec3 axis = glm::normalize(glm::vec3(.4F + p.seed, 1, .25F - p.seed));
      auto m = glm::rotate(glm::translate(glm::mat4(1), p.position), age * (p.growth > 0 ? 14.F : 9.F) + p.seed * 6.F, axis);
      const float size = p.size * std::min(1.F, life * 2);
      const glm::vec3 shape = p.growth > 0 ? glm::vec3(size * 1.6F, size * .12F, size) : glm::vec3(size) * (.8F + .4F * p.seed);
      out.push_back({glm::scale(m, shape), glm::vec4(p.colour * (.9F + .2F * p.seed), 1), {0, p.growth > 0 ? .5F : .2F, .1F, 1}});
      break;
    }
    case ParticleKind::Dust: {
      // Two offset lumps break up the sphere; puffs grow as they slow and fade out slowly.
      // Heavy dust (extent.x > 0: jet wash, running dust) is denser and lingers longer.
      const float size = p.size * (1 + p.growth * age), heavy = p.extent.x;
      const float alpha = std::min(.85F, .46F * (1 + heavy)) * std::min(1.F, age / .05F) * (heavy > 0 ? life : life * life);
      const glm::vec3 colour = p.colour * (.92F + .16F * p.seed);
      const glm::vec3 offset = glm::vec3(std::sin(p.seed * 31.F), .35F, std::cos(p.seed * 17.F)) * size * .38F;
      // material.z shades puffs like a volume: lit from above, tinted by nearby effect lights.
      out.push_back({glm::scale(glm::translate(glm::mat4(1), p.position), glm::vec3(size)), glm::vec4(colour, alpha), {1, 2, .9F, 1}, Primitive::Blob, Blend::Alpha});
      out.push_back({glm::scale(glm::translate(glm::mat4(1), p.position + offset), glm::vec3(size * .72F)), glm::vec4(colour * 1.05F, alpha * .75F), {1, 2, .9F, 1}, Primitive::Blob, Blend::Alpha});
      break;
    }
    case ParticleKind::Fireball: {
      // Swells fast, burns white to its colour to dull red, and fades as it rises.
      const float t = 1 - life, size = p.size * (1 + p.growth * std::sqrt(age));
      const glm::vec3 colour = t < .25F ? glm::mix(glm::vec3(1, .97F, .85F), p.colour, t / .25F) : glm::mix(p.colour, p.colour * glm::vec3(.6F, .22F, .12F), (t - .25F) / .75F);
      appendGlow(out, p.position, glm::vec3(size), colour, std::pow(life, 1.3F), 1.5F);
      appendGlow(out, p.position, glm::vec3(size * .55F), glm::mix(colour, glm::vec3(1), .5F * life), life * life, 1.8F);
      break;
    }
    case ParticleKind::Scorch: {
      // Dark scuff on the floor that spreads slightly and fades out.
      const float size = p.size * (1 + p.growth * age);
      out.push_back({glm::scale(glm::translate(glm::mat4(1), {p.position.x, 1.006F, p.position.z}), {size, .004F, size}), glm::vec4(p.colour, .6F * std::min(1.F, life * 1.5F)), {1, 1.6F, 0, 1}, Primitive::Cylinder, Blend::Alpha});
      out.push_back({glm::scale(glm::translate(glm::mat4(1), {p.position.x, 1.008F, p.position.z}), {size * .55F, .004F, size * .55F}), glm::vec4(1, .45F, .15F, .5F * std::max(0.F, life * 2 - 1)), {1, 2, 0, 1}, Primitive::Cylinder, Blend::Additive});
      break;
    }
    case ParticleKind::Shell: {
      // Shockwave sphere: bright rim (negative material.x), racing out and thinning.
      const float t = 1 - life, ease = 1 - (1 - t) * (1 - t) * (1 - t);
      const float radius = glm::mix(p.size, p.growth, ease);
      out.push_back({glm::scale(glm::translate(glm::mat4(1), p.position), glm::vec3(radius * 2)), glm::vec4(glm::mix(glm::vec3(1), p.colour, t), .9F * life * life), {-1, 2.2F, 0, 1}, Primitive::Sphere, Blend::Additive});
      break;
    }
    case ParticleKind::Ray: {
      // Light streak: the head races out along extent, the tail chases it, the width tapers.
      const float t = 1 - life, head = 1 - (1 - t) * (1 - t), tail = head * head * .9F;
      out.push_back({between(p.position + p.extent * tail, p.position + p.extent * head, p.size * life + .008F), glm::vec4(p.colour, life), {0, 2.8F, 0, 1}, Primitive::Box, Blend::Additive});
      break;
    }
    case ParticleKind::Pillar: {
      // Column of light that shoots up, then narrows to a thread.
      const float rise = std::min(1.F, age / .12F), width = p.size * life * life;
      const glm::vec3 base = p.position, top = p.position + glm::vec3(0, 7.F * rise, 0);
      out.push_back({between(base, top, width), glm::vec4(p.colour, .55F * life), {1, 2.2F, 0, 1}, Primitive::Cylinder, Blend::Additive});
      out.push_back({between(base, top, width * .3F + .02F), glm::vec4(1, 1, 1, .9F * life), {0, 2.4F, 0, 1}, Primitive::Cylinder, Blend::Additive});
      break;
    }
    case ParticleKind::Spark: {
      // Hot sparks start white, cool through their colour to a dull red; streak length follows speed.
      const float speed = glm::length(p.velocity);
      const auto tail = p.position - (speed > .01F ? p.velocity / speed : glm::vec3(0, 1, 0)) * std::clamp(speed * .06F, p.size * 2, .9F);
      const glm::vec3 cooled = glm::mix(p.colour * glm::vec3(.85F, .32F, .14F), p.colour, std::min(1.F, life * 2));
      const glm::vec3 colour = glm::mix(cooled, glm::vec3(1, .97F, .88F), life * life * .75F);
      out.push_back({between(tail, p.position, p.size * (.5F + .5F * life)), glm::vec4(colour, std::min(1.F, life * 1.6F)), {0, 2.7F, 0, 1}, Primitive::Box, Blend::Additive});
      appendGlow(out, p.position, glm::vec3(p.size * 6.F), colour, .28F * life, 1.F);
      break;
    }
    case ParticleKind::Glow: {
      const float flicker = .86F + .14F * std::sin(age * 37.F + p.seed * 20.F);
      appendGlow(out, p.position, glm::vec3(p.size * (.35F + .65F * life)), p.colour, std::min(1.F, life * 1.4F) * flicker, 1.2F);
      break;
    }
    case ParticleKind::Ember: {
      // Tiny cooling points that flicker as they drift.
      const float flicker = .55F + .45F * std::sin(age * 52.F + p.seed * 30.F);
      const glm::vec3 colour = glm::mix(p.colour * glm::vec3(.8F, .3F, .15F), glm::mix(p.colour, glm::vec3(1), .3F), life);
      appendGlow(out, p.position, glm::vec3(p.size * (.6F + .4F * life)), colour, life * flicker, 1.6F);
      appendGlow(out, p.position, glm::vec3(p.size * 4.F), colour, .18F * life * flicker, 1.F);
      break;
    }
    case ParticleKind::Arc: {
      // Jagged bolt re-rolled about 45 times a second: white core, coloured sheath.
      const float length = glm::length(p.extent);
      if (length < .01F) break;
      const auto direction = p.extent / length;
      const auto side = glm::normalize(glm::cross(direction, std::abs(direction.y) > .9F ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0)));
      const auto up = glm::cross(direction, side);
      const float tick = std::floor(age * 45.F);
      const auto hash = [&](float n) { const float h = std::sin(n * 12.9898F + p.seed * 78.233F) * 43758.5453F; return h - std::floor(h) - .5F; };
      glm::vec3 previous = p.position;
      constexpr int segments = 7;
      for (int i = 1; i <= segments; ++i) {
        const float t = float(i) / segments;
        glm::vec3 point = p.position + p.extent * t;
        if (i < segments) point += (side * hash(tick * 13.F + float(i)) + up * hash(tick * 7.F + float(i) * 3.F)) * length * .45F * std::sin(t * 3.14159F);
        out.push_back({between(previous, point, .016F), glm::vec4(.92F, .96F, 1, life), {0, 2.8F, 0, 1}, Primitive::Box, Blend::Additive});
        out.push_back({between(previous, point, .09F), glm::vec4(p.colour, .45F * life), {1, 2.2F, 0, 1}, Primitive::Cylinder, Blend::Additive});
        previous = point;
      }
      appendGlow(out, p.position, glm::vec3(.35F), p.colour, .5F * life, 1.2F);
      break;
    }
    case ParticleKind::Ring: {
      // Expanding flat ring built from short arcs; it thins as it fades.
      const float t = 1 - life, ease = 1 - (1 - t) * (1 - t) * (1 - t);
      const float radius = glm::mix(p.size, p.growth, ease), width = .07F * (1 - .6F * t);
      constexpr int segments = 22;
      for (int i = 0; i < segments; ++i) {
        const float a = float(i) * 6.2831853F / segments, b = float(i + 1) * 6.2831853F / segments;
        const glm::vec3 from = p.position + glm::vec3(std::cos(a), 0, std::sin(a)) * radius;
        const glm::vec3 to = p.position + glm::vec3(std::cos(b), 0, std::sin(b)) * radius;
        auto m = glm::rotate(glm::translate(glm::mat4(1), (from + to) * .5F), -(a + b) * .5F, {0, 1, 0});
        out.push_back({glm::scale(m, {width, .018F, glm::distance(from, to) * 1.04F}), glm::vec4(p.colour, .85F * life * life), {0, 2.3F, 0, 1}, Primitive::Box, Blend::Additive});
      }
      break;
    }
  }
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
void SceneModels::appendSwitch(glm::vec3 position, bool active, float press, std::vector<Instance>& out) {
  appendBox(out, position + glm::vec3(0, .09F, 0), {.75F, .18F, .65F}, dark, 0, 1);
  appendBox(out, position + glm::vec3(0, .46F, 0), {.48F, .62F, .4F}, shell, 0, 1); out.back().primitive=Primitive::Cylinder;
  appendBox(out, position + glm::vec3(0, .86F, 0), {.68F, .2F, .56F}, dark, 0, 1);
  appendBox(out, position + glm::vec3(0, 1.0F - press * .07F, 0), {.34F, .10F, .30F}, active ? mint : amber, .45F, 1);
  appendBox(out, position + glm::vec3(0, .52F, .215F), {.22F, .2F, .03F}, active ? teal : dark, .15F, 1);
}

void SceneModels::appendCheckpoint(const Checkpoint& c, float time, std::vector<Instance>& out) {
  // Beacon: a floor pad with a light ring, a slim pylon and a lamp head. Dormant beacons
  // blink amber; once activated the ring lights up in a sweep and a hologram unfolds.
  const glm::vec3 p = c.position;
  const bool on = c.active;
  const float boot = on ? std::min(1.F, c.activeAge / .8F) : 0, ease = 1 - (1 - boot) * (1 - boot);
  const float blink = .5F + .5F * std::sin(time * 4.F);
  const glm::vec3 lamp = on ? glm::vec3(.35F, 1, .9F) : glm::vec3(1, .6F, .15F);
  appendBox(out, p + glm::vec3(0, .04F, 0), {1.5F, .08F, 1.5F}, dark, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, .085F, 0), {1.28F, .02F, 1.28F}, shell, 0, 1); out.back().primitive = Primitive::Cylinder;
  for (int i = 0; i < 16; ++i) {
    const float a = float(i) * 6.2831853F / 16;
    const bool lit = on ? float(i) / 16 < boot : (int(time * 6.F) % 16) == i;
    appendBox(out, p + glm::vec3(std::sin(a) * .68F, .095F, std::cos(a) * .68F), {.05F, .03F, .14F}, lit ? lamp : glm::vec3(.16F, .18F, .19F), lit ? .8F : 0, 1, a);
  }
  appendBox(out, p + glm::vec3(0, .55F, 0), {.22F, .9F, .22F}, shell, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, .78F, 0), {.25F, .07F, .25F}, teal, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, 1.06F, 0), {.36F, .12F, .36F}, dark, 0, 1); out.back().primitive = Primitive::Cylinder;
  appendBox(out, p + glm::vec3(0, 1.2F, 0), {.21F, .21F, .21F}, lamp, on ? .95F : .3F + .5F * blink, 1); out.back().primitive = Primitive::Sphere;
  appendGlow(out, p + glm::vec3(0, 1.2F, 0), glm::vec3(on ? .75F : .42F), lamp, on ? .45F : .22F * blink, 1.2F);
  if (!on) return;
  // Hologram: a soft beam, two counter-rotating rings and a spinning diamond.
  out.push_back({between(p + glm::vec3(0, 1.25F, 0), p + glm::vec3(0, 1.25F + 1.9F * ease, 0), .5F), glm::vec4(.35F, 1, .9F, .16F), {1, 2, 0, 1}, Primitive::Cylinder, Blend::Additive});
  for (int ring = 0; ring < 2; ++ring) {
    const float h = 1.55F + .45F * float(ring), r = (.42F - .1F * float(ring)) * ease, spin = time * (ring ? -1.6F : 1.2F);
    for (int i = 0; i < 12; ++i) {
      if (i % 4 == 3) continue;  // Gaps make the rotation readable.
      const float a = spin + float(i) * .5236F, b = a + .5236F;
      out.push_back({between(p + glm::vec3(std::cos(a) * r, h, std::sin(a) * r), p + glm::vec3(std::cos(b) * r, h, std::sin(b) * r), .025F), glm::vec4(.5F, 1, .95F, .9F), {0, 2.4F, 0, 1}, Primitive::Box, Blend::Additive});
    }
  }
  auto diamond = glm::rotate(glm::translate(glm::mat4(1), p + glm::vec3(0, 2.45F + .06F * std::sin(time * 2.F), 0)), time * 2.F, {0, 1, 0});
  diamond = glm::rotate(diamond, .785F, {1, 0, 1});
  out.push_back({glm::scale(diamond, glm::vec3(.18F * ease)), glm::vec4(.6F, 1, .95F, .85F), {0, 2.2F, 0, 1}, Primitive::Box, Blend::Additive});
  appendGlow(out, p + glm::vec3(0, 2.45F, 0), glm::vec3(.5F * ease), {.35F, 1, .9F}, .4F, 1.1F);
}

void SceneModels::appendSpeedPad(const SpeedPad& pad, float time, std::vector<Instance>& out) {
  // Launch strip: a dark bed with lit rails and cyan chevrons that chase along the arrow;
  // a launch floods them white.
  const glm::vec3 c((pad.area.min.x + pad.area.max.x) * .5F, 1, (pad.area.min.z + pad.area.max.z) * .5F);
  const glm::vec3 size = pad.area.max - pad.area.min;
  const bool alongX = std::abs(pad.direction.x) > .5F;
  const float length = alongX ? size.x : size.z, width = alongX ? size.z : size.x;
  const glm::vec3 d(pad.direction.x, 0, pad.direction.y), side(-d.z, 0, d.x);
  appendBox(out, c + glm::vec3(0, .012F, 0), {size.x, .024F, size.z}, dark, 0, 1);
  for (float s : {-1.F, 1.F})
    appendBox(out, c + glm::vec3(0, .03F, 0) + side * (s * (width * .5F - .06F)), alongX ? glm::vec3(length, .03F, .07F) : glm::vec3(.07F, .03F, length), teal, .6F + pad.flash, 1);
  const glm::vec3 lit = glm::mix(glm::vec3(.30F, .80F, 1), glm::vec3(1), pad.flash);
  const int n = std::max(2, int(length / .55F));
  for (int i = 0; i < n; ++i) {
    const float along = (float(i) + .5F) / float(n) * length - length * .5F;
    const float wave = std::fmod(float(i) / float(n) - time * 1.4F + 100.F, 1.F);
    const float glow = std::min(1.F, std::pow(1 - wave, 3.F) * .85F + .15F + pad.flash);
    const glm::vec3 apex = c + glm::vec3(0, .035F, 0) + d * (along + .16F);
    for (float s : {-1.F, 1.F})
      out.push_back({between(apex, apex - d * .32F + side * (s * width * .32F), .07F), glm::vec4(lit, glow), {0, 2.4F, 0, 1}, Primitive::Box, Blend::Additive});
  }
  appendGlow(out, c + glm::vec3(0, .12F, 0), glm::vec3(alongX ? length * .55F : width * .55F, .08F, alongX ? width * .55F : length * .55F), lit, .10F + .45F * pad.flash, 1.F);
  // Idle sparkle: motes in cyan, violet and pink stream along the arrow and drift up, so the strip
  // reads from across the room (drawn from time, not spawned, so a quiet level stays empty).
  const glm::vec3 hues[3]{{.45F, .9F, 1}, {.72F, .5F, 1}, {1, .45F, .85F}};
  for (int i = 0; i < 14; ++i) {
    const float h = std::fmod(float(i) * .618034F, 1.F), k = std::fmod(float(i) * .754878F + .31F, 1.F);
    const float life = std::fmod(time * (.7F + .5F * k) + h, 1.F);
    const glm::vec3 mote = c + d * ((life - .5F) * length) + side * ((k - .5F) * width * .8F) + glm::vec3(0, .08F + .9F * life * life, 0);
    appendGlow(out, mote, glm::vec3(.09F + .06F * k), hues[i % 3], (.85F + .9F * pad.flash) * std::sin(life * 3.14159F), 2.F);
  }
}

void SceneModels::appendPlate(const PressurePlate& plate, float doorHold, float time, std::vector<Instance>& out) {
  // Floor button: a dark ring and a white cap that sinks when pressed. Amber while its door is
  // sealed, cyan while it is held open; the ring's lamps count the hold down.
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

void SceneModels::appendPickup(const Pickup& item, float time, std::vector<Instance>& out) {
  // A small floor dock with a ring of lamps; above it the item spins and bobs inside a soft beam.
  // Spring boots are a green coil with an up-arrow, the shield a cyan hex plate, the clock a gold
  // dial with sweeping hands. While it is respawning only the dock shows, its ring filling back
  // up; a spent clock leaves the dock dark.
  const bool shield = item.kind == PowerUp::Shield, clock = item.kind == PowerUp::Time;
  const glm::vec3 colour = pickupColour(item.kind), p = item.position;
  appendBox(out, p + glm::vec3(0, .02F, 0), {.9F, .04F, .9F}, dark, 0, 1); out.back().primitive = Primitive::Cylinder;
  const float back = item.taken ? 0 : item.respawn > 0 ? 1 - item.respawn / Game::pickupRespawn : 1;
  for (int i = 0; i < 10; ++i) {
    const float a = float(i) * .6283F;
    const bool lit = float(i) / 10 < back;
    appendBox(out, p + glm::vec3(std::sin(a) * .38F, .045F, std::cos(a) * .38F), {.05F, .02F, .1F}, lit ? colour : glm::vec3(.16F, .18F, .19F), lit ? .8F : 0, 1, a);
  }
  if (item.respawn > 0 || item.taken) return;
  const float bob = .08F * std::sin(time * 2.6F), spin = time * 1.8F;
  const glm::vec3 c = p + glm::vec3(0, .9F + bob, 0);
  out.push_back({between(p + glm::vec3(0, .05F, 0), p + glm::vec3(0, 1.5F, 0), .5F), glm::vec4(colour, .1F), {1, 2, 0, 1}, Primitive::Cylinder, Blend::Additive});
  if (clock) {
    // Clock dial: a dark-gold face edge-on with a bright gold rim, twelve tick lamps, a hub and
    // two hands sweeping fast on both faces.
    auto m = glm::rotate(glm::translate(glm::mat4(1), c), spin, {0, 1, 0});
    const glm::vec3 right(m * glm::vec4(1, 0, 0, 0)), normal(m * glm::vec4(0, 0, 1, 0)), up(0, 1, 0);
    m = glm::rotate(m, 1.5708F, {1, 0, 0});
    out.push_back({glm::scale(m, {.5F, .07F, .5F}), glm::vec4(colour * .32F, 1), {0, .5F, 0, 1}, Primitive::Cylinder, Blend::Opaque});
    out.push_back({glm::scale(m, {.58F, .05F, .58F}), glm::vec4(colour, .75F), {0, 2.2F, 0, 1}, Primitive::Cylinder, Blend::Additive});
    for (float face : {-1.F, 1.F}) {
      const glm::vec3 front = c + normal * (.045F * face);
      for (int i = 0; i < 12; ++i) {
        const float a = float(i) * .5236F;
        appendBox(out, front + (right * std::sin(a) + up * std::cos(a)) * .2F, glm::vec3(i % 3 == 0 ? .04F : .025F), {1, .95F, .8F}, 1.2F, 1);
      }
      for (const auto& [rate, length] : {std::pair{6.F, .19F}, std::pair{1.1F, .12F}}) {
        const float a = time * rate * face;
        out.push_back({between(front, front + (right * std::sin(a) + up * std::cos(a)) * length, .03F), glm::vec4(1, .97F, .88F, 1), {0, 2.4F, 0, 1}, Primitive::Box, Blend::Additive});
      }
    }
    appendBox(out, c, glm::vec3(.08F), {1, .95F, .8F}, 1.5F, 1); out.back().primitive = Primitive::Sphere;
  } else if (shield) {
    // Hex plate: a flattened cylinder edge-on, a bright rim and a core.
    auto m = glm::rotate(glm::translate(glm::mat4(1), c), spin, {0, 1, 0});
    m = glm::rotate(m, 1.5708F, {1, 0, 0});
    out.push_back({glm::scale(m, {.5F, .08F, .5F}), glm::vec4(colour * .7F, 1), {0, .6F, 0, 1}, Primitive::Cylinder, Blend::Opaque});
    out.push_back({glm::scale(m, {.58F, .05F, .58F}), glm::vec4(colour, .7F), {0, 2.F, 0, 1}, Primitive::Cylinder, Blend::Additive});
    appendBox(out, c, glm::vec3(.16F), {.85F, 1, 1}, 1.5F, 1); out.back().primitive = Primitive::Sphere;
  } else {
    // Coil: three stacked rings and an arrow on top.
    for (int i = 0; i < 3; ++i) {
      const float squeeze = .5F + .5F * std::sin(time * 6.F + float(i));
      appendBox(out, c + glm::vec3(0, -.18F + float(i) * (.12F + .03F * squeeze), 0), {.34F, .05F, .34F}, colour, 1.2F, 1); out.back().primitive = Primitive::Cylinder;
    }
    const glm::vec3 tip = c + glm::vec3(0, .32F, 0);
    for (float side : {-1.F, 1.F}) out.push_back({between(tip, tip + glm::vec3(side * .14F * std::cos(spin), -.14F, side * .14F * std::sin(spin)), .05F), glm::vec4(colour, 1), {0, 2.2F, 0, 1}, Primitive::Box, Blend::Additive});
  }
  appendGlow(out, c, glm::vec3(.55F), colour, .35F, 1.2F);
}

void SceneModels::append(const Game& game, std::vector<Instance>& out, float wakeTime) {
  auto p = game.player(); p.motion.wakeTime=wakeTime;
  appendFacility(game,out);
  poseRobot(p);
  auto root = glm::translate(glm::mat4(1), p.position + glm::vec3(0, bodyHop(p), 0));
  root = glm::scale(glm::rotate(root, p.yaw, {0, 1, 0}) * fallTransform(p), bodyScale(p));
  // Power-up auras: a shield bubble with a slow shimmer and a pop-in, spring rings at the boots.
  if (p.shielded && p.deathTime <= 0) {
    const float in = std::min(1.F, p.motion.pickupAge / .25F), t = game.elapsed();
    const glm::vec3 c = p.position + glm::vec3(0, .78F + bodyHop(p), 0);
    out.push_back({glm::scale(glm::translate(glm::mat4(1), c), glm::vec3(1.85F * in, 2.F * in, 1.85F * in)), glm::vec4(.45F, .9F, 1, .3F + .08F * std::sin(t * 5.F)), {-1, 2.4F, 0, 1}, Primitive::Sphere, Blend::Additive});
    appendGlow(out, c, glm::vec3(1.1F * in), {.45F, .9F, 1}, .12F, 1.F);
    for (int i = 0; i < 3; ++i) {
      const float h = std::fmod(t * .6F + float(i) / 3, 1.F), r = .92F * std::sin(h * 3.14159F) * in;
      for (int k = 0; k < 10; ++k) { const float a = float(k) * .6283F + t, b = a + .5F;
        out.push_back({between(c + glm::vec3(std::cos(a) * r, (h - .5F) * 1.8F, std::sin(a) * r), c + glm::vec3(std::cos(b) * r, (h - .5F) * 1.8F, std::sin(b) * r), .02F), glm::vec4(.6F, .95F, 1, .8F), {0, 2.4F, 0, 1}, Primitive::Box, Blend::Additive}); }
    }
  }
  if (p.spring > 0 && p.deathTime <= 0) {
    const float fade = std::min(1.F, p.spring / 1.5F), t = game.elapsed();
    for (bool right : {false, true}) {
      const glm::vec3 boot = p.position + glm::vec3(0, bodyHop(p) + .06F, 0) + glm::vec3(std::cos(p.yaw), 0, -std::sin(p.yaw)) * (right ? .17F : -.17F) * Game::modelScale;
      appendGlow(out, boot, glm::vec3(.22F, .08F, .22F), {.5F, 1, .6F}, (.35F + .15F * std::sin(t * 8.F)) * fade, 1.5F);
    }
  }
  const std::size_t start = out.size();
  robot_.append(root, out);
  const std::size_t end = out.size();
  if(p.backGlow>.01F&&p.deathTime<=0) {
    // Backpack glow: a soft halo behind the pack's light strip, never strong enough to wash
    // out the courier.
    const auto transforms=robot_.transforms(root);
    const glm::vec3 back(transforms[std::size_t(pack_)]*glm::vec4(0,.03F,.2F,1));
    appendGlow(out,back,glm::vec3(.30F,.40F,.26F),{.5F,.85F,1},.36F*p.backGlow,1.3F);
    appendGlow(out,back-glm::vec3(transforms[std::size_t(pack_)]*glm::vec4(0,0,.06F,0)),glm::vec3(.08F,.22F,.06F),{.75F,.95F,1},.5F*p.backGlow,1.5F);
  }
  if(p.interaction==Interaction::Core && p.interactionTime>0) {
    const auto transforms=robot_.transforms(root);
    const glm::vec3 hand(transforms[std::size_t(handR_)][3]);
    const auto delta=p.interactionTarget-hand;
    const float length=glm::length(delta);
    if(length>.01F) {
      // Energy tether: bright core line, soft sheath and motes flowing to the hand.
      const float flicker=.8F+.2F*std::sin(game.elapsed()*60.F);
      out.push_back({between(p.interactionTarget,hand,.022F),glm::vec4(.85F,1,.97F,flicker),{0,2.4F,0,1},Primitive::Cylinder,Blend::Additive});
      out.push_back({between(p.interactionTarget,hand,.13F),glm::vec4(.30F,1,.85F,.35F*flicker),{1,2.F,0,1},Primitive::Cylinder,Blend::Additive});
      for(int i=0;i<6;++i) {
        const float t=std::fmod((.46F-p.interactionTime)*3.2F+float(i)/6.F,1.F);
        appendGlow(out,glm::mix(p.interactionTarget,hand,t*t),glm::vec3(.09F),{.45F,1,.9F},std::sin(t*3.14159F));
      }
      appendGlow(out,hand,glm::vec3(.30F),{.45F,1,.9F},.55F);
    }
  }
  if (p.motion.spawnAge < .6F) {
    const float beam = 1 - p.motion.spawnAge / .6F;
    for (std::size_t i = start; i < end; ++i) { out[i].colour = glm::mix(out[i].colour, glm::vec4(.6F, 1, .95F, 1), .8F * beam); out[i].material.y = std::max(out[i].material.y, .9F * beam); }
  } else if (p.deathTime<=0 && p.health>0 && p.hurtCooldown > 0) {
    // A white impact frame, then a gentle warm blink that keeps the silhouette readable.
    const float white=std::max(0.F,1-(1.1F-p.hurtCooldown)/.09F);
    // Shocks strobe the shell blue-white fast while the current runs; blunt hits blink warm.
    const bool electric=p.motion.hitKind==1,shocked=electric&&p.motion.shockAge<.8F;
    const bool blink=int(p.hurtCooldown*(shocked?32.F:14.F))%2==0;
    for (std::size_t i = start; i < end; ++i) {
      auto& part=out[i];
      if(white>0) { part.colour=glm::mix(part.colour,glm::vec4(1),.85F*white); part.material.y=std::max(part.material.y,.9F*white); }
      else if(blink&&shocked) { part.colour=glm::mix(part.colour,glm::vec4(.55F,.88F,1,1),.6F); part.material.y=std::max(part.material.y,.7F); }
      else if(blink) part.colour=glm::mix(part.colour,electric?glm::vec4(.5F,.8F,1,1):glm::vec4(1,.45F,.32F,1),.32F);
    }
  }
  if(p.motion.hitKind==0&&p.motion.hitAge<1.3F&&p.deathTime<=0) {
    // Dizzy stars orbit the head for a moment after a blunt hit.
    const float fade=std::min(1.F,(1.3F-p.motion.hitAge)/.3F)*std::min(1.F,p.motion.hitAge/.15F);
    const glm::vec3 crown=p.position+glm::vec3(0,(2.25F-.9F*p.crouchBlend)*Game::modelScale,0);
    for(int i=0;i<3;++i) {
      const float a=p.motion.hitAge*7.F+float(i)*2.094F;
      const glm::vec3 star=crown+glm::vec3(std::cos(a)*.38F,.06F*std::sin(a*2),std::sin(a)*.38F);
      appendGlow(out,star,glm::vec3(.16F),{1,.82F,.3F},.55F*fade,1.4F);
      out.push_back({between(star-glm::vec3(.11F,0,0),star+glm::vec3(.11F,0,0),.028F),glm::vec4(1,.95F,.6F,fade),{0,2.6F,0,1},Primitive::Box,Blend::Additive});
      out.push_back({between(star-glm::vec3(0,.11F,0),star+glm::vec3(0,.11F,0),.028F),glm::vec4(1,.95F,.6F,fade),{0,2.6F,0,1},Primitive::Box,Blend::Additive});
    }
  }
  const float shadowY=game.shadowHeight();
  const float shadowScale=1.F/(1+std::max(0.F,p.position.y-shadowY)*.22F);
  out.push_back({glm::scale(glm::translate(glm::mat4(1),{p.position.x,shadowY+.012F,p.position.z}),{.95F*shadowScale,.012F,.85F*shadowScale}),glm::vec4(.06F,.08F,.09F,.55F*shadowScale),{0,1.5F,0,1},Primitive::Cylinder,Blend::Alpha});
  appendGate(game.gatePosition(), game.gateOpen(), game.gateUnlocked(), out);
  for (std::size_t i = 0; i < game.cores().size(); ++i)
    if (!game.cores()[i].collected) appendCore(game.cores()[i].position, game.elapsed() + float(i), out);
  for (const auto& sw : game.switches())
    appendSwitch(sw.position, game.trapPhase(std::size_t(sw.trap)) == TrapPhase::Disabled, sw.animation, out);
  for (const auto& checkpoint : game.checkpoints()) appendCheckpoint(checkpoint, game.elapsed(), out);
  for (const auto& pad : game.speedPads()) appendSpeedPad(pad, game.elapsed(), out);
  for (const auto& plate : game.plates()) appendPlate(plate, game.doors()[std::size_t(plate.door)].hold, game.elapsed(), out);
  for (const auto& door : game.doors()) appendDoor(door, game.elapsed(), out);
  for (const auto& item : game.pickups()) appendPickup(item, game.elapsed(), out);
  if (game.tutorial() && game.state() == RunState::Playing) {
    // Training beacon on the current objective: a soft column, a spinning floor ring and a
    // bobbing marker that drops in each time the step changes.
    const glm::vec3 t = game.tutorialTarget();
    const float time = game.elapsed(), in = std::min(1.F, game.tutorialStepAge() / .5F), bob = .12F * std::sin(time * 3.F);
    const glm::vec3 guide(.35F, 1, .8F);
    out.push_back({between(t + glm::vec3(0, .05F, 0), t + glm::vec3(0, 3.4F * in, 0), .55F), glm::vec4(guide, .12F), {1, 2, 0, 1}, Primitive::Cylinder, Blend::Additive});
    for (int i = 0; i < 12; ++i) {
      if (i % 3 == 2) continue;
      const float a = time * 1.5F + float(i) * .5236F, b = a + .5236F, r = .75F + .1F * std::sin(time * 4.F);
      out.push_back({between(t + glm::vec3(std::cos(a) * r, .06F, std::sin(a) * r), t + glm::vec3(std::cos(b) * r, .06F, std::sin(b) * r), .05F), glm::vec4(guide, .9F * in), {0, 2.4F, 0, 1}, Primitive::Box, Blend::Additive});
    }
    auto marker = glm::rotate(glm::translate(glm::mat4(1), t + glm::vec3(0, 2.9F + bob + 2.F * (1 - in), 0)), time * 2.F, {0, 1, 0});
    marker = glm::rotate(marker, .785F, {1, 0, 1});
    out.push_back({glm::scale(marker, glm::vec3(.24F)), glm::vec4(guide, .9F * in), {0, 2.2F, 0, 1}, Primitive::Box, Blend::Additive});
    appendGlow(out, t + glm::vec3(0, 2.9F + bob, 0), glm::vec3(.5F), guide, .35F * in, 1.1F);
  }
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
  for (const auto& particle : game.particles()) if (particle.remaining > 0) appendParticle(particle, out);
  // Ambient dust hangs in the air around the courier and catches the light.
  for (int i = 0; i < 70; ++i) {
    const float h = std::fmod(float(i) * .618034F, 1.F), k = std::fmod(float(i) * .754878F + .31F, 1.F), j = std::fmod(float(i) * .5698F + .7F, 1.F);
    const float t = game.elapsed() * (.05F + .05F * j);
    glm::vec3 mote(std::fmod(h * 24.F + t + 100.F, 24.F) + 5.F, 1.3F + 3.F * std::fmod(k + t * .3F, 1.F), std::fmod(j * 24.F + std::sin(t + h * 6.F) + 100.F, 24.F) + 5.F);
    const float near = 1 - std::clamp(glm::distance(mote, p.position + glm::vec3(0, 1.2F, 0)) / 8.F, 0.F, 1.F);
    if (near > 0) appendGlow(out, mote, glm::vec3(.03F + .02F * k), {1, .97F, .9F}, .45F * near * (.6F + .4F * std::sin(game.elapsed() * 1.3F + float(i))), 1.F);
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
  root=glm::scale(glm::rotate(glm::translate(root,{0,bodyHop(p),0}),p.yaw,{0,1,0})*fallTransform(p),bodyScale(p)); robot_.append(root,out);
  const float shadow=1.F/(1+p.position.y*.22F);
  out.push_back({glm::scale(glm::translate(glm::mat4(1),{0,.012F,0}),{.95F*shadow,.012F,.85F*shadow}),glm::vec4(.06F,.08F,.09F,.5F*shadow),{0,1.5F,0,1},Primitive::Cylinder,Blend::Alpha});
}

void SceneModels::appendFacility(const Game& game,std::vector<Instance>& out) {
  // Fixtures stay on the boundary walls of room(); no new invisible obstacles.
  const auto room=game.room();
  const float x0=room.min.x,x1=room.max.x,z0=room.min.z,z1=room.max.z;
  for(float x=x0+3;x<x1-1;x+=6) {
    appendBox(out,{x,3.45F,z0-.01F},{4.4F,.22F,.045F},dark,0,1);
    appendBox(out,{x,3.45F,z0+.018F},{4.0F,.075F,.02F},{.76F,.89F,1},.55F,1);
    appendBox(out,{x,3.45F,z1+.005F},{4.0F,.075F,.02F},{.76F,.89F,1},.55F,1);
  }
  for(float z=z0+4;z<z1-3;z+=8) {
    appendBox(out,{x0-.01F,2.9F,z},{.04F,1.5F,4.6F},dark,0,1);
    appendBox(out,{x0+.018F,2.9F,z},{.02F,1.22F,4.25F},{.18F,.29F,.34F},.06F,1);
    appendBox(out,{x0+.03F,2.9F,z},{.02F,1.24F,.055F},shell,0,1);
    appendBox(out,{x1-.01F,3.5F,z},{.035F,.10F,4.8F},mint,.35F,1);
  }
  for(int i=0;i<int(z1-z0)-2;++i) {
    appendBox(out,{x0+1,1.008F,z0+1.4F+float(i)},{.07F,.012F,.55F},teal,.1F);
    appendBox(out,{x1-1.2F,1.008F,z0+1.4F+float(i)},{.07F,.012F,.55F},teal,.1F);
  }
  // A simple, readable test-sector sign on the north wall (training is sector 00).
  appendBox(out,{x0+4,2.6F,z0+.015F},{2.65F,2.35F,.025F},shell,0,1);
  const int masks[]{0x3f,0x06,0x5b};
  const auto digit=[&](float x,int number) {
    const glm::vec2 pos[7]{{0,.5F},{.27F,.25F},{.27F,-.25F},{0,-.5F},{-.27F,-.25F},{-.27F,.25F},{0,0}};
    for(int j=0;j<7;++j) if(masks[number]&(1<<j)) {
      const bool horizontal=j==0||j==3||j==6;
      appendBox(out,{x+pos[j].x,2.8F+pos[j].y,z0+.039F},horizontal?glm::vec3(.43F,.085F,.018F):glm::vec3(.085F,.43F,.018F),dark,0,1);
    }
  };
  digit(x0+3.55F,0); digit(x0+4.45F,game.tutorial()?0:game.level()+1);
  appendBox(out,{x0+4,1.85F,z0+.04F},{1.8F,.055F,.025F},teal,.1F);
  for(int i=0;i<5;++i) appendBox(out,{x0+3.3F+float(i)*.35F,1.65F,z0+.04F},{.21F,.12F,.02F},i==0?amber:dark,0,1);
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
