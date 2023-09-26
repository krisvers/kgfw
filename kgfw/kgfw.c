#include "kgfw.h"
#include <GLFW/glfw3.h>

int kgfw_init(void) {
	if (glfwInit() != GLFW_TRUE) {
		return 1;
	}

	return 0;
}

void kgfw_deinit(void) {
	glfwTerminate();
}

int kgfw_update(void) {
	glfwPollEvents();

	return 0;
}