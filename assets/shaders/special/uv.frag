#version 330 core

in vec2 v_uv;

out vec4 out_color;

void main() {
	out_color = vec4(v_uv, 0, 1);
}