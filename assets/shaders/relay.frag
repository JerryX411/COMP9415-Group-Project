#version 330 core
in vec3 vPosition, vNormal, vTangent, vColour;
in vec2 vUv;
flat in vec4 vMaterial;
uniform sampler2D uAlbedo, uNormal;
uniform bool uNormalMapping;
uniform vec3 uEye;
out vec4 fragColour;
void main() {
  vec3 n=normalize(vNormal);
  vec3 t=normalize(vTangent-n*dot(n,vTangent));
  if(uNormalMapping) {
    vec3 detail=texture(uNormal,vUv).xyz*2.0-1.0;
    detail.xy*=mix(1.0,.18,vMaterial.x);
    n=normalize(mat3(t,cross(n,t),n)*normalize(detail));
  }
  vec3 light=normalize(vec3(-.45,1.0,.35));
  vec3 view=normalize(uEye-vPosition);
  vec3 albedo=vColour*mix(texture(uAlbedo,vUv).rgb,vec3(.98),vMaterial.x*.8);
  float diffuse=max(dot(n,light),0.0);
  float specular=pow(max(dot(n,normalize(light+view)),0.0),mix(12.0,56.0,vMaterial.x));
  float sky=.5+.5*n.y;
  vec3 ambient=mix(vec3(.20,.22,.24),vec3(.48,.52,.56),sky);
  float rim=pow(1.0-max(dot(n,view),0.0),3.0)*.13*vMaterial.x;
  float fill=max(dot(n,normalize(vec3(.6,.4,-.8))),0.0)*.24;
  vec3 colour=albedo*(ambient+.66*diffuse+fill)+vec3(.85,.93,1.0)*(specular*vMaterial.z+rim)+albedo*vMaterial.y;
  float fog=1.0-exp(-length(uEye-vPosition)*.006);
  colour=mix(colour,vec3(.28,.34,.38),fog);
  fragColour=vec4(colour,1.0);
}
