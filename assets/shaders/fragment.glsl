#version 330 core

in vec3 v_pos;
in vec3 v_color;
out vec4 out_color;

void main() {
	out_color = vec4(v_color, 1);
}
