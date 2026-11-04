#version 330 core
in vec2 vNdc;
uniform mat4 uInverseViewProjection;
uniform samplerCube uSkybox;
out vec4 fragColour;
void main() {
    vec4 nearPoint = uInverseViewProjection * vec4(vNdc, -1.0, 1.0);
    vec4 farPoint = uInverseViewProjection * vec4(vNdc, 1.0, 1.0);
    // Subtract points rather than using the eye: works for perspective and orthographic views.
    vec3 direction = farPoint.xyz / farPoint.w - nearPoint.xyz / nearPoint.w;
    fragColour = vec4(texture(uSkybox, normalize(direction)).rgb, 1.0);
}
