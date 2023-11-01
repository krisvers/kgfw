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
	vec3 normal = normalize(vec3(texture(unif_texture_normal, v_uv)));
	vec3 light_dir = normalize(unif_light_pos - v_pos);
	vec3 view_dir = normalize(unif_view_pos - v_pos);
	float dist = distance(unif_view_pos, v_pos);

	vec3 ambient_color = unif_light_color;
	
	vec3 diffuse_color = unif_light_color;
	float diffusion = unif_diffusion * max(dot(normal, light_dir), 0);

	vec3 H = normalize(light_dir + view_dir);
	vec3 specular_color = unif_light_color;
	float speculation = unif_speculation * pow(max(dot(normal, H), 0.0), unif_metalic);

	vec4 tex_color;
	if (unif_texture_weight == 0) {
		tex_color = vec4(1, 1, 1, 1);
	} else {
		tex_color = texture(unif_texture_color, v_uv);
		if (tex_color.a < 0.5) {
			discard;
		}
	}
	
	vec3 color = (ambient_color * unif_ambience) + (diffuse_color * diffusion) + (specular_color * speculation);
	out_color = vec4(tex_color * vec4(v_color, 1));
}
