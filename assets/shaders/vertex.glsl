#version 330 core

in vec3 in_pos;
in vec3 in_color;
in vec3 in_normal;
in vec2 in_uv;
uniform mat4 unif_mvp;
out vec3 v_pos;
out vec3 v_color;
out vec3 v_normal;
out vec2 v_uv;

void main() {
	gl_Position = unif_mvp * vec4(in_pos, 1.0);
	v_pos = in_pos;
	v_color = in_color;
	v_normal = in_normal;
	v_uv = in_uv;
}
