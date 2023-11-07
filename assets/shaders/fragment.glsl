#version 330 core

in vec3 v_pos;
in vec3 v_color;
in vec2 v_uv;
uniform float unif_time;
uniform vec3 unif_view_pos;
uniform vec3 unif_light_pos;
uniform vec3 unif_light_color;
uniform float unif_ambience;
uniform float unif_diffusion;
uniform float unif_speculation;
uniform float unif_metalic;
uniform float unif_texture_weight;
uniform float unif_normal_weight;
uniform sampler2D unif_texture_color;
uniform sampler2D unif_texture_normal;
out vec4 out_color;

void main() {
	vec4 tex_color;
	if (unif_texture_weight == 0) {
		tex_color = vec4(1, 1, 1, 1);
	} else {
		tex_color = texture(unif_texture_color, v_uv);
		if (tex_color.a < 0.5) {
			discard;
		}
	}
	
	out_color = vec4(tex_color * vec4(v_color, 1));
}
