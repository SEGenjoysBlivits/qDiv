#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 tex;
layout (location = 2) in float lgt;
out vec2 fragPos;
out float light;
uniform mat3 matrix;
void main() {
	gl_Position = vec4(matrix * vec3(pos, 1.0), 1.0);
	fragPos = vec2(tex.x, -tex.y);
	light = lgt;
}
