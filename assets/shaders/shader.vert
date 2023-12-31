/*layout (location = 0) in*/ attribute vec3 in_pos;
/*layout (location = 1) in*/ attribute vec3 in_color;
/*layout (location = 2) in*/ attribute vec3 in_normal;
/*layout (location = 3) in*/ attribute vec2 in_uv;
uniform mat4 unif_m;
uniform mat4 unif_vp;
uniform mat4 unif_m_r;

varying vec3 v_pos;
varying vec3 v_color;
varying vec3 v_normal;
varying vec2 v_uv;

void main() {
	gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0);
	v_pos = vec3(unif_m * vec4(in_pos, 1.0));
	v_color = in_color;
	v_normal = normalize(vec3(unif_m * vec4(in_normal, 0.0)));
	v_uv = in_uv;
}
