#version 330 core

in vec3 in_pos;
in vec3 in_color;
uniform mat4 unif_mvp;
out vec3 v_pos;
out vec3 v_color;

void main() {
	gl_Position = unif_mvp * vec4(in_pos, 1.0);
	v_pos = gl_Position.xyz;
	v_color = in_color;
}
