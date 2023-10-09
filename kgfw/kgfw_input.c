#include "kgfw_input.h"
#include "kgfw_log.h"
#include <string.h>
#include <GLFW/glfw3.h>

#define KGFW_KEY_MAX_CALLBACKS 64

struct {
	unsigned char keys[KGFW_KEY_MAX];
	unsigned char prev_keys[KGFW_KEY_MAX];
	kgfw_input_key_callback callbacks[KGFW_KEY_MAX_CALLBACKS];
	unsigned long long int callback_count;
	float mouse_x;
	float mouse_y;
	float prev_mouse_x;
	float prev_mouse_y;
} static key_state;

static kgfw_input_key_enum glfw_key_to_kgfw(int key);
static void kgfw_glfw_key(GLFWwindow * window, int key, int scancode, int mods, int action);
static void kgfw_glfw_mouse(GLFWwindow * window, double x, double y);

int kgfw_input_register_window(kgfw_window_t * window) {
	if (window == NULL) {
		return 1;
	}
	if (window->internal == NULL) {
		return 2;
	}

	glfwSetKeyCallback(window->internal, kgfw_glfw_key);
	glfwSetCursorPosCallback(window->internal, kgfw_glfw_mouse);
	return 0;
}

unsigned char kgfw_input_key(kgfw_input_key_enum key) {
	return (key_state.keys[key]);
}

unsigned char kgfw_input_key_down(kgfw_input_key_enum key) {
	return (key_state.keys[key] && key_state.keys[key] != key_state.prev_keys[key]);
}

unsigned char kgfw_input_key_up(kgfw_input_key_enum key) {
	return (!key_state.keys[key] && key_state.keys[key] != key_state.prev_keys[key]);
}

void kgfw_input_update(void) {
	memcpy(key_state.prev_keys, key_state.keys, KGFW_KEY_MAX_CALLBACKS);
	key_state.prev_mouse_x = key_state.mouse_x;
	key_state.prev_mouse_y = key_state.mouse_y;
}

static void kgfw_glfw_key(GLFWwindow * window, int key, int scancode, int action, int mods) {
	key_state.keys[glfw_key_to_kgfw(key) % KGFW_KEY_MAX] = (action);
	for (unsigned long long int i = 0; i < key_state.callback_count; ++i) {
		key_state.callbacks[i](glfw_key_to_kgfw(key) % KGFW_KEY_MAX, (action));
	}
}

static void kgfw_glfw_mouse(GLFWwindow * window, double x, double y) {
	key_state.mouse_x = x;
	key_state.mouse_y = y;
}

static kgfw_input_key_enum glfw_key_to_kgfw(int key) {
	if (key >= 340) {
		return (key - 340) + KGFW_KEY_LSHIFT;
	}
	if (key >= 320) {
		return (key - 320) + KGFW_KEY_NUMPAD_0;
	}
	if (key >= 290) {
		return (key - 290) + KGFW_KEY_F1;
	}
	if (key >= 280) {
		return (key - 280) + KGFW_KEY_CAPS_LOCK;
	}
	if (key >= 256) {
		return (key - 256) + KGFW_KEY_ESCAPE;
	}

	return key;
}

void kgfw_input_mouse_delta(float * out_dx, float * out_dy) {
	*out_dx = key_state.prev_mouse_x - key_state.mouse_x;
	*out_dy = key_state.prev_mouse_y - key_state.mouse_y;
}

void kgfw_input_mouse_pos(float * out_dx, float * out_dy) {
	*out_dx = key_state.mouse_x;
	*out_dy = key_state.mouse_y;
}

int kgfw_input_key_register_callback(kgfw_input_key_callback callback) {
	if (callback == NULL) {
		return 1;
	}

	key_state.callbacks[key_state.callback_count] = callback;
	++key_state.callback_count;
	return 0;
}
