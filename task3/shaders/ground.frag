#version 330

in vec2 TexCoord;

out vec4 outColor;

uniform sampler2D ourTexture;

void main() {
    outColor = texture(ourTexture, TexCoord);
}
