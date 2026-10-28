#version 330 core

in vec2 uv;

uniform sampler2D uFont;
uniform vec4 uColour;
// 1.0 masks the colour with the glyph's alpha; 0.0 draws a solid rectangle.
uniform float uUseTexture;

out vec4 fragColor;

void main() {
    float coverage = mix(1.0, texture(uFont, uv).a, uUseTexture);
    fragColor = vec4(uColour.rgb, uColour.a * coverage);
}
