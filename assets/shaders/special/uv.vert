#version 330 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec2 in_uv;

out vec2 v_uv;

uniform mat4 unif_m;
uniform mat4 unif_vp;

void main() {
	gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0);
	v_uv = in_uv;
}