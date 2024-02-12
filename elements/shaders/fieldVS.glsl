#version 330 core
layout (location = 0) in vec2 positionVS;
layout (location = 1) in vec2 textureVS;
layout (location = 2) in vec3 lightVS;
out vec2 positionFS;
out vec3 lightFS;
uniform mat3 matrix;
void main() {
	gl_Position = vec4(matrix * vec3(positionVS, 1.0), 1.0);
	positionFS = vec2(textureVS.x, -textureVS.y);
	lightFS = lightVS;
}
