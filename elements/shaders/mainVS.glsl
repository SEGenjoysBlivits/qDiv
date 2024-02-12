#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 tex;
out vec2 fragPos;
uniform mat3 matrix;
void main() {
	gl_Position = vec4(matrix * vec3(pos, 1.0), 1.0);
	fragPos = vec2(tex.x, -tex.y);
}
