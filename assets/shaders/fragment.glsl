#version 330 core

in vec3 v_pos;
in vec3 v_color;
in vec3 v_normal;
in vec2 v_uv;
out vec4 out_color;

void main() {
	out_color = vec4(v_color, 1);
}