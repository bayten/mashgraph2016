#version 330 core

in vec2 TexCoords;
in vec4 ourColors;

uniform sampler2D ourTexture;

out vec4 outColor;

void main() {
    outColor = ourColors*texture(ourTexture, TexCoords)*max(TexCoords.y, 0.3);
}
