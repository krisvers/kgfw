#include "kgfw_defines.h"

#if (KGFW_OPENGL == 33 || defined(KGFW_VULKAN) || KGFW_DIRECTX == 11)

#include "kgfw.h"
#include <GLFW/glfw3.h>
#include <time.h>
#include <stdlib.h>

static void glfw_error(int error, const char * desc);

int kgfw_init(void) {
	glfwSetErrorCallback(glfw_error);

	if (glfwInit() != GLFW_TRUE) {
		return 1;
	}
	glfwSetTime(0);

	srand(time(NULL));

	return 0;
}

void kgfw_deinit(void) {
	glfwTerminate();
}

int kgfw_update(void) {
	glfwPollEvents();

	return 0;
}

static void glfw_error(int error, const char * desc) {
	kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "[GLFW] %i 0x%X: %s", error, error, desc);
}
#endif