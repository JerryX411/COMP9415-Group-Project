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
    vec3 ray = normalize(direction);
    vec3 sky = texture(uSkybox, ray).rgb;
    // Gentle blue lift preserves the supplied art without amplifying JPEG noise.
    sky = pow(max(sky, vec3(0.0)), vec3(0.88)) * vec3(0.88, 0.98, 1.10);
    float horizon = 1.0 - smoothstep(0.0, 0.55, abs(ray.y));
    sky = mix(sky, vec3(0.16, 0.22, 0.29), 0.12 + 0.22 * horizon);
    fragColour = vec4(sky, 1.0);
}
