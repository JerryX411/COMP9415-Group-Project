#version 330 core

// The rasterizer stores this output color in the framebuffer.
out vec4 fragColor;

in vec3 vertexColor;
uniform vec3 uTint;

void main() {
    fragColor = vec4(vertexColor * uTint, 1.0);
}
