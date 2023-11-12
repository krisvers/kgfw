#version 450

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;
layout (location = 3) in vec3 in_normal;
layout (location = 2) in vec3 in_uv;

layout (location = 0) out vec3 v_color;

layout (binding = 0) uniform ubo_t {
	mat4 mvp;
} ubo;

void main() {
	gl_Position = ubo.mvp * vec4(in_pos, 1.0);
	v_color = in_color;
	if (gl_Position.z < 0) {
		gl_Position.z = 0;
	}
}