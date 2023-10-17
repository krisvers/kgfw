#version 330 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec2 in_uv;
layout (location = 4) in vec3 in_tangent;
layout (location = 5) in vec3 in_bitangent;
uniform mat4 unif_m;
uniform mat4 unif_vp;
out vec3 v_pos;
out vec3 v_color;
out vec2 v_uv;
out mat3 v_tbn;

void main() {
	gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0);
	v_pos = vec3(unif_m * vec4(in_pos, 1.0));
	v_color = in_color;
	v_uv = in_uv;
	v_tbn[0] = in_normal;
	v_tbn[1] = in_tangent;
	v_tbn[2] = in_bitangent;
}
