#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
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

typedef struct mesh_node {
	GLuint vbo;
	GLuint ibo;
	GLuint program;

	unsigned long long int vbo_size;
	unsigned long long int ibo_size;

	struct {
		float pos[3];
		float rot[3];
		float scale[3];
	} transform;

	struct mesh_node * child;
	struct mesh_node * sibling;
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
} static state = {
	NULL, NULL,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	{  },
	NULL,
	0, 0, 0, 0,
};

int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera, kgfw_graphics_mesh_t * mesh) {
	state.window = window;
	state.camera = camera;
	state.vbo_size = KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT;
	state.ibo_size = KGFW_GRAPHICS_DEFAULT_INDICES_COUNT;
	if (mesh != NULL) {
		state.static_vbo_size = mesh->vertices_count;
		state.static_ibo_size = mesh->indices_count;
	}

	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
		}
	}

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "Failed to load OpenGL 3.3 context");
		return 1;
	}

	if (window != NULL) {
		if (window->internal != NULL) {
			glViewport(0, 0, window->width, window->height);
		}
	}

	{
		const GLchar * fallback_vshader =
			"#version 330 core\n"
			"in vec3 in_pos; in vec3 in_color; uniform mat4 unif_mvp; out vec3 v_pos; out vec3 v_color; void main() { gl_Position = unif_mvp * vec4(in_pos, 1.0); v_pos = gl_Position.xyz; v_color = in_color; }";

		const GLchar * fallback_fshader =
			"#version 330 core\n"
			"in vec3 v_pos; in vec3 v_color; out vec4 out_color; void main() vec3 lawdkwkkd  { out_color = vec4(v_color, 1); }";

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

		state.vshader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(state.vshader, 1, (const GLchar * const *) &vshader, NULL);
		glCompileShader(state.vshader);

		GLint success = GL_TRUE;
		glGetShaderiv(state.vshader, GL_COMPILE_STATUS, &success);
		if (success == GL_FALSE) {
			GLsizei len = 0;
			glGetShaderInfoLog(state.vshader, 0, &len, NULL);
			GLchar * msg = malloc(len + 1);
			if (msg == NULL) {
				goto vfallback_compilation;
			}
			msg[len] = '\0';
			glGetShaderInfoLog(state.vshader, len, NULL, msg);
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided vertex shader compilation error: \"%s\"", msg);
			free(msg);

		vfallback_compilation:
			vshader = (GLchar *) fallback_vshader;
			glShaderSource(state.vshader, 1, (const GLchar * const *) &vshader, NULL);
			glCompileShader(state.vshader);
			glGetShaderiv(state.vshader, GL_COMPILE_STATUS, &success);
			if (success == GL_FALSE) {
				len = 0;
				glGetShaderInfoLog(state.vshader, 0, &len, NULL);
				msg = malloc(len + 1);
				if (msg == NULL) {
					return 2;
				}
				msg[len] = '\0';
				glGetShaderInfoLog(state.vshader, len, NULL, msg);
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback vertex shader compilation error: \"%s\"", msg);
				free(msg);
				return 2;
			}
		}

		state.fshader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(state.fshader, 1, (const GLchar * const *) &fshader, NULL);
		glCompileShader(state.fshader);
		
		glGetShaderiv(state.fshader, GL_COMPILE_STATUS, &success);
		if (success == GL_FALSE) {
			GLsizei len = 0;
			glGetShaderInfoLog(state.fshader, 0, &len, NULL);
			GLchar * msg = malloc(len + 1);
			if (msg == NULL) {
				goto ffallback_compilation;
			}
			msg[len] = '\0';
			glGetShaderInfoLog(state.fshader, len, NULL, msg);
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided fragment shader compilation error: \"%s\"", msg);
			free(msg);

		ffallback_compilation:
			fshader = (GLchar *) fallback_fshader;
			glShaderSource(state.fshader, 1, (const GLchar * const *) &fshader, NULL);
			glCompileShader(state.fshader);
			glGetShaderiv(state.fshader, GL_COMPILE_STATUS, &success);
			if (success == GL_FALSE) {
				len = 0;
				glGetShaderInfoLog(state.fshader, 0, &len, NULL);
				msg = malloc(len + 1);
				if (msg == NULL) {
					return 2;
				}
				msg[len] = '\0';
				glGetShaderInfoLog(state.fshader, len, NULL, msg);
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback fragment shader compilation error: \"%s\"", msg);
				free(msg);
				return 2;
			}
		}
	}

	state.program = glCreateProgram();
	glAttachShader(state.program, state.vshader);
	glAttachShader(state.program, state.fshader);
	glLinkProgram(state.program);
	glUseProgram(state.program);

	glGenVertexArrays(1, &state.vao);
	glGenBuffers(1, &state.vbo);
	glGenBuffers(1, &state.ibo);

	if (mesh != NULL) {
		glGenBuffers(1, &state.static_vbo);
		glGenBuffers(1, &state.static_ibo);
	}

	glBindVertexArray(state.vao);
	glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * state.vbo_size, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * state.ibo_size, NULL, GL_DYNAMIC_DRAW);

	if (mesh != NULL) {
		glBindBuffer(GL_ARRAY_BUFFER, state.static_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * state.static_vbo_size, mesh->vertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.static_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * state.static_ibo_size, mesh->indices, GL_STATIC_DRAW);

		mat4x4_identity(state.static_model);
		mat4x4_translate(state.static_model, mesh->pos[0], mesh->pos[1], -mesh->pos[2]);
		mat4x4_rotate_X(state.static_model, state.static_model, mesh->rot[0] * 3.141592f / 180.0f);
		mat4x4_rotate_Y(state.static_model, state.static_model, mesh->rot[1] * 3.141592f / 180.0f);
		mat4x4_rotate_Z(state.static_model, state.static_model, mesh->rot[2] * 3.141592f / 180.0f);
		mat4x4_scale_aniso(state.static_model, state.static_model, mesh->scale[0], mesh->scale[1], mesh->scale[2]);
	}

 	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, x));
 	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, r));
 	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, nx));
 	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, u));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	state.unif_mvp = glGetUniformLocation(state.program, "unif_mvp");

	glEnable(GL_DEPTH_TEST);
	/*glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);*/

	return 0;
}

void kgfw_graphics_draw(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.15f, 0.1f, 0.175f, 1.0f);

	mat4x4 mvp;
	mat4x4 vp;
	mat4x4 m;
	mat4x4 v;
	mat4x4 p;

	mat4x4_identity(mvp);
	mat4x4_identity(vp);
	mat4x4_identity(m);
	mat4x4_identity(v);
	mat4x4_identity(p);

	kgfw_camera_view(state.camera, v);
	kgfw_camera_perspective(state.camera, p);

	mat4x4_mul(vp, p, v);
	mat4x4_mul(mvp, vp, state.static_model);
	//mat4x4_mul(mvp, vp, m);

	glUniformMatrix4fv(state.unif_mvp, 1, GL_FALSE, &mvp[0][0]);

	if (state.ibo_size > 0) {
		glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.ibo);
		glDrawElements(GL_TRIANGLES, state.ibo_size, GL_UNSIGNED_INT, NULL);
	}

	if (state.static_ibo_size > 0) {
		glUniformMatrix4fv(state.unif_mvp, 1, GL_FALSE, &mvp[0][0]);
		
		glBindBuffer(GL_ARRAY_BUFFER, state.static_vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.static_ibo);
		glDrawElements(GL_TRIANGLES, state.static_ibo_size, GL_UNSIGNED_INT, NULL);
	}
}

void * kgfw_graphics_mesh_upload(kgfw_graphics_mesh_t * mesh) {


	return 0;
}

void kgfw_graphics_set_window(kgfw_window_t * window) {
	state.window = window;
	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
		}
		glViewport(0, 0, window->width, window->height);
	}
}

kgfw_window_t * kgfw_graphics_get_window(void) {
	return state.window;
}

void kgfw_graphics_deinit(void) {
	
}
