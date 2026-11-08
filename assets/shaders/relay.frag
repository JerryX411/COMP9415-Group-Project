#version 330 core
in vec3 vPosition, vNormal, vTangent, vColour;
in vec2 vUv;
in float vAlpha;
flat in vec4 vMaterial;
flat in float vSurface;
uniform sampler2DArray uAlbedo, uNormal;
uniform bool uNormalMapping, uNormalCompare;
uniform float uNormalStrength, uSplitX;
uniform vec3 uEye, uFogColour;
uniform float uFogDensity;
// Map-edge cutaway: room floor bounds (min x, min z, max x, max z). When the camera is beyond
// a boundary, that side's outer wall and its fixtures are not drawn, so they never hide the
// courier (who never comes within 0.3 of a wall). The floor stays.
uniform bool uCutaway;
uniform vec4 uRoom;
out vec4 fragColour;
void main() {
  if(uCutaway&&vPosition.y>1.001) {
    const float edge=.06;
    if((uEye.x<uRoom.x&&vPosition.x<uRoom.x+edge)||(uEye.x>uRoom.z&&vPosition.x>uRoom.z-edge)||
       (uEye.z<uRoom.y&&vPosition.z<uRoom.y+edge)||(uEye.z>uRoom.w&&vPosition.z>uRoom.w-edge)) discard;
  }
  vec3 n=normalize(vNormal);
  vec3 view=normalize(uEye-vPosition);
  vec3 light=normalize(vec3(-.65,.75,-.35));
  float fog=1.0-exp(-length(uEye-vPosition)*uFogDensity);
  // Emission above 1 is unlit glow: particles, beams and halos. A positive
  // material.z shades smoke and dust as volumes instead.
  if(vMaterial.y>1.0) {
    float facing=abs(dot(n,view));
    // material.x > 0 fades the edges (soft blobs); < 0 fades the centre (shells).
    float soft=vMaterial.x>=0.0?mix(1.0,facing*facing,vMaterial.x):mix(1.0,pow(1.0-facing,2.0),-vMaterial.x);
    vec3 glow=vColour*(vMaterial.y-1.0);
    if(vMaterial.z>0.0) glow=glow*mix(1.0,.5+.45*max(dot(n,light),0.0)+.2*n.y,vMaterial.z);
    fragColour=vec4(mix(glow,uFogColour,fog*.6),vAlpha*soft);
    return;
  }
  // Derive the UV basis from world-space derivatives. This retains UV handedness
  // on caps/mirrored charts and accounts for non-uniform model scaling.
  vec3 dpdx=dFdx(vPosition), dpdy=dFdy(vPosition);
  vec2 duvdx=dFdx(vUv), duvdy=dFdy(vUv);
  float det=duvdx.x*duvdy.y-duvdx.y*duvdy.x;
  vec3 t=normalize(vTangent-n*dot(n,vTangent));
  vec3 b=cross(n,t);
  if(abs(det)>1e-10) {
    vec3 du=(dpdx*duvdy.y-dpdy*duvdx.y)/det;
    vec3 dv=(dpdy*duvdx.x-dpdx*duvdy.x)/det;
    vec3 tangent=du-n*dot(n,du);
    if(dot(tangent,tangent)>1e-12) {
      t=normalize(tangent);
      b=cross(n,t)*(dot(cross(n,t),dv)<0.0?-1.0:1.0);
    }
  }
  int surface=int(clamp(vSurface,1.0,5.0))-1;
  // Terrain: the fraction of the surface index is a faint self-glow (guide trim, beacons).
  float selfGlow=fract(vSurface);
  vec3 materialUv=vec3(vUv,float(surface));
  // Ceramic, tread steel, brushed alloy, rubber, painted casing.
  float gloss=surface==0?80.0:surface==1?24.0:surface==2?110.0:surface==3?8.0:48.0;
  float reflectance=surface==0?.25:surface==1?.20:surface==2?.65:surface==3?.025:.18;
  float relief=surface==0?.7:surface==1?1.0:surface==2?.45:surface==3?.65:.45;
  bool mapped=uNormalCompare?gl_FragCoord.x>=uSplitX:uNormalMapping;
  if(mapped && uNormalStrength>0.0) {
    vec3 detail=texture(uNormal,materialUv).xyz*2.0-1.0;
    detail.xy*=relief;
    detail.xy *= uNormalStrength;
    n=normalize(mat3(t,b,n)*normalize(detail));
  }
  vec3 albedo=vColour*texture(uAlbedo,materialUv).rgb;
  float diffuse=max(dot(n,light),0.0);
  float specular=pow(max(dot(n,normalize(light+view)),0.0),gloss);
  float sky=.5+.5*n.y;
  vec3 ambient=mix(vec3(.16,.18,.23),vec3(.34,.39,.47),sky);
  // Required ambient + Lambert diffuse + Blinn-Phong specular, in world space.
  vec3 colour=albedo*(ambient+vec3(1.0,.90,.78)*.85*diffuse)
             +vec3(1,.94,.86)*specular*reflectance+albedo*(vMaterial.y+selfGlow);
  colour=mix(colour,uFogColour,fog);
  fragColour=vec4(colour,vAlpha);
}
