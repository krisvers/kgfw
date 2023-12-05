#version 330 core

in vec3 v_pos;
in vec3 v_color;
in vec3 v_normal;
in vec2 v_uv;
uniform float unif_time;
uniform vec3 unif_view_pos;
uniform float unif_textured_color;
uniform float unif_textured_normal;
uniform sampler2D unif_texture_color;
uniform sampler2D unif_texture_normal;
out vec4 out_color;

void main() {
	vec4 tex = texture(unif_texture_color, v_uv);
	vec4 col = mix(vec4((v_normal + 1) / 2, 1), tex, unif_textured_color);
	if (col.a == 0) {
		discard;
	}

	vec3 light_pos = vec3(0, 100, 0);
	float light_power = 2000;
	vec3 dir = light_pos - v_pos;
	float dist = length(dir);
	dir = normalize(dir);
	vec3 light_color = vec3(1, 1, 1);
	vec3 ambient_color = vec3(1, 1, 1);

	float ambience = 0.6;
	float diffusion = 1;
	float shiny = 2;

	float lambertian = max(dot(dir, v_normal), 0) * diffusion;
	
	float specular = 0;
	if (lambertian > 0) {
		vec3 view_dir = normalize(-v_pos);
		vec3 half_dir = normalize(dir + view_dir);
		float spec = max(dot(half_dir, v_normal), 0);
		specular = pow(spec, shiny);
	}

	vec3 color = col.xyz * ((ambient_color * ambience) + (light_color * lambertian * light_power / (dist * dist)) + (light_color * specular * light_power / (dist * dist)));
	out_color = vec4(color, 1);/*vec4((v_normal + 1) / 2, 1);*//*vec4(v_pos, 1);*//*vec4(v_uv, 0, 1);*/
}
