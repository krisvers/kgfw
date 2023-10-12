#version 330 core

in vec3 v_pos;
in vec3 v_color;
in vec3 v_normal;
in vec2 v_uv;
out vec4 out_color;

void main() {
	vec3 normal = normalize(v_normal);
	vec3 light_pos = vec3(0, 0, 0);
	vec3 light_dir = normalize(light_pos - v_pos);
	vec3 light_color = vec3(1, 1, 1);

	vec3 ambient_color = light_color;
	float ambience = 0.25;
	
	vec3 diffuse_color = light_color;
	float diffusion = max(dot(normal, light_dir), 0);

	vec3 color = (ambience * ambient_color) + (diffuse_color * diffusion);
	out_color = vec4(v_color * color, 1);
}
