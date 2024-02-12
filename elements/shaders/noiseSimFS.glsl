#version 330 core
out vec4 FragColor;
in vec2 fragPos;
in float light;
uniform vec4 color;
uniform sampler2D sampler;
void main() {
	vec4 check = texture(sampler, fragPos) * light * color;
	if(check.a == 0.0) discard;
	FragColor = check;
}
