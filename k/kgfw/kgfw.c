#include "kgfw_defines.h"

#if (KGFW_OPENGL == 33 || defined(KGFW_VULKAN))

#include "kgfw.h"
#include <GLFW/glfw3.h>

static void glfw_error(int error, const char * desc);

int kgfw_init(void) {
	glfwSetErrorCallback(glfw_error);

	if (glfwInit() != GLFW_TRUE) {
		return 1;
	}
	glfwSetTime(0);

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

#elif (KGFW_DIRECTX == 11)

int kgfw_init(void) {
	return 0;
}

void kgfw_deinit(void) {
	return;
}

int kgfw_update(void) {
	return 0;
}
#endif