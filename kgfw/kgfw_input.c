#include "kgfw_defines.h"
#include "kgfw_input.h"
#include "kgfw_log.h"

#ifdef KGFW_WINDOWS

#include <windows.h>
#include <xinput.h>

#endif

#if (KGFW_OPENGL == 33 || defined(KGFW_VULKAN))
#include <string.h>
#include <GLFW/glfw3.h>

#define KGFW_KEY_MAX_CALLBACKS 16
#define KGFW_MOUSE_BUTTON_MAX_CALLBACKS 16

struct {
	unsigned char keys[KGFW_KEY_MAX];
	unsigned char prev_keys[KGFW_KEY_MAX];
	unsigned char mouse[KGFW_MOUSE_BUTTON_MAX];
	kgfw_input_key_callback callbacks[KGFW_KEY_MAX_CALLBACKS];
	kgfw_input_mouse_button_callback mouse_callbacks[KGFW_MOUSE_BUTTON_MAX_CALLBACKS];
	unsigned long long int callback_count;
	unsigned long long int mouse_callback_count;
	float mouse_x;
	float mouse_y;
	float scroll_x;
	float scroll_y;
	float prev_mouse_x;
	float prev_mouse_y;

	kgfw_gamepad_t gamepads[4];
} static key_state;

static kgfw_input_key_enum glfw_key_to_kgfw(int key);
static void kgfw_glfw_key(GLFWwindow * window, int key, int scancode, int mods, int action);
static void kgfw_glfw_mouse(GLFWwindow * window, double x, double y);
static void kgfw_glfw_scroll(GLFWwindow * window, double x, double y);
static void kgfw_glfw_mouse_buttons(GLFWwindow * window, int button, int action, int mods);

int kgfw_input_register_window(kgfw_window_t * window) {
	if (window == NULL) {
		return 1;
	}
	if (window->internal == NULL) {
		return 2;
	}

	glfwSetKeyCallback(window->internal, kgfw_glfw_key);
	glfwSetCursorPosCallback(window->internal, kgfw_glfw_mouse);
	glfwSetMouseButtonCallback(window->internal, kgfw_glfw_mouse_buttons);
	glfwSetScrollCallback(window->internal, kgfw_glfw_scroll);
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

unsigned char kgfw_input_mouse_button(kgfw_input_mouse_button_enum button) {
	return (key_state.mouse[button]);
}

void kgfw_input_update(void) {
	key_state.prev_mouse_x = key_state.mouse_x;
	key_state.prev_mouse_y = key_state.mouse_y;
	key_state.scroll_x = 0;
	key_state.scroll_y = 0;
	return;
}

static void kgfw_glfw_key(GLFWwindow * window, int key, int scancode, int action, int mods) {
	key_state.prev_keys[glfw_key_to_kgfw(key) % KGFW_KEY_MAX] = key_state.keys[glfw_key_to_kgfw(key) % KGFW_KEY_MAX];
	key_state.keys[glfw_key_to_kgfw(key) % KGFW_KEY_MAX] = (action);
	for (unsigned long long int i = 0; i < key_state.callback_count; ++i) {
		key_state.callbacks[i](glfw_key_to_kgfw(key) % KGFW_KEY_MAX, (action));
	}
}

static void kgfw_glfw_mouse(GLFWwindow * window, double x, double y) {
	key_state.prev_mouse_x = key_state.mouse_x;
	key_state.prev_mouse_y = key_state.mouse_y;
	key_state.mouse_x = x;
	key_state.mouse_y = y;
}

static void kgfw_glfw_scroll(GLFWwindow * window, double x, double y) {
	key_state.scroll_x = x;
	key_state.scroll_y = y;
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

static void kgfw_glfw_mouse_buttons(GLFWwindow * window, int button, int action, int mods) {
	if (button >= KGFW_MOUSE_BUTTON_MAX) {
		return;
	}

	key_state.mouse[button] = (action);
	for (unsigned long long int i = 0; i < key_state.mouse_callback_count; ++i) {
		key_state.mouse_callbacks[i](button % KGFW_MOUSE_BUTTON_MAX, (action));
	}
}

void kgfw_input_mouse_delta(float * out_dx, float * out_dy) {
	*out_dx = key_state.prev_mouse_x - key_state.mouse_x;
	*out_dy = key_state.prev_mouse_y - key_state.mouse_y;
}

void kgfw_input_mouse_pos(float * out_dx, float * out_dy) {
	*out_dx = key_state.mouse_x;
	*out_dy = key_state.mouse_y;
}

void kgfw_input_mouse_scroll(float * out_dx, float * out_dy) {
	*out_dx = key_state.scroll_x;
	*out_dy = key_state.scroll_y;
}

int kgfw_input_key_register_callback(kgfw_input_key_callback callback) {
	if (callback == NULL) {
		return 1;
	}

	key_state.callbacks[key_state.callback_count++] = callback;
	return 0;
}

int kgfw_input_mouse_button_register_callback(kgfw_input_mouse_button_callback callback) {
	if (callback == NULL) {
		return 1;
	}

	key_state.mouse_callbacks[key_state.mouse_callback_count++] = callback;
	return 0;
}

#elif (KGFW_DIRECTX == 11)

#include <string.h>
#include <windows.h>

#define KGFW_KEY_MAX_CALLBACKS 16
#define KGFW_MOUSE_BUTTON_MAX_CALLBACKS 16

struct {
	unsigned char keys[KGFW_KEY_MAX];
	unsigned char prev_keys[KGFW_KEY_MAX];
	unsigned char mouse[KGFW_MOUSE_BUTTON_MAX];
	kgfw_input_key_callback callbacks[KGFW_KEY_MAX_CALLBACKS];
	kgfw_input_mouse_button_callback mouse_callbacks[KGFW_MOUSE_BUTTON_MAX_CALLBACKS];
	unsigned long long int callback_count;
	unsigned long long int mouse_callback_count;
	float mouse_x;
	float mouse_y;
	float scroll_x;
	float scroll_y;
	float prev_mouse_x;
	float prev_mouse_y;
} static key_state;

int kgfw_input_register_window(kgfw_window_t * window) {
	if (window == NULL) {
		return 1;
	}
	if (window->internal == NULL) {
		return 2;
	}

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

unsigned char kgfw_input_mouse_button(kgfw_input_mouse_button_enum button) {
	return (key_state.mouse[button]);
}

void kgfw_input_update(void) {
	key_state.prev_mouse_x = key_state.mouse_x;
	key_state.prev_mouse_y = key_state.mouse_y;
	key_state.scroll_x = 0;
	key_state.scroll_y = 0;
	return;
}

void kgfw_input_mouse_delta(float * out_dx, float * out_dy) {
	*out_dx = key_state.prev_mouse_x - key_state.mouse_x;
	*out_dy = key_state.prev_mouse_y - key_state.mouse_y;
}

void kgfw_input_mouse_pos(float * out_dx, float * out_dy) {
	*out_dx = key_state.mouse_x;
	*out_dy = key_state.mouse_y;
}

void kgfw_input_mouse_scroll(float * out_dx, float * out_dy) {
	*out_dx = key_state.scroll_x;
	*out_dy = key_state.scroll_y;
}

int kgfw_input_key_register_callback(kgfw_input_key_callback callback) {
	if (callback == NULL) {
		return 1;
	}

	key_state.callbacks[key_state.callback_count++] = callback;
	return 0;
}

int kgfw_input_mouse_button_register_callback(kgfw_input_mouse_button_callback callback) {
	if (callback == NULL) {
		return 1;
	}

	key_state.mouse_callbacks[key_state.mouse_callback_count++] = callback;
	return 0;
}

void kgfw_input_press_key_down(kgfw_input_key_enum key) {
	key_state.prev_keys[key % KGFW_KEY_MAX] = key_state.keys[key % KGFW_KEY_MAX];
	key_state.keys[key % KGFW_KEY_MAX] = 1;
	for (unsigned long long int i = 0; i < key_state.callback_count; ++i) {
		key_state.callbacks[i](key % KGFW_KEY_MAX, 1);
	}
}
void kgfw_input_press_key_up(kgfw_input_key_enum key) {
	key_state.prev_keys[key % KGFW_KEY_MAX] = key_state.keys[key % KGFW_KEY_MAX];
	key_state.keys[key % KGFW_KEY_MAX] = 0;
	for (unsigned long long int i = 0; i < key_state.callback_count; ++i) {
		key_state.callbacks[i](key % KGFW_KEY_MAX, 0);
	}
}

void kgfw_input_press_mouse_button_down(kgfw_input_mouse_button_enum button) {
	if (button >= KGFW_MOUSE_BUTTON_MAX) {
		return;
	}

	key_state.mouse[button % KGFW_MOUSE_BUTTON_MAX] = 1;
	for (unsigned long long int i = 0; i < key_state.mouse_callback_count; ++i) {
		key_state.mouse_callbacks[i](button % KGFW_MOUSE_BUTTON_MAX, 1);
	}
}

void kgfw_input_press_mouse_button_up(kgfw_input_mouse_button_enum button) {
	if (button >= KGFW_MOUSE_BUTTON_MAX) {
		return;
	}

	key_state.mouse[button % KGFW_MOUSE_BUTTON_MAX] = 0;
	for (unsigned long long int i = 0; i < key_state.mouse_callback_count; ++i) {
		key_state.mouse_callbacks[i](button % KGFW_MOUSE_BUTTON_MAX, 0);
	}
}

void kgfw_input_set_mouse_pos(float x, float y) {
	key_state.prev_mouse_x = key_state.mouse_x;
	key_state.prev_mouse_y = key_state.mouse_y;
	key_state.mouse_x = x;
	key_state.mouse_y = y;
}

void kgfw_input_set_mouse_scroll(float x, float y) {
	key_state.scroll_x = x;
	key_state.scroll_y = y;
}
#endif

#ifdef KGFW_WINDOWS

kgfw_gamepad_t * kgfw_input_gamepad_get(void) {
	return NULL;
}

#endif