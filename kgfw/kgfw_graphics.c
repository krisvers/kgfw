#include "kgfw_defines.h"

#if (KGFW_OPENGL == 33)
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
#define GL_CHECK_ERROR() { GLenum err = glGetError(); if (err != GL_NO_ERROR) { kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "(%s:%u) OpenGL Error (%u 0x%X) %s", __FILE__, __LINE__, err, err, (err == 0x500) ? "INVALID ENUM" : (err == 0x501) ? "INVALID VALUE" : (err == 0x502) ? "INVALID OPERATION" : (err == 0x503) ? "STACK OVERFLOW" : (err == 0x504) ? "STACK UNDERFLOW" : (err == 0x505) ? "OUT OF MEMORY" : (err == 0x506) ? "INVALID FRAMEBUFFER OPERATION" : "UNKNOWN"); abort(); } }
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

	struct {
		mat4x4 translation;
		mat4x4 rotation;
		mat4x4 scale;
	} matrices;

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
	//GL_CALL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO));

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
	}
	else if (use == KGFW_GRAPHICS_TEXTURE_USE_NORMAL) {
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
	}
	else if (use == KGFW_GRAPHICS_TEXTURE_USE_NORMAL) {
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
	GL_CALL(glEnableVertexAttribArray(0));
	GL_CALL(glEnableVertexAttribArray(1));
	GL_CALL(glEnableVertexAttribArray(2));
	GL_CALL(glEnableVertexAttribArray(3));

	if (parent == NULL) {
		if (state.mesh_root == NULL) {
			state.mesh_root = node;
		}
		else {
			mesh_node_t * n;
			for (n = state.mesh_root; n->sibling != NULL; n = n->sibling);
			n->sibling = node;
			node->prior_sibling = n;
		}
		return (kgfw_graphics_mesh_node_t *) node;
	}

	if (parent->child == NULL) {
		parent->child = (kgfw_graphics_mesh_node_t *) node;
	}
	else {
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
	if (mesh->child != NULL) {
		mesh->child->parent = mesh->parent;
	}
	if (mesh->sibling != NULL) {
		mesh->sibling->prior_sibling = mesh->prior_sibling;
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
	}
	else {
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
	GLint uniform_model = GL_CALL(glGetUniformLocation(program, "unif_m"));
	GLint uniform_vp = GL_CALL(glGetUniformLocation(program, "unif_vp"));
	GLint uniform_time = GL_CALL(glGetUniformLocation(program, "unif_time"));
	GLint uniform_view = GL_CALL(glGetUniformLocation(program, "unif_view_pos"));
	GLint uniform_texture_color = GL_CALL(glGetUniformLocation(program, "unif_texture_color"));
	GLint uniform_texture_normal = GL_CALL(glGetUniformLocation(program, "unif_texture_normal"));

	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);

	GL_CALL(glUniformMatrix4fv(uniform_model, 1, GL_FALSE, &out_m[0][0]));
	GL_CALL(glUniformMatrix4fv(uniform_vp, 1, GL_FALSE, &state.vp[0][0]));
	GL_CALL(glUniform1f(uniform_time, kgfw_time_get()));
	GL_CALL(glUniform3f(uniform_view, state.camera->pos[0], state.camera->pos[1], state.camera->pos[2]));

	GL_CALL(glUniform1i(uniform_texture_color, 0));
	if (mesh->gl.tex == 0) {
		GL_CALL(glActiveTexture(GL_TEXTURE0));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
	}
	else {
		GL_CALL(glActiveTexture(GL_TEXTURE0));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, mesh->gl.tex));
	}

	GL_CALL(glUniform1i(uniform_texture_normal, 1));
	if (mesh->gl.normal == 0) {
		GL_CALL(glActiveTexture(GL_TEXTURE1));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
	}
	else {
		GL_CALL(glActiveTexture(GL_TEXTURE1));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, mesh->gl.normal));
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
	}
	else if (strcmp("enable", argv[1]) == 0) {
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
	}
	else if (strcmp("disable", argv[1]) == 0) {
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
	}
	else if (strcmp("options", argv[1]) == 0) {
		const char * options = "vsync    shaders";
		const char * arguments = "[option]    see 'gfx options'";
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "options: %s", options);
	}
	else {
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

#elif (KGFW_DIRECTX == 11)

#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
#include "kgfw_time.h"
#include "kgfw_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linmath.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

#define KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_INDICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_SHADOW_MAP_WIDTH 2048
#define KGFW_GRAPHICS_DEFAULT_SHADOW_MAP_HEIGHT 2048

#ifdef KGFW_DEBUG
#define D3D11_CALL(statement) { HRESULT hr = statement; if (FAILED(hr)) { kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "D3D11 Error (%i 0x%X) %s:%u", hr, hr, __FILE__, __LINE__); abort(); } }
#define D3D11_CALL_DO(statement, todo) { HRESULT hr = statement; if (FAILED(hr)) { todo; abort(); } }
#else
#define D3D11_CALL(statement) statement;
#define D3D11_CALL_DO(statement, todo)
#endif

typedef struct mesh_node {
	struct {
		float pos[3];
		float rot[3];
		float scale[3];
		unsigned char absolute;
	} transform;

	struct {
		mat4x4 translation;
		mat4x4 rotation;
		mat4x4 scale;
	} matrices;

	struct mesh_node * parent;
	struct mesh_node * child;
	struct mesh_node * sibling;
	struct mesh_node * prior_sibling;

	struct {
		ID3D11Buffer * vbo;
		ID3D11Buffer * ibo;
		ID3D11ShaderResourceView * tex;
		ID3D11SamplerState * sampler;

		unsigned long long int vbo_size;
		unsigned long long int ibo_size;
	} d3d11;
} mesh_node_t;

struct kgfw_graphics_mesh_node_internal_t {
	void * _a;
	void * _b;
	void * _c;
	void * _d;

	unsigned long long int _e;
	unsigned long long int _f;
};

typedef struct mesh_shader_uniform {
	mat4x4 mvp;
} mesh_shader_uniform_t;

struct {
	kgfw_window_t * window;
	kgfw_camera_t * camera;
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

	ID3D11Device * dev;
	ID3D11DeviceContext * devctx;
	IDXGISwapChain1 * swapchain;
	ID3D11InputLayout * layout;
	ID3D11VertexShader * vshader;
	ID3D11PixelShader * pshader;
	ID3D11Buffer * ubuffer;
	ID3D11BlendState * blend;
	ID3D11RasterizerState * rasterizer;
	ID3D11DepthStencilState * depth;
	ID3D11DepthStencilView * depth_view;
	ID3D11RenderTargetView * target;
} static state = {
	NULL, NULL,
	{ 0 },
	NULL,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
	{
		{ 0, 100, 0 },
		{ 1, 1, 1 },
		0.0f, 0.5f, 0.25f, 8
	},
	NULL, NULL,
	NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL,
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

static int shaders_load(const char * vpath, const char * ppath, ID3D11VertexShader ** out_vshader, ID3D11PixelShader ** out_pshader);

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
	{
		D3D_FEATURE_LEVEL features_levels[1] = { D3D_FEATURE_LEVEL_11_0 };

		UINT flags = 0;
		#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif

		D3D11_CALL(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, features_levels, 1, D3D11_SDK_VERSION, &state.dev, NULL, &state.devctx));

		{
			if (shaders_load("assets/shaders/vertex.hlsl", "assets/shaders/pixel.hlsl", &state.vshader, &state.pshader) != 0) {
				return 1;
			}

			D3D11_BUFFER_DESC desc = {
				.ByteWidth = sizeof(mesh_shader_uniform_t),
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
			};

			D3D11_CALL(state.dev->lpVtbl->CreateBuffer(state.dev, &desc, NULL, &state.ubuffer));
		}

		#ifdef _DEBUG
		ID3D11InfoQueue * iq;
		D3D11_CALL(state.dev->lpVtbl->QueryInterface(state.dev, &IID_ID3D11InfoQueue, &iq));
		D3D11_CALL(iq->lpVtbl->SetBreakOnSeverity(iq, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		D3D11_CALL(iq->lpVtbl->SetBreakOnSeverity(iq, D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
		iq->lpVtbl->Release(iq);

		HMODULE dxgidbg = LoadLibraryA("dxgidebug.dll");
		if (dxgidbg != NULL) {
			HRESULT(WINAPI * dxgiGetDebugInterface)(REFIID riid, void ** ppDebug);
			*(FARPROC *)&dxgiGetDebugInterface = GetProcAddress(dxgidbg, "DXGIGetDebugInterface");

			IDXGIInfoQueue * dxgiiq;
			D3D11_CALL(dxgiGetDebugInterface(&IID_IDXGIInfoQueue, &dxgiiq));
			D3D11_CALL(dxgiiq->lpVtbl->SetBreakOnSeverity(dxgiiq, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			D3D11_CALL(dxgiiq->lpVtbl->SetBreakOnSeverity(dxgiiq, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
			dxgiiq->lpVtbl->Release(dxgiiq);
		}
		#endif
	}

	{
		IDXGIDevice * dev;;
		D3D11_CALL(state.dev->lpVtbl->QueryInterface(state.dev, &IID_IDXGIDevice, &dev));

		IDXGIAdapter * adapter;
		D3D11_CALL(dev->lpVtbl->GetAdapter(dev, &adapter));

		IDXGIFactory2 * factory;
		D3D11_CALL(adapter->lpVtbl->GetParent(adapter, &IID_IDXGIFactory2, &factory));

		DXGI_SWAP_CHAIN_DESC1 desc;
		SecureZeroMemory(&desc, sizeof(desc));
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.Width = state.window->width;
		desc.Height = state.window->height;

		D3D11_CALL(factory->lpVtbl->CreateSwapChainForHwnd(factory, (IUnknown *) state.dev, state.window->internal, &desc, NULL, NULL, &state.swapchain));
		D3D11_CALL(factory->lpVtbl->MakeWindowAssociation(factory, state.window->internal, DXGI_MWA_NO_ALT_ENTER));
		factory->lpVtbl->Release(factory);
		adapter->lpVtbl->Release(adapter);
		dev->lpVtbl->Release(dev);
	}

	{
		D3D11_BLEND_DESC desc = {
			.RenderTarget[0] = {
				.BlendEnable = TRUE,
				.SrcBlend = D3D11_BLEND_SRC_ALPHA,
				.DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
				.BlendOp = D3D11_BLEND_OP_ADD,
				.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA,
				.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
				.BlendOpAlpha = D3D11_BLEND_OP_ADD,
				.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
			},
		};

		D3D11_CALL(state.dev->lpVtbl->CreateBlendState(state.dev, &desc, &state.blend));
	}

	{ 
		D3D11_RASTERIZER_DESC desc = {
			.FillMode = D3D11_FILL_SOLID,
			.CullMode = D3D11_CULL_NONE,
		};
		D3D11_CALL(state.dev->lpVtbl->CreateRasterizerState(state.dev, &desc, &state.rasterizer));


		ID3D11Texture2D * backbuffer;
		D3D11_CALL(state.swapchain->lpVtbl->GetBuffer(state.swapchain, 0, &IID_ID3D11Texture2D, &backbuffer));
		D3D11_CALL(state.dev->lpVtbl->CreateRenderTargetView(state.dev, (ID3D11Resource *) backbuffer, NULL, &state.target));
		backbuffer->lpVtbl->Release(backbuffer);
	}

	{
		D3D11_DEPTH_STENCIL_DESC desc = {
			TRUE,
			D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS,
			TRUE,
			0xFF, 0xFF,
			{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_INCR, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
			{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_DECR, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
		};

		D3D11_CALL(state.dev->lpVtbl->CreateDepthStencilState(state.dev, &desc, &state.depth));
	}

	{
		ID3D11Texture2D * depth;
		{
			D3D11_TEXTURE2D_DESC desc = {
				state.window->width, state.window->height,
				1, 1,
				DXGI_FORMAT_D32_FLOAT,
				{ 1, 0 },
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_DEPTH_STENCIL,
				0, 0
			};
			D3D11_CALL(state.dev->lpVtbl->CreateTexture2D(state.dev, &desc, NULL, &depth));
		}

		{
			D3D11_DEPTH_STENCIL_VIEW_DESC desc = {
				DXGI_FORMAT_D32_FLOAT,
				D3D11_DSV_DIMENSION_TEXTURE2D,
			};
			desc.Texture2D.MipSlice = 0;

			D3D11_CALL(state.dev->lpVtbl->CreateDepthStencilView(state.dev, (ID3D11Resource *) depth, &desc, &state.depth_view));
		}
		depth->lpVtbl->Release(depth);
	}

	update_settings(state.settings);

	return 0;
}

void kgfw_graphics_draw(void) {
	float color[4] = { 0.57f, 0.59f, 0.58f, 1.0f };
	state.devctx->lpVtbl->ClearRenderTargetView(state.devctx, state.target, color);
	state.devctx->lpVtbl->ClearDepthStencilView(state.devctx, state.depth_view, D3D11_CLEAR_DEPTH, 1, 0xFF);

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

	state.swapchain->lpVtbl->Present(state.swapchain, (state.settings & KGFW_GRAPHICS_SETTINGS_VSYNC) != 0, 0);
}

void kgfw_graphics_mesh_texture(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_t * texture, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;

	if (m->d3d11.sampler != NULL) {
		m->d3d11.sampler->lpVtbl->Release(m->d3d11.sampler);
		m->d3d11.sampler = NULL;
	}

	{
		D3D11_FILTER filter = (texture->filtering == KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST) ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		D3D11_TEXTURE_ADDRESS_MODE u = (texture->u_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		D3D11_TEXTURE_ADDRESS_MODE v = (texture->v_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		D3D11_TEXTURE_ADDRESS_MODE w = (texture->v_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;

		D3D11_SAMPLER_DESC desc = {
			filter,
			u, v, w
		};

		D3D11_CALL(state.dev->lpVtbl->CreateSamplerState(state.dev, &desc, &m->d3d11.sampler));
	}

	{
		DXGI_FORMAT fmt = (texture->fmt == KGFW_GRAPHICS_TEXTURE_FORMAT_RGBA) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;

		D3D11_TEXTURE2D_DESC desc = {
			texture->width, texture->height,
			1, 1,
			fmt,
			{ 1, 0 },
			D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE,
		};

		D3D11_SUBRESOURCE_DATA init = {
			texture->bitmap,
			texture->width * sizeof(unsigned int),
			texture->height * sizeof(unsigned int),
		};

		ID3D11Texture2D * tex;
		D3D11_CALL(state.dev->lpVtbl->CreateTexture2D(state.dev, &desc, &init, &tex));
		D3D11_CALL(state.dev->lpVtbl->CreateShaderResourceView(state.dev, (ID3D11Resource *) tex, NULL, &m->d3d11.tex));
		tex->lpVtbl->Release(tex);
	}
}

void kgfw_graphics_mesh_texture_detach(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	if (m->d3d11.sampler != NULL) {
		m->d3d11.sampler->lpVtbl->Release(m->d3d11.sampler);
		m->d3d11.sampler = NULL;
	}
	if (m->d3d11.tex != NULL) {
		m->d3d11.tex->lpVtbl->Release(m->d3d11.tex);
		m->d3d11.tex = NULL;
	}
}

kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent) {
	mesh_node_t * node = meshes_new();
	node->parent = (mesh_node_t *) parent;
	memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
	memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
	memcpy(node->transform.scale, mesh->scale, sizeof(vec3));
	
	{
		D3D11_BUFFER_DESC desc = {
			0
		};

		D3D11_SUBRESOURCE_DATA init = {
			0
		};

		desc.ByteWidth = mesh->vertices_count * sizeof(kgfw_graphics_vertex_t);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		init.pSysMem = mesh->vertices;

		D3D11_CALL(state.dev->lpVtbl->CreateBuffer(state.dev, &desc, &init, &node->d3d11.vbo));

		desc.ByteWidth = mesh->indices_count * sizeof(unsigned int);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		init.pSysMem = mesh->indices;
		D3D11_CALL(state.dev->lpVtbl->CreateBuffer(state.dev, &desc, &init, &node->d3d11.ibo));

		node->d3d11.vbo_size = mesh->vertices_count;
		node->d3d11.ibo_size = mesh->indices_count;
	}

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
	if (mesh->child != NULL) {
		mesh->child->parent = mesh->parent;
	}
	if (mesh->sibling != NULL) {
		mesh->sibling->prior_sibling = mesh->prior_sibling;
	}

	if (state.mesh_root == (mesh_node_t *) mesh) {
		state.mesh_root = NULL;
	}

	meshes_free((mesh_node_t *) mesh);
}

void kgfw_graphics_debug_line(vec3 p0, vec3 p1) {
}

void kgfw_graphics_set_window(kgfw_window_t * window) {
	state.window = window;
	if (window != NULL) {
		if (window->internal != NULL) {
		}
	}
}

void kgfw_graphics_viewport(unsigned int width, unsigned int height) {
	state.devctx->lpVtbl->ClearState(state.devctx);
	state.target->lpVtbl->Release(state.target);
	
	D3D11_CALL(state.swapchain->lpVtbl->ResizeBuffers(state.swapchain, 0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
	ID3D11Texture2D * backbuffer;
	D3D11_CALL(state.swapchain->lpVtbl->GetBuffer(state.swapchain, 0, &IID_ID3D11Texture2D, &backbuffer));
	D3D11_CALL(state.dev->lpVtbl->CreateRenderTargetView(state.dev, (ID3D11Resource *) backbuffer, NULL, &state.target));
	backbuffer->lpVtbl->Release(backbuffer);
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

	if (node->d3d11.vbo != NULL) {
		node->d3d11.vbo->lpVtbl->Release(node->d3d11.vbo);
	}
	if (node->d3d11.ibo != NULL) {
		node->d3d11.ibo->lpVtbl->Release(node->d3d11.ibo);
	}
	if (node->d3d11.tex != NULL) {
		node->d3d11.tex->lpVtbl->Release(node->d3d11.tex);
	}
	if (node->d3d11.sampler != NULL) {
		node->d3d11.sampler->lpVtbl->Release(node->d3d11.sampler);
	}

	free(node);
}

static mesh_node_t * meshes_new(void) {
	mesh_node_t * m = meshes_alloc();	
	if (m == NULL) {
		return NULL;
	}

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

	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);

	mat4x4 mvp;
	mat4x4_mul(mvp, state.vp, out_m);

	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		D3D11_CALL(state.devctx->lpVtbl->Map(state.devctx, (ID3D11Resource *) state.ubuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		CopyMemory(mapped.pData, mvp, sizeof(mvp));
		state.devctx->lpVtbl->Unmap(state.devctx, (ID3D11Resource *) state.ubuffer, 0);
	}

	state.devctx->lpVtbl->IASetInputLayout(state.devctx, state.layout);
	state.devctx->lpVtbl->IASetPrimitiveTopology(state.devctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(kgfw_graphics_vertex_t);
	UINT offset = 0;
	state.devctx->lpVtbl->IASetVertexBuffers(state.devctx, 0, 1, &mesh->d3d11.vbo, &stride, &offset);
	state.devctx->lpVtbl->IASetIndexBuffer(state.devctx, mesh->d3d11.ibo, DXGI_FORMAT_R32_UINT, 0);

	state.devctx->lpVtbl->VSSetConstantBuffers(state.devctx, 0, 1, &state.ubuffer);
	state.devctx->lpVtbl->VSSetShader(state.devctx, state.vshader, NULL, 0);

	D3D11_VIEWPORT viewport = {
		.TopLeftX = 0,
		.TopLeftY = 0,
		.Width = (float) state.window->width,
		.Height = (float) state.window->height,
		.MinDepth = 0,
		.MaxDepth = 1,
	};
	state.devctx->lpVtbl->RSSetViewports(state.devctx, 1, &viewport);
	state.devctx->lpVtbl->RSSetState(state.devctx, state.rasterizer);

	state.devctx->lpVtbl->PSSetSamplers(state.devctx, 0, 1, &mesh->d3d11.sampler);
	state.devctx->lpVtbl->PSSetShaderResources(state.devctx, 0, 1, &mesh->d3d11.tex);
	state.devctx->lpVtbl->PSSetShader(state.devctx, state.pshader, NULL, 0);

	state.devctx->lpVtbl->OMSetDepthStencilState(state.devctx, state.depth, 1);
	state.devctx->lpVtbl->OMSetBlendState(state.devctx, state.blend, NULL, 0xFFFFFFFFFFFFFFFF);
	state.devctx->lpVtbl->OMSetRenderTargets(state.devctx, 1, &state.target, state.depth_view);
	
	state.devctx->lpVtbl->DrawIndexed(state.devctx, mesh->d3d11.ibo_size, 0, 0);
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
			//glfwSwapInterval((state.settings & KGFW_GRAPHICS_SETTINGS_VSYNC));
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
			if (state.vshader != NULL) {
				state.vshader->lpVtbl->Release(state.vshader);
				state.vshader = NULL;
			}
			if (state.pshader != NULL) {
				state.pshader->lpVtbl->Release(state.pshader);
				state.pshader = NULL;
			}
			if (shaders_load("assets/shaders/vertex.hlsl", "assets/shaders/pixel.hlsl", &state.vshader, &state.pshader) != 0) {
				return 1;
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

static int shaders_load(const char * vpath, const char * ppath, ID3D11VertexShader ** out_vshader, ID3D11PixelShader ** out_pshader) {
	const char * fallback_vshader =
		"#version 330 core\n"
		"layout(location = 0) in vec3 in_pos; layout(location = 1) in vec3 in_color; layout(location = 2) in vec3 in_normal; layout(location = 3) in vec2 in_uv; uniform mat4 unif_m; uniform mat4 unif_vp; out vec3 v_pos; out vec3 v_color; out vec3 v_normal; out vec2 v_uv; void main() { gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0); v_pos = vec3(unif_m * vec4(in_pos, 1.0)); v_color = in_color; v_normal = in_normal; v_uv = in_uv; }";
	const char * fallback_pshader =
		"#version 330 core\n"
		"in vec3 v_pos; in vec3 v_color; in vec3 v_normal; in vec2 v_uv; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

	char * vshader = (char *) fallback_vshader;
	char * pshader = (char *) fallback_pshader;
	unsigned long long int vlen = strlen(vshader);
	unsigned long long int plen = strlen(pshader);
	{
		FILE * fp = fopen(vpath, "rb");
		unsigned long long int length = 0;
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (char *) fallback_vshader;
			goto load_pshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		vshader = malloc(length + 1);
		if (vshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (char *) fallback_vshader;
			goto load_pshader;
		}
		if (fread(vshader, 1, length, fp) != length) {
			length = 0;
			free(vshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (char *) fallback_vshader;
			goto load_pshader;
		}
		vshader[length] = '\0';
		fclose(fp);
		vlen = length;
	load_pshader:
		fp = fopen(ppath, "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", ppath);
			pshader = (char *) fallback_pshader;
			goto end_pshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		pshader = malloc(length + 1);
		if (pshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", ppath);
			pshader = (char *) fallback_pshader;
			goto end_pshader;
		}
		if (fread(pshader, 1, length, fp) != length) {
			length = 0;
			free(pshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", ppath);
			pshader = (char *) fallback_pshader;
			goto end_pshader;
		}
		pshader[length] = '\0';
		fclose(fp);
		plen = length;
	end_pshader:;
	}

	D3D11_INPUT_ELEMENT_DESC desc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(kgfw_graphics_vertex_t, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(kgfw_graphics_vertex_t, nx), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(kgfw_graphics_vertex_t, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS;
	#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif

	ID3DBlob * err;
	ID3DBlob * vblob;
	ID3DBlob * pblob;

	D3D11_CALL_DO(D3DCompile(vshader, vlen, vpath, NULL, NULL, "vs", "vs_5_0", flags, 0, &vblob, &err),
		{
			if (err != NULL) {
				const char * message = err->lpVtbl->GetBufferPointer(err);
				OutputDebugStringA(message);
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "D3DCompile Failed!", "%s", message);
			}
		}
	);

	if (vblob == NULL) {
		return 2;
	}

	D3D11_CALL_DO(D3DCompile(pshader, plen, ppath, NULL, NULL, "ps", "ps_5_0", flags, 0, &pblob, &err),
		{
			if (err != NULL) {
				const char * message = err->lpVtbl->GetBufferPointer(err);
				OutputDebugStringA(message);
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "D3DCompile Failed!", "%s", message);
			}
		}
	);

	if (vblob == NULL) {
		return 3;
	}

	D3D11_CALL(state.dev->lpVtbl->CreateVertexShader(state.dev, vblob->lpVtbl->GetBufferPointer(vblob), vblob->lpVtbl->GetBufferSize(vblob), NULL, &state.vshader));
	D3D11_CALL(state.dev->lpVtbl->CreatePixelShader(state.dev, pblob->lpVtbl->GetBufferPointer(pblob), pblob->lpVtbl->GetBufferSize(pblob), NULL, &state.pshader));
	D3D11_CALL(state.dev->lpVtbl->CreateInputLayout(state.dev, desc, 4, vblob->lpVtbl->GetBufferPointer(vblob), vblob->lpVtbl->GetBufferSize(vblob), &state.layout));

	vblob->lpVtbl->Release(vblob);
	pblob->lpVtbl->Release(pblob);

	return 0;
}

#endif
