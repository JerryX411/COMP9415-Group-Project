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
uniform float uFogDensity, uTime;
// Map-edge cutaway: room floor bounds (min x, min z, max x, max z). When the camera is beyond
// a boundary, that side's outer wall and its fixtures are not drawn, so they never hide the
// courier (who never comes within 0.3 of a wall). The floor stays.
uniform bool uCutaway;
uniform vec4 uRoom;
out vec4 fragColour;
float hash1(float n) { float h = sin(n * 12.9898) * 43758.5453; return h - floor(h); }
// Same flicker as relay::crimsonFlicker (RelayGame.h): about .75 most of the time.
float crimsonFlicker(float time, float seed) {
  float step=floor(time*14.0), slow=floor(time*3.0);
  float jitter=hash1(step*1.7+seed*9.1), dip=hash1(slow*3.3+seed*5.7);
  float k=.68+.14*jitter;
  if(dip>.82&&jitter>.35) k=.12+.1*jitter;
  else if(jitter>.93) k=1.0;
  return k;
}
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
  // Void floor (surface 9): the full edition's crimson tiles. A near-black red bed of cells, each
  // flickering dark red like a failing light on its own beat (a stepped jitter, brief dips toward
  // dark, the odd bright pop), shared with a slower glow over 6 x 6 blocks and a soft wave.
  if(vSurface>8.5) {
    vec2 cell=floor(vPosition.xz+.0001), f=fract(vPosition.xz);
    float seed=cell.x*7.0+cell.y*13.0;
    vec2 block=floor(vPosition.xz/6.0);
    float k=.6*crimsonFlicker(uTime,seed)+.4*crimsonFlicker(uTime,block.x*3.1+block.y*5.3);
    float wave=.5+.5*sin(uTime*2.6-(cell.x+cell.y)*.7);
    float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y));
    vec3 crimson=vec3(.62,.03,.08);
    vec3 lit=crimson*(.22+.32*k+.08*wave)*(.25+.9*(.08+.38*k+.05*wave));
    vec3 bed=vec3(.11,.01,.02)*.7;
    vec3 colour=mix(bed,lit,smoothstep(.035,.06,edge));
    fragColour=vec4(mix(colour,uFogColour,fog*.7),vAlpha);
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
  // Jump-pad surface: two circles and a centre dot, printed on the top face.
  // UV derivatives antialias the marks at any camera distance; they retain the
  // painted surface's normal detail and scene lighting rather than washing out.
  float launchMark=0.0;
  if(vSurface>5.95&&vSurface<6.05&&vNormal.y>.5) {
    float radius=length(vUv-vec2(.5)),aa=max(fwidth(radius),.001);
    float outer=1.0-smoothstep(.018-aa,.018+aa,abs(radius-.37));
    float inner=1.0-smoothstep(.016-aa,.016+aa,abs(radius-.20));
    float centre=1.0-smoothstep(.047-aa,.047+aa,radius);
    launchMark=max(max(outer,inner),centre);
    albedo=mix(albedo,vec3(.34,.92,.80),launchMark);
  }
  float diffuse=max(dot(n,light),0.0);
  float specular=pow(max(dot(n,normalize(light+view)),0.0),gloss);
  float sky=.5+.5*n.y;
  vec3 ambient=mix(vec3(.16,.18,.23),vec3(.34,.39,.47),sky);
  // Required ambient + Lambert diffuse + Blinn-Phong specular, in world space.
  vec3 colour=albedo*(ambient+vec3(1.0,.90,.78)*.85*diffuse)
             +vec3(1,.94,.86)*specular*reflectance+albedo*(vMaterial.y+selfGlow);
  colour+=launchMark*vec3(.20,.72,.62)*(.18+vMaterial.y*.35);
  colour=mix(colour,uFogColour,fog);
  fragColour=vec4(colour,vAlpha);
}
