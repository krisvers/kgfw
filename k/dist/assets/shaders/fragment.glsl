#version 330 core

in vec3 v_pos;
in vec3 v_color;
in vec2 v_uv;
uniform float unif_time;
uniform vec3 unif_view_pos;
uniform sampler2D unif_texture_color;
uniform sampler2D unif_texture_normal;
out vec4 out_color;

void main() {
	vec4 tex = texture(unif_texture_color, v_uv);
	if (tex.a <= 0.1) {
		discard;
	}

	out_color = vec4(tex * vec4(v_color, 1));
}
