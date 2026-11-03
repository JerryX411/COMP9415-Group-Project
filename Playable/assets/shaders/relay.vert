#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aColour;
layout(location=5) in mat4 iModel;
layout(location=9) in vec4 iColour;
layout(location=10) in vec4 iMaterial;
uniform mat4 uViewProjection;
out vec3 vPosition, vNormal, vTangent, vColour;
out vec2 vUv;
out float vAlpha;
flat out vec4 vMaterial;
void main() {
  vec4 world=iModel*vec4(aPosition,1.0);
  vPosition=world.xyz;
  vNormal=normalize(transpose(inverse(mat3(iModel)))*aNormal);
  vTangent=normalize(mat3(iModel)*aTangent);
  vColour=aColour*iColour.rgb;
  vAlpha=iColour.a;
  vUv=aUv*iMaterial.w;
  vMaterial=iMaterial;
  gl_Position=uViewProjection*world;
}
