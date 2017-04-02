#version 330 core

in vec4 point;
in vec2 texcoords;
in vec2 position;
in vec4 variance;
in vec2 rotation;
in vec2 scale;
in vec4 colors;

uniform mat4 camera;

out vec2 TexCoords;
out vec4 ourColors;

void main() {
	TexCoords = texcoords;
	ourColors = colors;

    mat4 scaleMatrix = mat4(1.0);
    scaleMatrix[0][0] = 0.01 * scale.x;
    scaleMatrix[1][1] = 0.1 * scale.y;

    mat4 positionMatrix = mat4(1.0);
    positionMatrix[3][0] = position.x;
    positionMatrix[3][2] = position.y;
	
	mat4 rotationMatrix = mat4(1.0);
	rotationMatrix[0][0] = rotation.y;
	rotationMatrix[0][2] = rotation.x;
	rotationMatrix[2][0] = -rotation.x;
	rotationMatrix[2][2] = rotation.y;
	
	gl_Position = camera * (positionMatrix*rotationMatrix * scaleMatrix * point + variance * point.y * point.y);
}
