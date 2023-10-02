#include "kgfw_graphics.h"
#include "kgfw_log.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef KGFW_APPLE
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_INDICES_COUNT 0

struct {
	kgfw_window_t * window;
	GLuint vbo;
	GLuint ibo;
	GLuint vao;
	GLuint vshader;
	GLuint fshader;
	GLuint program;

	unsigned long long int vbo_size;
	unsigned long long int ibo_size;
} static state;

int kgfw_graphics_init(kgfw_window_t * window) {
	state.window = window;
	state.vbo_size = KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT;
	state.ibo_size = KGFW_GRAPHICS_DEFAULT_INDICES_COUNT;

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

	const GLchar * vshader =
		"#version 330 core\n"
		"in vec3 in_pos; in vec3 in_color; out vec3 v_pos; out vec3 v_color; void main() { gl_Position = vec4(in_pos, 1.0); v_pos = in_pos; v_color = in_color; }";

	state.vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(state.vshader, 1, &vshader, NULL);
	glCompileShader(state.vshader);

	const GLchar * fshader =
		"#version 330 core\n"
		"in vec3 v_pos; in vec3 v_color; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

	state.fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(state.fshader, 1, &fshader, NULL);
	glCompileShader(state.fshader);

	state.program = glCreateProgram();
	glAttachShader(state.program, state.vshader);
	glAttachShader(state.program, state.fshader);
	glLinkProgram(state.program);
	glUseProgram(state.program);

	glGenVertexArrays(1, &state.vao);
	glGenBuffers(1, &state.vbo);
	glGenBuffers(1, &state.ibo);
	glBindVertexArray(state.vao);
	glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * state.vbo_size, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * state.ibo_size, NULL, GL_DYNAMIC_DRAW);

 	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, x));
 	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, r));
 	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, nx));
 	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, u));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	return 0;
}

void kgfw_graphics_draw(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.15f, 0.1f, 0.175f, 1.0f);
	glDrawElements(GL_TRIANGLES, state.ibo_size, GL_UNSIGNED_INT, NULL);
}

void kgfw_graphics_update(kgfw_graphics_mesh_t * mesh) {
	glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
	if (mesh->vertices_count > state.vbo_size) {
		state.vbo_size = mesh->vertices_count;
		glBufferData(GL_ARRAY_BUFFER, mesh->vertices_count * sizeof(kgfw_graphics_vertex_t), mesh->vertices, GL_DYNAMIC_DRAW);
	} else {
		glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->vertices_count * sizeof(kgfw_graphics_vertex_t), mesh->vertices);
	}
	glBindBuffer(GL_ARRAY_BUFFER, state.ibo);
	if (mesh->indices_count > state.ibo_size) {
		state.ibo_size = mesh->indices_count;
		glBufferData(GL_ARRAY_BUFFER, mesh->indices_count * sizeof(unsigned int), mesh->indices, GL_DYNAMIC_DRAW);
	} else {
		glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->indices_count * sizeof(unsigned int), mesh->indices);
	}
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
