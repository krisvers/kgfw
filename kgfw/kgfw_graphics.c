#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
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

	struct {
		GLuint vao;
		GLuint vbo;
		GLuint ibo;
		GLuint program;

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
	GLint unif_mvp;
	mat4x4 vp;

	mesh_node_t * mesh_root;

	unsigned int settings;
} static state = {
	NULL, NULL,
	0, 0, 0, 0,
	{ 0 },
	NULL,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
};

struct {
	mat4x4 model;
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
static void gl_errors(void);

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

	{
		const GLchar * fallback_vshader =
			"#version 330 core\n"
			"layout(location = 0) in vec3 in_pos; layout(location = 1) in vec3 in_color; layout(location = 2) in vec3 in_normal; layout(location = 3) in vec2 in_uv; uniform mat4 unif_mvp; out vec3 v_pos; out vec3 v_color; out vec3 v_normal; out vec2 v_uv; void main() { gl_Position = unif_mvp * vec4(in_pos, 1.0); v_pos = in_pos; v_color = in_color; v_normal = in_normal; v_uv = in_uv; }";

		const GLchar * fallback_fshader =
			"#version 330 core\n"
			"in vec3 v_pos; in vec3 v_color; in vec3 v_normal; in vec2 v_uv; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

		GLchar * vshader = (GLchar *) fallback_vshader;
		GLchar * fshader = (GLchar *) fallback_fshader;

		{
			FILE * fp = fopen("assets/shaders/vertex.glsl", "rb");
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
				kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"assets/shaders/vertex.glsl\" falling back to default shader");
				vshader = (GLchar *) fallback_vshader;
				goto load_fshader;
			}

			if (fread(vshader, 1, length, fp) != length) {
				length = 0;
				free(vshader);
				fclose(fp);
				kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"assets/shaders/vertex.glsl\" falling back to default shader");
				vshader = (GLchar *) fallback_vshader;
				goto load_fshader;
			}
			vshader[length] = '\0';

			fclose(fp);

		load_fshader:
			fp = fopen("assets/shaders/fragment.glsl", "rb");
			if (fp == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"assets/shaders/fragment.glsl\" falling back to default shader");
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
				kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"assets/shaders/fragment.glsl\" falling back to default shader");
				fshader = (GLchar *) fallback_fshader;
				goto end_fshader;
			}

			if (fread(fshader, 1, length, fp) != length) {
				length = 0;
				free(fshader);
				fclose(fp);
				kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"assets/shaders/fragment.glsl\" falling back to default shader");
				fshader = (GLchar *) fallback_fshader;
				goto end_fshader;
			}
			fshader[length] = '\0';

			fclose(fp);

		end_fshader:;
		}

		state.vshader = GL_CALL(glCreateShader(GL_VERTEX_SHADER));
		GL_CALL(glShaderSource(state.vshader, 1, (const GLchar * const *) &vshader, NULL));
		GL_CALL(glCompileShader(state.vshader));

		GLint success = GL_TRUE;
		GL_CALL(glGetShaderiv(state.vshader, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			char msg[512];
			GL_CALL(glGetShaderInfoLog(state.vshader, 512, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided vertex shader compilation error: \"%s\"", msg);

		vfallback_compilation:
			vshader = (GLchar *) fallback_vshader;
			GL_CALL(glShaderSource(state.vshader, 1, (const GLchar * const *) &vshader, NULL));
			GL_CALL(glCompileShader(state.vshader));
			GL_CALL(glGetShaderiv(state.vshader, GL_COMPILE_STATUS, &success));
			if (success == GL_FALSE) {
				GL_CALL(glGetShaderInfoLog(state.vshader, 512, NULL, msg));
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback vertex shader compilation error: \"%s\"", msg);
				return 2;
			}
		}

		state.fshader = GL_CALL(glCreateShader(GL_FRAGMENT_SHADER));
		GL_CALL(glShaderSource(state.fshader, 1, (const GLchar * const *) &fshader, NULL));
		GL_CALL(glCompileShader(state.fshader));
		
		GL_CALL(glGetShaderiv(state.fshader, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			char msg[512];
			GL_CALL(glGetShaderInfoLog(state.fshader, 512, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided fragment shader compilation error: \"%s\"", msg);

		ffallback_compilation:
			fshader = (GLchar *) fallback_fshader;
			GL_CALL(glShaderSource(state.fshader, 1, (const GLchar * const *) &fshader, NULL));
			GL_CALL(glCompileShader(state.fshader));
			GL_CALL(glGetShaderiv(state.fshader, GL_COMPILE_STATUS, &success));
			if (success == GL_FALSE) {
				GL_CALL(glGetShaderInfoLog(state.fshader, 512, NULL, msg));
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback fragment shader compilation error: \"%s\"", msg);
				return 2;
			}
		}
	}

	state.program = GL_CALL(glCreateProgram());
	GL_CALL(glAttachShader(state.program, state.vshader));
	GL_CALL(glAttachShader(state.program, state.fshader));
	GL_CALL(glLinkProgram(state.program));
	GL_CALL(glUseProgram(state.program));

	state.unif_mvp = GL_CALL(glGetUniformLocation(state.program, "unif_mvp"));

	GL_CALL(glEnable(GL_DEPTH_TEST));

	return 0;
}

void kgfw_graphics_draw(void) {
	GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GL_CALL(glClearColor(0.15f, 0.1f, 0.175f, 1.0f));

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
		} else {
			mesh_node_t * n;
			for (n = state.mesh_root; n->sibling != NULL; n = n->sibling);
			n->sibling = node;
		}
		return (kgfw_graphics_mesh_node_t *) node;
	}

	if (parent->child == NULL) {
		parent->child = (kgfw_graphics_mesh_node_t *) node;
	} else {
		mesh_node_t * n;
		for (n = (mesh_node_t *) parent->child; n->sibling != NULL; n = n->sibling);
		n->sibling = node;
	}
	
	return (kgfw_graphics_mesh_node_t *) node;
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
	if (node->gl.vao != 0) {
		glDeleteProgram(node->gl.vao);
	}
	if (node->gl.vbo != 0) {
		glDeleteBuffers(1, &node->gl.vbo);
	}
	if (node->gl.ibo != 0) {
		glDeleteBuffers(1, &node->gl.ibo);
	}
	if (node->gl.program != 0) {
		glDeleteProgram(node->gl.program);
	}

	free(node);
}

static void meshes_gen(mesh_node_t * node) {
	GL_CALL(glGenVertexArrays(1, &node->gl.vao));
	GL_CALL(glGenBuffers(1, &node->gl.vbo));
	GL_CALL(glGenBuffers(1, &node->gl.ibo));
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
		recurse_state.pos[0] = 0;
		recurse_state.pos[1] = 0;
		recurse_state.pos[2] = 0;
		recurse_state.rot[0] = 0;
		recurse_state.rot[1] = 0;
		recurse_state.rot[2] = 0;
		recurse_state.scale[0] = 1;
		recurse_state.scale[1] = 1;
		recurse_state.scale[2] = 1;
		mat4x4_identity(out_m);
		mat4x4_translate(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], -recurse_state.pos[0] - mesh->transform.pos[2]);
	} else {
		recurse_state.pos[0] += mesh->transform.pos[0];
		recurse_state.pos[1] += mesh->transform.pos[1];
		recurse_state.pos[2] += mesh->transform.pos[2];
		recurse_state.rot[0] += mesh->transform.rot[0];
		recurse_state.rot[1] += mesh->transform.rot[1];
		recurse_state.rot[2] += mesh->transform.rot[2];
		recurse_state.scale[0] *= mesh->transform.scale[0];
		recurse_state.scale[1] *= mesh->transform.scale[1];
		recurse_state.scale[2] *= mesh->transform.scale[2];
		mat4x4_translate_in_place(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], -recurse_state.pos[2] - mesh->transform.pos[2]);
	}
	mat4x4_rotate_X(out_m, out_m, (recurse_state.rot[0] + mesh->transform.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(out_m, out_m, (recurse_state.rot[1] + mesh->transform.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(out_m, out_m, (recurse_state.rot[2] + mesh->transform.rot[2]) * 3.141592f / 180.0f);
	mat4x4_scale_aniso(out_m, out_m, recurse_state.scale[0] * mesh->transform.scale[0], recurse_state.scale[1] * mesh->transform.scale[1], recurse_state.scale[2] * mesh->transform.scale[2]);
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
	GLint uniform = GL_CALL(glGetUniformLocation(state.program, "unif_mvp"));

	mat4x4 mvp;
	mat4x4_identity(mvp);
	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);
	mat4x4_mul(mvp, state.vp, out_m);

	GL_CALL(glUniformMatrix4fv(uniform, 1, GL_FALSE, &mvp[0][0]));
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
	
	if (strcmp("enable", argv[1]) == 0) {
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
