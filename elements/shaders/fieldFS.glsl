#version 330 core
out vec4 FragColor;
in vec2 positionFS;
in vec3 lightFS;
uniform sampler2D sampler;
void main() {
	vec4 check = texture(sampler, positionFS) * vec4(lightFS, 1.f) * 1.f;
	if(check.a == 0.0) discard;
	FragColor = check;
}
