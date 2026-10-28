#version 330 core

// Fraction of the image visible along each axis (1.0 = all of it), used to
// fill the window without stretching the image.
uniform vec2 uUvScale;

out vec2 uv;

// One oversized triangle covers the screen without needing a vertex buffer.
void main() {
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    // Image rows start at the top, so flip v; then crop around the centre.
    uv = 0.5 + (vec2(corner.x, 1.0 - corner.y) - 0.5) * uUvScale;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
