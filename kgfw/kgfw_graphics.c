#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
#include "kgfw_time.h"
#include "kgfw_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linmath.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef KGFW_APPLE
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_INDICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_SHADOW_MAP_WIDTH 2048
#define KGFW_GRAPHICS_DEFAULT_SHADOW_MAP_HEIGHT 2048

#ifdef KGFW_DEBUG
#define GL_CHECK_ERROR() { GLenum err = glGetError(); if (err != GL_NO_ERROR) { kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "(%s:%u) OpenGL Error (%u 0x%X) %s", __FILE__, __LINE__, err, err, (err == 0x500) ? "INVALID ENUM" : (err == 0x501) ? "INVALID VALUE" : (err == 0x502) ? "INVALID OPERATION" : (err == 0x503) ? "STACK OVERFLOW" : (err == 0x504) ? "STACK UNDERFLOW" : (err == 0x505) ? "OUT OF MEMORY" : (err == 0x506) ? "INVALID FRAMEBUFFER OPERATION" : "UNKNOWN"); } }
#define GL_CALL(statement) statement; GL_CHECK_ERROR()
#else
#define GL_CHECK_ERROR()
#define GL_CALL(statement) statement;
#endif

typedef struct mesh_node {
	struct {
		float pos[3];
		float rot[3];
		float scale[3];
		unsigned char absolute;
	} transform;

	struct mesh_node * parent;
	struct mesh_node * child;
	struct mesh_node * sibling;
	struct mesh_node * prior_sibling;

	struct {
		GLuint vao;
		GLuint vbo;
		GLuint ibo;
		GLuint program;
		GLuint tex;
		GLuint normal;

		unsigned long long int vbo_size;
		unsigned long long int ibo_size;
	} gl;
} mesh_node_t;

struct {
	kgfw_window_t * window;
	kgfw_camera_t * camera;
	GLuint vshader;
	GLuint fshader;
	GLuint program;
	mat4x4 vp;

	mesh_node_t * mesh_root;

	unsigned int settings;

	struct {
		vec3 pos;
		vec3 color;
		float ambience;
		float diffusion;
		float speculation;
		float metalic;
	} light;
} static state = {
	NULL, NULL,
	0, 0, 0,
	{ 0 },
	NULL,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
	{
		{ 0, 100, 0 },
		{ 1, 1, 1 },
		0.0f, 0.5f, 0.25f, 8
	}
};

struct {
	mat4x4 model;
	mat4x4 model_r;
	vec3 pos;
	vec3 rot;
	vec3 scale;
} recurse_state = {
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	},
};

static void update_settings(unsigned int change);
static void register_commands(void);

static void meshes_draw_recursive(mesh_node_t * mesh);
static void meshes_draw_recursive_fchild(mesh_node_t * mesh);
static void meshes_free(mesh_node_t * node);
static mesh_node_t * meshes_new(void);
static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m);
static void meshes_free_recursive_fchild(mesh_node_t * mesh);
static void meshes_free_recursive(mesh_node_t * mesh);
static void gl_errors(void);

static int shaders_load(const char * vpath, const char * fpath, GLuint * out_program);

void kgfw_graphics_settings_set(kgfw_graphics_settings_action_enum action, unsigned int settings) {
	unsigned int change = 0;

	switch (action) {
		case KGFW_GRAPHICS_SETTINGS_ACTION_SET:
			change = state.settings & ~settings;
			state.settings = settings;
			break;
		case KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE:
			state.settings |= settings;
			change = settings;
			break;
		case KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE:
			state.settings &= ~settings;
			change = settings;
			break;
		default:
			return;
	}

	update_settings(change);
}

unsigned int kgfw_graphics_settings_get(void) {
	return state.settings;
}

int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera) {
	register_commands();

	state.window = window;
	state.camera = camera;

	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
			glfwSwapInterval(0);
		}
	}

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "Failed to load OpenGL 3.3 context");
		return 1;
	}

	if (window != NULL) {
		if (window->internal != NULL) {
			GL_CALL(glViewport(0, 0, window->width, window->height));
		}
	}

	state.program = GL_CALL(glCreateProgram());
	int r = shaders_load("assets/shaders/vertex.glsl", "assets/shaders/fragment.glsl", &state.program);
	if (r != 0) {
		return r;
	}

	GL_CALL(glEnable(GL_DEPTH_TEST));
	//GL_CALL(glEnable(GL_FRAMEBUFFER_SRGB));
	//GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
	GL_CALL(glEnable(GL_BLEND));
	GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

	update_settings(state.settings);

	return 0;
}

void kgfw_graphics_draw(void) {
	GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GL_CALL(glClearColor(0.57f, 0.59f, 0.58f, 1.0f));

	mat4x4 mvp;
	mat4x4 m;
	mat4x4 v;
	mat4x4 p;

	mat4x4_identity(mvp);
	mat4x4_identity(state.vp);
	mat4x4_identity(m);
	mat4x4_identity(v);
	mat4x4_identity(p);

	kgfw_camera_view(state.camera, v);
	kgfw_camera_perspective(state.camera, p);

	mat4x4_mul(state.vp, p, v);

	state.light.pos[0] = sinf(kgfw_time_get() * 2) * 10;
	state.light.pos[1] = cosf(kgfw_time_get() / 6) * 15;
	state.light.pos[2] = sinf(kgfw_time_get() + 3) * 10;
	if (state.mesh_root != NULL) {
		mat4x4_identity(recurse_state.model);

		recurse_state.pos[0] = 0;
		recurse_state.pos[1] = 0;
		recurse_state.pos[2] = 0;
		recurse_state.rot[0] = 0;
		recurse_state.rot[1] = 0;
		recurse_state.rot[2] = 0;
		recurse_state.scale[0] = 1;
		recurse_state.scale[1] = 1;
		recurse_state.scale[2] = 1;

		meshes_draw_recursive_fchild(state.mesh_root);
	}
}

void kgfw_graphics_mesh_texture(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_t * texture, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	GLenum fmt = (texture->fmt == KGFW_GRAPHICS_TEXTURE_FORMAT_RGBA) ? GL_RGBA : GL_BGRA;
	GLenum filtering = (texture->filtering == KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST) ? GL_NEAREST : GL_LINEAR;
	GLenum filtering_mipmap = (texture->filtering == KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
	GLenum u_wrap = (texture->u_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? GL_CLAMP_TO_BORDER : GL_REPEAT;
	GLenum v_wrap = (texture->v_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? GL_CLAMP_TO_BORDER : GL_REPEAT;
	GLuint * t = NULL;
	if (use == KGFW_GRAPHICS_TEXTURE_USE_COLOR) {
		t = &m->gl.tex;
	} else if (use == KGFW_GRAPHICS_TEXTURE_USE_NORMAL) {
		t = &m->gl.normal;
	}

	if (*t == 0) {
		GL_CALL(glGenTextures(1, t));
	}
	
	GL_CALL(glBindTexture(GL_TEXTURE_2D, *t));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, u_wrap));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, v_wrap));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering_mipmap));
	GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture->bitmap));
	GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
}

void kgfw_graphics_mesh_texture_detach(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	GLuint * t = NULL;
	if (use == KGFW_GRAPHICS_TEXTURE_USE_COLOR) {
		t = &m->gl.tex;
	} else if (use == KGFW_GRAPHICS_TEXTURE_USE_NORMAL) {
		t = &m->gl.normal;
	}

	if (*t != 0) {
		GL_CALL(glDeleteTextures(1, t));
		*t = 0;
	}
}

kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent) {
	mesh_node_t * node = meshes_new();
	node->parent = (mesh_node_t *) parent;
	memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
	memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
	memcpy(node->transform.scale, mesh->scale, sizeof(vec3));
	node->gl.vbo_size = mesh->vertices_count;
	node->gl.ibo_size = mesh->indices_count;
	GL_CALL(glBindVertexArray(node->gl.vao));
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, node->gl.vbo));
	GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * mesh->vertices_count, mesh->vertices, GL_STATIC_DRAW));
	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, node->gl.ibo));
	GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->indices_count, mesh->indices, GL_STATIC_DRAW));

	GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, x)));
	GL_CALL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, r)));
	GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, nx)));
	GL_CALL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, u)));
	GL_CALL(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, tx)));
	GL_CALL(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, btx)));
	GL_CALL(glEnableVertexAttribArray(0));
	GL_CALL(glEnableVertexAttribArray(1));
	GL_CALL(glEnableVertexAttribArray(2));
	GL_CALL(glEnableVertexAttribArray(3));
	GL_CALL(glEnableVertexAttribArray(4));
	GL_CALL(glEnableVertexAttribArray(5));

	if (parent == NULL) {
		if (state.mesh_root == NULL) {
			state.mesh_root = node;
		} else {
			mesh_node_t * n;
			for (n = state.mesh_root; n->sibling != NULL; n = n->sibling);
			n->sibling = node;
			node->prior_sibling = n;
		}
		return (kgfw_graphics_mesh_node_t *) node;
	}

	if (parent->child == NULL) {
		parent->child = (kgfw_graphics_mesh_node_t *) node;
	} else {
		mesh_node_t * n;
		for (n = (mesh_node_t *) parent->child; n->sibling != NULL; n = n->sibling);
		n->sibling = node;
		node->prior_sibling = n;
	}
	
	return (kgfw_graphics_mesh_node_t *) node;
}

void kgfw_graphics_mesh_destroy(kgfw_graphics_mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->parent != NULL) {
		mesh->parent->child = mesh->child;
	}
	if (mesh->prior_sibling != NULL) {
		mesh->prior_sibling->sibling = mesh->sibling;
	}

	if (state.mesh_root == (mesh_node_t *) mesh) {
		state.mesh_root = NULL;
	}

	meshes_free((mesh_node_t *) mesh);
}

void kgfw_graphics_set_window(kgfw_window_t * window) {
	state.window = window;
	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
		}
		GL_CALL(glViewport(0, 0, window->width, window->height));
	}
}

void kgfw_graphics_viewport(unsigned int width, unsigned int height) {
	GL_CALL(glViewport(0, 0, width, height));
}

kgfw_window_t * kgfw_graphics_get_window(void) {
	return state.window;
}

void kgfw_graphics_deinit(void) {
	meshes_free_recursive_fchild(state.mesh_root);
}

static mesh_node_t * meshes_alloc(void) {
	mesh_node_t * m = malloc(sizeof(mesh_node_t));
	if (m == NULL) {
		return NULL;
	}

	memset(m, 0, sizeof(*m));
	m->transform.scale[0] = 1;
	m->transform.scale[1] = 1;
	m->transform.scale[2] = 1;
	return m;
}

static void meshes_free(mesh_node_t * node) {
	if (node == NULL) {
		return;
	}

	if (node->gl.vbo != 0) {
		GL_CALL(glDeleteBuffers(1, &node->gl.vbo));
	}
	if (node->gl.ibo != 0) {
		GL_CALL(glDeleteBuffers(1, &node->gl.ibo));
	}
	if (node->gl.program != 0) {
		GL_CALL(glDeleteProgram(node->gl.program));
	}
	if (node->gl.vao != 0) {
		GL_CALL(glDeleteVertexArrays(1, &node->gl.vao));
	}
	if (node->gl.tex != 0) {
		GL_CALL(glDeleteTextures(1, &node->gl.tex));
	}
	if (node->gl.normal != 0) {
		GL_CALL(glDeleteTextures(1, &node->gl.normal));
	}

	free(node);
}

static void meshes_gen(mesh_node_t * node) {
	GL_CALL(glGenVertexArrays(1, &node->gl.vao));
	GL_CALL(glGenBuffers(1, &node->gl.vbo));
	GL_CALL(glGenBuffers(1, &node->gl.ibo));
	node->gl.tex = 0;
	node->gl.normal = 0;
}

static mesh_node_t * meshes_new(void) {
	mesh_node_t * m = meshes_alloc();	
	if (m == NULL) {
		return NULL;
	}

	meshes_gen(m);
	return m;
}

static void mesh_transform(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh->transform.absolute) {
		mat4x4_identity(out_m);
		mat4x4_translate(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[0] + mesh->transform.pos[2]);
		recurse_state.pos[0] = mesh->transform.pos[0];
		recurse_state.pos[1] = mesh->transform.pos[1];
		recurse_state.pos[2] = mesh->transform.pos[2];
		recurse_state.rot[0] = mesh->transform.rot[0];
		recurse_state.rot[1] = mesh->transform.rot[1];
		recurse_state.rot[2] = mesh->transform.rot[2];
		recurse_state.scale[0] = mesh->transform.scale[0];
		recurse_state.scale[1] = mesh->transform.scale[1];
		recurse_state.scale[2] = mesh->transform.scale[2];
	} else {
		mat4x4_translate_in_place(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[2] + mesh->transform.pos[2]);
		recurse_state.pos[0] += mesh->transform.pos[0];
		recurse_state.pos[1] += mesh->transform.pos[1];
		recurse_state.pos[2] += mesh->transform.pos[2];
		recurse_state.rot[0] += mesh->transform.rot[0];
		recurse_state.rot[1] += mesh->transform.rot[1];
		recurse_state.rot[2] += mesh->transform.rot[2];
		recurse_state.scale[0] *= mesh->transform.scale[0];
		recurse_state.scale[1] *= mesh->transform.scale[1];
		recurse_state.scale[2] *= mesh->transform.scale[2];
	}
	mat4x4_identity(recurse_state.model_r);
	mat4x4_rotate_X(out_m, out_m, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(out_m, out_m, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(out_m, out_m, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_rotate_X(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_scale_aniso(out_m, out_m, recurse_state.scale[0], recurse_state.scale[1], recurse_state.scale[2]);
}

static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh == NULL || out_m == NULL) {
		return;
	}
	if (mesh->gl.vbo_size == 0 || mesh->gl.ibo_size == 0 || mesh->gl.vbo == 0 || mesh->gl.ibo == 0) {
		return;
	}

	GLuint program = (mesh->gl.program == 0) ? state.program : mesh->gl.program;
	GL_CALL(glUseProgram(program));
	GLint uniform_model = GL_CALL(glGetUniformLocation(state.program, "unif_m"));
	GLint uniform_model_r = GL_CALL(glGetUniformLocation(state.program, "unif_m_r"));
	GLint uniform_vp = GL_CALL(glGetUniformLocation(state.program, "unif_vp"));
	GLint uniform_time = GL_CALL(glGetUniformLocation(state.program, "unif_time"));
	GLint uniform_view = GL_CALL(glGetUniformLocation(state.program, "unif_view_pos"));
	GLint uniform_light = GL_CALL(glGetUniformLocation(state.program, "unif_light_pos"));
	GLint uniform_light_color = GL_CALL(glGetUniformLocation(state.program, "unif_light_color"));
	GLint uniform_ambience = GL_CALL(glGetUniformLocation(state.program, "unif_ambience"));
	GLint uniform_diffusion = GL_CALL(glGetUniformLocation(state.program, "unif_diffusion"));
	GLint uniform_speculation = GL_CALL(glGetUniformLocation(state.program, "unif_speculation"));
	GLint uniform_metalic = GL_CALL(glGetUniformLocation(state.program, "unif_metalic"));
	GLint uniform_texture_weight = GL_CALL(glGetUniformLocation(state.program, "unif_texture_weight"));
	GLint uniform_texture_color = GL_CALL(glGetUniformLocation(state.program, "unif_texture_color"));
	GLint uniform_normal_weight = GL_CALL(glGetUniformLocation(state.program, "unif_normal_weight"));
	GLint uniform_texture_normal = GL_CALL(glGetUniformLocation(state.program, "unif_texture_normal"));

	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);

	GL_CALL(glUniformMatrix4fv(uniform_model, 1, GL_FALSE, &out_m[0][0]));
	GL_CALL(glUniformMatrix4fv(uniform_model_r, 1, GL_FALSE, &recurse_state.model_r[0][0]));
	GL_CALL(glUniformMatrix4fv(uniform_vp, 1, GL_FALSE, &state.vp[0][0]));
	GL_CALL(glUniform1f(uniform_time, kgfw_time_get()));
	GL_CALL(glUniform3f(uniform_view, state.camera->pos[0], state.camera->pos[1], state.camera->pos[2]));
	GL_CALL(glUniform3f(uniform_light, state.light.pos[0], state.light.pos[1], state.light.pos[2]));
	GL_CALL(glUniform3f(uniform_light_color, state.light.color[0], state.light.color[1], state.light.color[2]));
	GL_CALL(glUniform1f(uniform_ambience, state.light.ambience));
	GL_CALL(glUniform1f(uniform_diffusion, state.light.diffusion));
	GL_CALL(glUniform1f(uniform_speculation, state.light.speculation));
	GL_CALL(glUniform1f(uniform_metalic, state.light.metalic));

	GL_CALL(glUniform1i(uniform_texture_color, 0));
	if (mesh->gl.tex == 0) {
		GL_CALL(glActiveTexture(GL_TEXTURE0));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
		GL_CALL(glUniform1f(uniform_normal_weight, 0));
	}
	else {
		GL_CALL(glActiveTexture(GL_TEXTURE0));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, mesh->gl.tex));
		GL_CALL(glUniform1f(uniform_texture_weight, 1));
	}

	GL_CALL(glUniform1i(uniform_texture_normal, 1));
	if (mesh->gl.normal == 0) {
		GL_CALL(glActiveTexture(GL_TEXTURE1));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
		GL_CALL(glUniform1f(uniform_normal_weight, 0));
	}
	else {
		GL_CALL(glActiveTexture(GL_TEXTURE1));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, mesh->gl.normal));
		GL_CALL(glUniform1f(uniform_normal_weight, 1));
	}

	GL_CALL(glBindVertexArray(mesh->gl.vao));
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mesh->gl.vbo));
	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl.ibo));
	GL_CALL(glDrawElements(GL_TRIANGLES, mesh->gl.ibo_size, GL_UNSIGNED_INT, 0));
}

static void meshes_draw_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_draw(mesh, recurse_state.model);
	if (mesh->child == NULL) {
		return;
	}

	meshes_draw_recursive_fchild(mesh->child);
}

static void meshes_draw_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	vec3 pos;
	vec3 rot;
	vec3 scale;
	memcpy(pos, recurse_state.pos, sizeof(pos));
	memcpy(rot, recurse_state.rot, sizeof(rot));
	memcpy(scale, recurse_state.scale, sizeof(scale));
	
	meshes_draw_recursive(mesh);
	for (mesh_node_t * m = mesh;;) {
		m = m->sibling;
		if (m == NULL) {
			break;
		}

		memcpy(recurse_state.pos, pos, sizeof(pos));
		memcpy(recurse_state.rot, rot, sizeof(rot));
		memcpy(recurse_state.scale, scale, sizeof(scale));
		meshes_draw_recursive(m);
	}
}

static void meshes_free_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->child == NULL) {
		return;
	}

	meshes_free_recursive_fchild(mesh->child);
	meshes_free(mesh);
}

static void meshes_free_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	meshes_free_recursive(mesh);
	for (mesh_node_t * m = mesh;;) {
		m = m->sibling;
		if (m == NULL) {
			break;
		}

		meshes_free_recursive(m);
	}
}

static void update_settings(unsigned int change) {
	if (change & KGFW_GRAPHICS_SETTINGS_VSYNC) {
		if (state.window != NULL) {
			glfwSwapInterval((state.settings & KGFW_GRAPHICS_SETTINGS_VSYNC));
		}
	}
}

static int gfx_command(int argc, char ** argv) {
	const char * subcommands = "set    enable    disable    reload";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}
	
	if (strcmp("reload", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("shaders", argv[2]) == 0) {
			GL_CALL(glDeleteProgram(state.program));
			state.program = GL_CALL(glCreateProgram());
			int r = shaders_load("assets/shaders/vertex.glsl", "assets/shaders/fragment.glsl", &state.program);
			if (r != 0) {
				return r;
			}
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	} else if (strcmp("enable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	} else if (strcmp("disable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	} else if (strcmp("options", argv[1]) == 0) {
		const char * options = "vsync    shaders";
		const char * arguments = "[option]    see 'gfx options'";
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "options: %s", options);
	} else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}
	return 0;
}

static void register_commands(void) {
	kgfw_console_register_command("gfx", gfx_command);
}

static void gl_errors(void) {
	GLenum err = -1;
	while (1) {
		err = glGetError();
		if (err == GL_NO_ERROR) {
			break;
		}
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "err %i", err);
	}
}

static int shaders_load(const char * vpath, const char * fpath, GLuint * out_program) {
	const GLchar * fallback_vshader =
		"#version 330 core\n"
		"layout(location = 0) in vec3 in_pos; layout(location = 1) in vec3 in_color; layout(location = 2) in vec3 in_normal; layout(location = 3) in vec2 in_uv; uniform mat4 unif_m; uniform mat4 unif_vp; out vec3 v_pos; out vec3 v_color; out vec3 v_normal; out vec2 v_uv; void main() { gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0); v_pos = vec3(unif_m * vec4(in_pos, 1.0)); v_color = in_color; v_normal = in_normal; v_uv = in_uv; }";
	const GLchar * fallback_fshader =
		"#version 330 core\n"
		"in vec3 v_pos; in vec3 v_color; in vec3 v_normal; in vec2 v_uv; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

	GLchar * vshader = (GLchar *) fallback_vshader;
	GLchar * fshader = (GLchar *) fallback_fshader;
	{
		FILE * fp = fopen(vpath, "rb");
		unsigned long long int length = 0;
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"assets/shaders/vertex.glsl\" falling back to default shader");
			vshader = (GLchar *) fallback_vshader;
			goto load_fshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		vshader = malloc(length + 1);
		if (vshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (GLchar *) fallback_vshader;
			goto load_fshader;
		}
		if (fread(vshader, 1, length, fp) != length) {
			length = 0;
			free(vshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (GLchar *) fallback_vshader;
			goto load_fshader;
		}
		vshader[length] = '\0';
		fclose(fp);
	load_fshader:
		fp = fopen(fpath, "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", fpath);
			fshader = (GLchar *) fallback_fshader;
			goto end_fshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		fshader = malloc(length + 1);
		if (fshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", fpath);
			fshader = (GLchar *) fallback_fshader;
			goto end_fshader;
		}
		if (fread(fshader, 1, length, fp) != length) {
			length = 0;
			free(fshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", fpath);
			fshader = (GLchar *) fallback_fshader;
			goto end_fshader;
		}
		fshader[length] = '\0';
		fclose(fp);
	end_fshader:;
	}
	GLuint vert = GL_CALL(glCreateShader(GL_VERTEX_SHADER));
	GL_CALL(glShaderSource(vert, 1, (const GLchar * const *) &vshader, NULL));
	GL_CALL(glCompileShader(vert));
	GLint success = GL_TRUE;
	GL_CALL(glGetShaderiv(vert, GL_COMPILE_STATUS, &success));
	if (success == GL_FALSE) {
		char msg[512];
		GL_CALL(glGetShaderInfoLog(vert, 512, NULL, msg));
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided vertex shader compilation error: %s", msg);
	vfallback_compilation:
		vshader = (GLchar *) fallback_vshader;
		GL_CALL(glShaderSource(vert, 1, (const GLchar * const *) &vshader, NULL));
		GL_CALL(glCompileShader(vert));
		GL_CALL(glGetShaderiv(vert, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			GL_CALL(glGetShaderInfoLog(vert, 512, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback vertex shader compilation error: %s", msg);
			return 2;
		}
	}
	GLuint frag = GL_CALL(glCreateShader(GL_FRAGMENT_SHADER));
	GL_CALL(glShaderSource(frag, 1, (const GLchar * const *) &fshader, NULL));
	GL_CALL(glCompileShader(frag));
	
	GL_CALL(glGetShaderiv(frag, GL_COMPILE_STATUS, &success));
	if (success == GL_FALSE) {
		char msg[512];
		GL_CALL(glGetShaderInfoLog(frag, 512, NULL, msg));
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided fragment shader compilation error: %s", msg);
	ffallback_compilation:
		fshader = (GLchar *) fallback_fshader;
		GL_CALL(glShaderSource(frag, 1, (const GLchar * const *) &fshader, NULL));
		GL_CALL(glCompileShader(frag));
		GL_CALL(glGetShaderiv(frag, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			GL_CALL(glGetShaderInfoLog(frag, 512, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback fragment shader compilation error: %s", msg);
			return 2;
		}
	}

	GL_CALL(glAttachShader(*out_program, vert));
	GL_CALL(glAttachShader(*out_program, frag));
	GL_CALL(glLinkProgram(*out_program));
	GL_CALL(glDeleteShader(vert));
	GL_CALL(glDeleteShader(frag));

	return 0;
}
