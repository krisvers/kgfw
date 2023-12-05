#version 330 core

in vec3 v_normal;

out vec4 out_color;

void main() {
	out_color = vec4((v_normal + 1) / 2, 1);
}