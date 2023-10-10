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
#define GL_CALL(statement) { statement; }
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
	GLuint vbo;
	GLuint ibo;
	GLuint vao;
	GLuint vshader;
	GLuint fshader;
	GLuint program;
	GLint unif_mvp;
	GLuint static_vbo;
	GLuint static_ibo;
	mat4x4 static_model;

	mesh_node_t * mesh_root;

	unsigned long long int vbo_size;
	unsigned long long int ibo_size;
	unsigned long long int static_vbo_size;
	unsigned long long int static_ibo_size;

	unsigned int settings;
} static state = {
	NULL, NULL,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	{ 0 },
	NULL,
	0, 0, 0, 0,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
};

struct {
	mat4x4 model;
	mat4x4 vp;
} recurse_state = {
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	},
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	}
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

int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera, kgfw_graphics_mesh_t * mesh) {
	register_commands();

	state.window = window;
	state.camera = camera;
	state.vbo_size = KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT;
	state.ibo_size = KGFW_GRAPHICS_DEFAULT_INDICES_COUNT;
	/*if (mesh != NULL) {
		state.static_vbo_size = mesh->vertices_count;
		state.static_ibo_size = mesh->indices_count;
	}*/

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
			"in vec3 in_pos; in vec3 in_color; uniform mat4 unif_mvp; out vec3 v_pos; out vec3 v_color; void main() { gl_Position = unif_mvp * vec4(in_pos, 1.0); v_pos = gl_Position.xyz; v_color = in_color; }";

		const GLchar * fallback_fshader =
			"#version 330 core\n"
			"in vec3 v_pos; in vec3 v_color; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

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
			GLsizei len = 0;
			GL_CALL(glGetShaderInfoLog(state.vshader, 0, &len, NULL));
			GLchar * msg = malloc(len + 1);
			if (msg == NULL) {
				goto vfallback_compilation;
			}
			msg[len] = '\0';
			GL_CALL(glGetShaderInfoLog(state.vshader, len, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided vertex shader compilation error: \"%s\"", msg);
			free(msg);

		vfallback_compilation:
			vshader = (GLchar *) fallback_vshader;
			GL_CALL(glShaderSource(state.vshader, 1, (const GLchar * const *) &vshader, NULL));
			GL_CALL(glCompileShader(state.vshader));
			GL_CALL(glGetShaderiv(state.vshader, GL_COMPILE_STATUS, &success));
			if (success == GL_FALSE) {
				len = 0;
				GL_CALL(glGetShaderInfoLog(state.vshader, 0, &len, NULL));
				msg = malloc(len + 1);
				if (msg == NULL) {
					return 2;
				}
				msg[len] = '\0';
				GL_CALL(glGetShaderInfoLog(state.vshader, len, NULL, msg));
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback vertex shader compilation error: \"%s\"", msg);
				free(msg);
				return 2;
			}
		}

		state.fshader = GL_CALL(glCreateShader(GL_FRAGMENT_SHADER));
		GL_CALL(glShaderSource(state.fshader, 1, (const GLchar * const *) &fshader, NULL));
		GL_CALL(glCompileShader(state.fshader));
		
		GL_CALL(glGetShaderiv(state.fshader, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			GLsizei len = 0;
			GL_CALL(glGetShaderInfoLog(state.fshader, 0, &len, NULL));
			GLchar * msg = malloc(len + 1);
			if (msg == NULL) {
				goto ffallback_compilation;
			}
			msg[len] = '\0';
			GL_CALL(glGetShaderInfoLog(state.fshader, len, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided fragment shader compilation error: \"%s\"", msg);
			free(msg);

		ffallback_compilation:
			fshader = (GLchar *) fallback_fshader;
			GL_CALL(glShaderSource(state.fshader, 1, (const GLchar * const *) &fshader, NULL));
			GL_CALL(glCompileShader(state.fshader));
			GL_CALL(glGetShaderiv(state.fshader, GL_COMPILE_STATUS, &success));
			if (success == GL_FALSE) {
				len = 0;
				GL_CALL(glGetShaderInfoLog(state.fshader, 0, &len, NULL));
				msg = malloc(len + 1);
				if (msg == NULL) {
					return 2;
				}
				msg[len] = '\0';
				GL_CALL(glGetShaderInfoLog(state.fshader, len, NULL, msg));
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback fragment shader compilation error: \"%s\"", msg);
				free(msg);
				return 2;
			}
		}
	}

	state.program = GL_CALL(glCreateProgram());
	GL_CALL(glAttachShader(state.program, state.vshader));
	GL_CALL(glAttachShader(state.program, state.fshader));
	GL_CALL(glLinkProgram(state.program));
	GL_CALL(glUseProgram(state.program));

	GL_CALL(glGenVertexArrays(1, &state.vao));
	GL_CALL(glGenBuffers(1, &state.vbo));
	GL_CALL(glGenBuffers(1, &state.ibo));

	if (mesh != NULL) {
		//glGenBuffers(1, &state.static_vbo);
		//glGenBuffers(1, &state.static_ibo);
	}

	GL_CALL(glBindVertexArray(state.vao));
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, state.vbo));
	GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * state.vbo_size, NULL, GL_DYNAMIC_DRAW));
	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.ibo));
	GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * state.ibo_size, NULL, GL_DYNAMIC_DRAW));

	if (mesh != NULL) {
		mesh_node_t * node = (mesh_node_t *) kgfw_graphics_mesh_new(mesh, NULL);
		memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
		memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
		memcpy(node->transform.scale, mesh->scale, sizeof(vec3));
		node->transform.absolute = 1;

		state.mesh_root = node;
		/*glBindBuffer(GL_ARRAY_BUFFER, state.static_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * state.static_vbo_size, mesh->vertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.static_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * state.static_ibo_size, mesh->indices, GL_STATIC_DRAW);*/
	}

	GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, x)));
	GL_CALL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, r)));
	GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, nx)));
	GL_CALL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, u)));
	GL_CALL(glEnableVertexAttribArray(0));
	GL_CALL(glEnableVertexAttribArray(1));
	GL_CALL(glEnableVertexAttribArray(2));
	GL_CALL(glEnableVertexAttribArray(3));

	state.unif_mvp = GL_CALL(glGetUniformLocation(state.program, "unif_mvp"));

	GL_CALL(glEnable(GL_DEPTH_TEST));
	/*glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);*/

	return 0;
}

void kgfw_graphics_draw(void) {
	GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GL_CALL(glClearColor(0.15f, 0.1f, 0.175f, 1.0f));

	mat4x4 mvp;
	mat4x4 vp;
	mat4x4 m;
	mat4x4 v;
	mat4x4 p;

	mat4x4_identity(mvp);
	mat4x4_identity(recurse_state.vp);
	mat4x4_identity(m);
	mat4x4_identity(v);
	mat4x4_identity(p);

	kgfw_camera_view(state.camera, v);
	kgfw_camera_perspective(state.camera, p);

	mat4x4_mul(recurse_state.vp, p, v);
	//mat4x4_mul(mvp, vp, m);

	/*if (state.ibo_size > 0) {
		glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.ibo);
		glDrawElements(GL_TRIANGLES, state.ibo_size, GL_UNSIGNED_INT, NULL);
	}

	if (state.static_ibo_size > 0) {
		glUniformMatrix4fv(state.unif_mvp, 1, GL_FALSE, &mvp[0][0]);
		
		glBindBuffer(GL_ARRAY_BUFFER, state.static_vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.static_ibo);
		glDrawElements(GL_TRIANGLES, state.static_ibo_size, GL_UNSIGNED_INT, NULL);
	}*/

	//mat4x4_identity(state.static_model);
	
	if (state.mesh_root != NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "node = {\n  transform = {\n    pos = {\n      %f, %f, %f\n    }\n    rot = {\n      %f, %f, %f\n    }\n    scale = {\n      %f, %f, %f\n    }\n    absolute = %u\n  }\n\n  parent = %p\n  child = %p\n  sibling = %p\n\n  gl = {\n    vbo = %u\n    ibo = %u\n    program = %u\n    vbo_size = %llu\n    ibo_size = %llu\n  }\n}", state.mesh_root->transform.pos[0], state.mesh_root->transform.pos[1], state.mesh_root->transform.pos[2], state.mesh_root->transform.rot[0], state.mesh_root->transform.rot[1], state.mesh_root->transform.rot[2], state.mesh_root->transform.scale[0], state.mesh_root->transform.scale[1], state.mesh_root->transform.scale[2], state.mesh_root->transform.absolute, state.mesh_root->parent, state.mesh_root->child, state.mesh_root->sibling, state.mesh_root->gl.vbo, state.mesh_root->gl.ibo, state.mesh_root->gl.program, state.mesh_root->gl.vbo_size, state.mesh_root->gl.ibo_size);
		meshes_draw_recursive_fchild(state.mesh_root);
	}
}

kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent) {
	//node->parent = (mesh_node_t *) parent;
	/*memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
	memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
	memcpy(node->transform.scale, mesh->scale, sizeof(vec3));*/
	//node->transform.absolute = 0;
	mesh_node_t * node = meshes_new();
	node->gl.vbo_size = mesh->vertices_count;
	node->gl.ibo_size = mesh->indices_count;
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, node->gl.vbo));
	GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * mesh->vertices_count, mesh->vertices, GL_STATIC_DRAW));
	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, node->gl.ibo));
	GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->indices_count, mesh->indices, GL_STATIC_DRAW));
	/*if (parent == NULL) {
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
	}*/
	
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
		//mat4x4_translate(out_m, mesh->transform.pos[0], mesh->transform.pos[1], -mesh->transform.pos[2]);
	} else {
		//mat4x4_translate_in_place(out_m, mesh->transform.pos[0], mesh->transform.pos[1], -mesh->transform.pos[2]);
	}
	mat4x4_rotate_X(out_m, out_m, mesh->transform.rot[0] * 3.141592f / 180.0f);
	mat4x4_rotate_Y(out_m, out_m, mesh->transform.rot[1] * 3.141592f / 180.0f);
	mat4x4_rotate_Z(out_m, out_m, mesh->transform.rot[2] * 3.141592f / 180.0f);
	//mat4x4_scale_aniso(out_m, out_m, mesh->transform.scale[0], mesh->transform.scale[1], mesh->transform.scale[2]);
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
	mesh_transform(mesh, out_m);
	mat4x4_mul(mvp, recurse_state.vp, out_m);

	GL_CALL(glUniformMatrix4fv(uniform, 1, GL_FALSE, &mvp[0][0]));
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

	mat4x4 matrix;
	memcpy(matrix, recurse_state.model, sizeof(mat4x4));
	
	meshes_draw_recursive(mesh);
	for (mesh_node_t * m = mesh;;) {
		m = m->sibling;
		if (m == NULL) {
			break;
		}

		memcpy(recurse_state.model, matrix, sizeof(mat4x4));
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
	const char * subcommands = "set    enable    disable";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}
	
	const char * options = "vsync";
	const char * arguments = "[option]    see 'gfx options'";
	if (strcmp("enable", argv[1]) == 0) {
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
		}
	} else if (strcmp("disable", argv[1]) == 0) {
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
		}
	} else if (strcmp("options", argv[1]) == 0) {
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
