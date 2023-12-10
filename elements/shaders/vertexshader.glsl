/*
The Vertex Shader of qDiv
Copyright (C) 2023  Gabriel F. Hodges

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
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
