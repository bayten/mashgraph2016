#version 330

in vec4 point;
in vec2 texcoords;

uniform mat4 camera;

out vec2 TexCoord;

void main() {
    TexCoord = texcoords;
    gl_Position = camera * point;
}
