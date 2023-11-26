#include "kgfw_defines.h"

#if (KGFW_OPENGL == 33 || defined(KGFW_VULKAN) || KGFW_DIRECTX == 11)

#include "kgfw_time.h"
#include <GLFW/glfw3.h>

static float start = 0;
static float end = 0;

float kgfw_time_get(void) {
	return (float) glfwGetTime();
}

float kgfw_time_delta(void) {
	return end - start;
}

void kgfw_time_start(void) {
	start = kgfw_time_get();
}

void kgfw_time_end(void) {
	end = kgfw_time_get();
}

void kgfw_time_update(void) {
	return;
}

void kgfw_time_init(void) {
	return;
}
#endif