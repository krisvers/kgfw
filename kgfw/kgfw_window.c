#include "kgfw.h"
#include <GLFW/glfw3.h>

static void kgfw_glfw_window_close(GLFWwindow * glfw_window);
static void kgfw_glfw_window_resize(GLFWwindow * glfw_window, int width, int height);

int kgfw_window_create(kgfw_window_t * out_window, unsigned int width, unsigned int height, char * title) {
	out_window->width = width;
	out_window->height = height;
	out_window->closed = 0;
	out_window->internal = NULL;

	out_window->internal = glfwCreateWindow(width, height, title, NULL, NULL);
	if (out_window->internal == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "GLFW window creation failed");
		return 1;
	}
	glfwSetWindowUserPointer(out_window->internal, (void *) out_window);
	glfwSetWindowCloseCallback(out_window->internal, kgfw_glfw_window_close);
	glfwSetWindowSizeCallback(out_window->internal, kgfw_glfw_window_resize);

	return 0;
}

void kgfw_window_destroy(kgfw_window_t * window) {
	glfwDestroyWindow(window->internal);
}

int kgfw_window_update(kgfw_window_t * window) {
	glfwSwapBuffers(window->internal);

	return 0;
}

static void kgfw_glfw_window_close(GLFWwindow * glfw_window) {
	kgfw_window_t * window = glfwGetWindowUserPointer(glfw_window);
	if (window == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_WARN, "GLFW window has invalid user pointer");
		return;
	}

	window->closed = 1;
}

static void kgfw_glfw_window_resize(GLFWwindow * glfw_window, int width, int height) {
	kgfw_window_t * window = glfwGetWindowUserPointer(glfw_window);
	if (window == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_WARN, "GLFW window has invalid user pointer");
		return;
	}

	window->width = (unsigned int) width;
	window->height = (unsigned int) height;
}