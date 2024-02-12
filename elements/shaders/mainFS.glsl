#version 330 core
out vec4 FragColor;
in vec2 fragPos;
uniform vec4 color;
uniform sampler2D sampler;
void main() {
	vec4 check = texture(sampler, fragPos) * color * 1.f;
	if(check.a == 0.0) discard;
	FragColor = check;
}
