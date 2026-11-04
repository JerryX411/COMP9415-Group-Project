#version 330 core
out vec2 vNdc;
void main() {
    // One oversized triangle covers the viewport without a diagonal seam.
    vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vNdc = positions[gl_VertexID];
    gl_Position = vec4(vNdc, 1.0, 1.0);
}
