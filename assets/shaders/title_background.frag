#version 330 core

in vec2 uv;

uniform sampler2D uImage;

out vec4 fragColor;

void main() {
    fragColor = vec4(texture(uImage, uv).rgb, 1.0);
}
