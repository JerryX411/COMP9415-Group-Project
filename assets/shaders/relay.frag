#version 330 core
in vec3 vPosition, vNormal, vTangent, vColour;
in vec2 vUv;
in float vAlpha;
flat in vec4 vMaterial;
uniform sampler2D uAlbedo, uNormal;
uniform bool uNormalMapping;
uniform vec3 uEye, uFogColour;
uniform float uFogDensity;
// Map-edge cutaway: room floor bounds (min x, min z, max x, max z). When the camera is beyond
// a boundary, that side's outer wall and its fixtures are not drawn, so they never hide the
// courier (who never comes within 0.3 of a wall). The floor stays.
uniform bool uCutaway;
uniform vec4 uRoom;
// Effect lights: xyz position + radius, rgb colour + intensity.
uniform int uLightCount;
uniform vec4 uLightPosition[12], uLightColour[12];
out vec4 fragColour;
vec3 effectLight(vec3 n) {
  vec3 sum=vec3(0.0);
  for(int i=0;i<uLightCount;++i) {
    vec3 d=uLightPosition[i].xyz-vPosition;
    float distance=length(d);
    float falloff=clamp(1.0-distance/uLightPosition[i].w,0.0,1.0);
    sum+=uLightColour[i].rgb*uLightColour[i].a*falloff*falloff*(.3+.7*max(dot(n,d/max(distance,.001)),0.0));
  }
  return sum;
}
void main() {
  if(uCutaway&&vPosition.y>1.001) {
    const float edge=.06;
    if((uEye.x<uRoom.x&&vPosition.x<uRoom.x+edge)||(uEye.x>uRoom.z&&vPosition.x>uRoom.z-edge)||
       (uEye.z<uRoom.y&&vPosition.z<uRoom.y+edge)||(uEye.z>uRoom.w&&vPosition.z>uRoom.w-edge)) discard;
  }
  vec3 n=normalize(vNormal);
  vec3 view=normalize(uEye-vPosition);
  vec3 light=normalize(vec3(-.45,1.0,.35));
  float fog=1.0-exp(-length(uEye-vPosition)*uFogDensity);
  // Emission above 1 is unlit glow: particles, beams and halos. A positive
  // material.z shades smoke and dust as volumes instead.
  if(vMaterial.y>1.0) {
    float facing=abs(dot(n,view));
    // material.x > 0 fades the edges (soft blobs); < 0 fades the centre (shells).
    float soft=vMaterial.x>=0.0?mix(1.0,facing*facing,vMaterial.x):mix(1.0,pow(1.0-facing,2.0),-vMaterial.x);
    vec3 glow=vColour*(vMaterial.y-1.0);
    if(vMaterial.z>0.0) glow=glow*mix(1.0,.5+.45*max(dot(n,light),0.0)+.2*n.y,vMaterial.z)+vColour*effectLight(n)*.7*vMaterial.z;
    fragColour=vec4(mix(glow,uFogColour,fog*.6),vAlpha*soft);
    return;
  }
  vec3 t=normalize(vTangent-n*dot(n,vTangent));
  if(uNormalMapping) {
    vec3 detail=texture(uNormal,vUv).xyz*2.0-1.0;
    detail.xy*=mix(1.0,.18,vMaterial.x);
    n=normalize(mat3(t,cross(n,t),n)*normalize(detail));
  }
  vec3 albedo=vColour*mix(texture(uAlbedo,vUv).rgb,vec3(.98),vMaterial.x*.8);
  float diffuse=max(dot(n,light),0.0);
  float specular=pow(max(dot(n,normalize(light+view)),0.0),mix(12.0,56.0,vMaterial.x));
  float sky=.5+.5*n.y;
  vec3 ambient=mix(vec3(.20,.22,.24),vec3(.48,.52,.56),sky);
  float rim=pow(1.0-max(dot(n,view),0.0),3.0)*.13*vMaterial.x;
  float fill=max(dot(n,normalize(vec3(.6,.4,-.8))),0.0)*.24;
  vec3 colour=albedo*(ambient+.66*diffuse+fill)+vec3(.85,.93,1.0)*(specular*vMaterial.z+rim)+albedo*vMaterial.y;
  colour+=albedo*effectLight(n);
  colour=mix(colour,uFogColour,fog);
  fragColour=vec4(colour,vAlpha);
}
