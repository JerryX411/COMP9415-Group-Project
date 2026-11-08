#version 330 core
// Procedural star sky: a deep navy gradient, a faint band of milky light with soft violet and teal
// nebula haze, and three layers of stars (small and many, medium, a few bright ones) that twinkle
// slowly. Everything is computed from the view direction, so there are no textures.
in vec2 vNdc;
uniform mat4 uInverseViewProjection;
uniform bool uScreenSpace;  // orthographic views: map the screen onto a patch of sky instead
uniform float uAspect, uTime;
out vec4 fragColour;

float hash31(vec3 p) { p = fract(p * vec3(.1031, .1030, .0973)); p += dot(p, p.yxz + 33.33); return fract((p.x + p.y) * p.z); }
vec3 hash33(vec3 p) { p = fract(p * vec3(.1031, .1030, .0973)); p += dot(p, p.yxz + 33.33); return fract((p.xxy + p.yxx) * p.zyx); }
float noise(vec3 p) {
  vec3 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  float n000 = hash31(i), n100 = hash31(i + vec3(1, 0, 0)), n010 = hash31(i + vec3(0, 1, 0)), n110 = hash31(i + vec3(1, 1, 0));
  float n001 = hash31(i + vec3(0, 0, 1)), n101 = hash31(i + vec3(1, 0, 1)), n011 = hash31(i + vec3(0, 1, 1)), n111 = hash31(i + vec3(1, 1, 1));
  return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y), mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}
float fbm(vec3 p) { float v = 0.0, a = .5; for (int i = 0; i < 5; ++i) { v += a * noise(p); p = p * 2.03 + 17.1; a *= .5; } return v; }
// One layer of stars: a jittered point per cell of a grid laid through direction space.
vec3 stars(vec3 d, float scale, float density, float size, float twinkleRate) {
  vec3 p = d * scale, cell = floor(p);
  vec3 sum = vec3(0);
  for (int x = -1; x <= 1; ++x) for (int y = -1; y <= 1; ++y) for (int z = -1; z <= 1; ++z) {
    vec3 c = cell + vec3(x, y, z);
    if (hash31(c + 7.3) > density) continue;
    vec3 r = hash33(c);
    vec3 star = c + .2 + .6 * r;
    float dist = length(p - star) / scale;
    float k = smoothstep(size, 0.0, dist);
    float twinkle = .65 + .35 * sin(uTime * twinkleRate * (.6 + r.x) + r.y * 40.0);
    vec3 tint = r.z < .25 ? vec3(.7, .8, 1.0) : r.z > .85 ? vec3(1.0, .85, .65) : vec3(.95, .97, 1.0);
    sum += tint * k * k * twinkle * (.55 + .45 * r.x);
  }
  return sum;
}
void main() {
  vec3 d;
  if (uScreenSpace) d = normalize(vec3(vNdc.x * uAspect * .8, vNdc.y * .8 + .35, -1.0));
  else {
    vec4 nearPoint = uInverseViewProjection * vec4(vNdc, -1.0, 1.0), farPoint = uInverseViewProjection * vec4(vNdc, 1.0, 1.0);
    d = normalize(farPoint.xyz / farPoint.w - nearPoint.xyz / nearPoint.w);
  }
  // Night gradient: almost black overhead, a little bluer toward the horizon, darker below.
  float up = d.y;
  vec3 colour = mix(vec3(.030, .045, .085), vec3(.010, .016, .035), smoothstep(-.05, .8, up));
  colour = mix(colour, vec3(.012, .018, .03), smoothstep(-.05, -.6, up));
  // A tilted band of milky light with violet and teal haze in it.
  vec3 bandNormal = normalize(vec3(.35, .8, .5));
  float band = exp(-pow(dot(d, bandNormal) / .32, 2.0));
  float haze = fbm(d * 3.2 + 4.0), wisps = fbm(d * 7.5 - 2.0);
  colour += band * (.06 + .17 * haze * haze) * mix(vec3(.34, .22, .62), vec3(.14, .45, .60), wisps);
  colour += band * smoothstep(.5, .85, wisps) * .07 * vec3(.65, .75, 1.0);
  // Soft nebula patches away from the band too.
  colour += smoothstep(.5, .85, fbm(d * 2.1 + 11.0)) * .05 * vec3(.38, .2, .58);
  // Stars, denser inside the band.
  colour += stars(d, 90.0, .12 + .4 * band, .005, 1.3) * .6;
  colour += stars(d, 38.0, .16 + .2 * band, .008, .9) * .9;
  colour += stars(d, 14.0, .1, .011, .6) * 1.5;
  fragColour = vec4(colour, 1.0);
}
