#include "kgfw_defines.h"
#include "kgfw_input.h"
#include "kgfw_log.h"

#define KGFW_KEY_MAX_CALLBACKS 16
#define KGFW_MOUSE_BUTTON_MAX_CALLBACKS 16
#define KGFW_GAMEPAD_MAX_CALLBACKS 16

struct {
	unsigned char keys[KGFW_KEY_MAX];
	unsigned char prev_keys[KGFW_KEY_MAX];
	unsigned char mouse[KGFW_MOUSE_BUTTON_MAX];
	kgfw_input_key_callback callbacks[KGFW_KEY_MAX_CALLBACKS];
	kgfw_input_mouse_button_callback mouse_callbacks[KGFW_MOUSE_BUTTON_MAX_CALLBACKS];
	kgfw_input_gamepad_callback gamepad_callbacks[KGFW_GAMEPAD_MAX_CALLBACKS];
	unsigned long long int callback_count;
	unsigned long long int mouse_callback_count;
	unsigned long long int gamepad_callback_count;
	float mouse_x;
	float mouse_y;
	float scroll_x;
	float scroll_y;
	float prev_mouse_x;
	float prev_mouse_y;

	unsigned char gamepad_enabled;
	kgfw_gamepad_t gamepads[KGFW_GAMEPAD_MAX];
} static key_state = {
	.gamepad_enabled = 1,
	.gamepads = {
		[0] = {
			.id = 0,
			.buttons = 0,
			.left_trigger = 0.0f,
			.right_trigger = 0.0f,
			.left_stick_x = 0.0f,
			.left_stick_y = 0.0f,
			.right_stick_x = 0.0f,
			.right_stick_y = 0.0f,
			.deadzone = {
				.lx = 0.1f,
				.ly = 0.1f,
				.rx = 0.1f,
				.ry = 0.1f,
			},
			.status = {
				.battery = 0.0f,
				.connected = 0,
			},
		},
		[1] = {
			.id = 1,
			.buttons = 0,
			.left_trigger = 0.0f,
			.right_trigger = 0.0f,
			.left_stick_x = 0.0f,
			.left_stick_y = 0.0f,
			.right_stick_x = 0.0f,
			.right_stick_y = 0.0f,
			.deadzone = {
				.lx = 0.1f,
				.ly = 0.1f,
				.rx = 0.1f,
				.ry = 0.1f,
			},
			.status = {
				.battery = 0.0f,
				.connected = 0,
			},
		},
		[2] = {
			.id = 2,
			.buttons = 0,
			.left_trigger = 0.0f,
			.right_trigger = 0.0f,
			.left_stick_x = 0.0f,
			.left_stick_y = 0.0f,
			.right_stick_x = 0.0f,
			.right_stick_y = 0.0f,
			.deadzone = {
				.lx = 0.1f,
				.ly = 0.1f,
				.rx = 0.1f,
				.ry = 0.1f,
			},
			.status = {
				.battery = 0.0f,
				.connected = 0,
			},
		},
		[3] = {
			.id = 3,
			.buttons = 0,
			.left_trigger = 0.0f,
			.right_trigger = 0.0f,
			.left_stick_x = 0.0f,
			.left_stick_y = 0.0f,
			.right_stick_x = 0.0f,
			.right_stick_y = 0.0f,
			.deadzone = {
				.lx = 0.1f,
				.ly = 0.1f,
				.rx = 0.1f,
				.ry = 0.1f,
			},
			.status = {
				.battery = 0.0f,
				.connected = 0,
			},
		},
	}
};

#if (KGFW_OPENGL == 33 || defined(KGFW_VULKAN))
#include <string.h>
#include <GLFW/glfw3.h>
#include <math.h>

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
	if (key_state.keys[key]) {
		return 1;
	}

	return 0;
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

	for (unsigned short i = 0; i < KGFW_GAMEPAD_MAX; ++i) {
		GLFWgamepadstate gstate;
		if (glfwJoystickIsGamepad(i) == GLFW_FALSE) {
			key_state.gamepads[i].status.connected = 0;
			key_state.gamepads[i].left_stick_x = 0;
			key_state.gamepads[i].left_stick_y = 0;
			key_state.gamepads[i].right_stick_x = 0;
			key_state.gamepads[i].right_stick_y = 0;

			key_state.gamepads[i].buttons = 0;
			key_state.gamepads[i].left_trigger = 0;
			key_state.gamepads[i].right_trigger = 0;
			key_state.gamepads[i].status.battery = 0;
			continue;
		}
		if (glfwGetGamepadState(i, &gstate)) {
			if (!key_state.gamepad_enabled) {
				key_state.gamepads[i].status.connected = 1;
				goto zero_gamepad_state;
			}

			float lx = gstate.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
			float ly = -gstate.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
			float rx = gstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
			float ry = -gstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];

			if (fabs(lx) < key_state.gamepads[i].deadzone.lx) {
				lx = 0;
			}
			if (fabs(ly) < key_state.gamepads[i].deadzone.ly) {
				ly = 0;
			}
			if (fabs(rx) < key_state.gamepads[i].deadzone.rx) {
				rx = 0;
			}
			if (fabs(ry) < key_state.gamepads[i].deadzone.ry) {
				ry = 0;
			}

			kgfw_gamepad_t gamepad;

			gamepad.left_stick_x = lx;
			gamepad.left_stick_y = ly;
			gamepad.right_stick_x = rx;
			gamepad.right_stick_y = ry;

			gamepad.id = i;
			gamepad.buttons = ((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]) << 0x01) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]) << 0x02) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]) << 0x03) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_START]) << 0x04) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_BACK]) << 0x05) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]) << 0x06) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB]) << 0x07) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]) << 0x08) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]) << 0x09) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_A]) << 0xC) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_B]) << 0xD) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_X]) << 0xE) | (((unsigned short) gstate.buttons[GLFW_GAMEPAD_BUTTON_Y]) << 0xF);
			gamepad.left_trigger = (gstate.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1) / 2.0f;
			gamepad.right_trigger = (gstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1) / 2.0f;
			gamepad.status.battery = 100;
			gamepad.status.connected = 1;
			memcpy(&gamepad.deadzone, &key_state.gamepads[i].deadzone, sizeof(gamepad.deadzone));

			if (memcmp(&gamepad, &key_state.gamepads[i], sizeof(kgfw_gamepad_t)) != 0) {
				key_state.gamepads[i] = gamepad;
				for (unsigned long long int j = 0; j < key_state.gamepad_callback_count; ++j) {
					key_state.gamepad_callbacks[j](&key_state.gamepads[i]);
				}
			}
		}
		else {
			key_state.gamepads[i].status.connected = 0;
			for (unsigned long long int j = 0; j < key_state.gamepad_callback_count; ++j) {
				key_state.gamepad_callbacks[j](&key_state.gamepads[i]);
			}

		zero_gamepad_state:
			key_state.gamepads[i].left_stick_x = 0;
			key_state.gamepads[i].left_stick_y = 0;
			key_state.gamepads[i].right_stick_x = 0;
			key_state.gamepads[i].right_stick_y = 0;

			key_state.gamepads[i].buttons = 0;
			key_state.gamepads[i].left_trigger = 0;
			key_state.gamepads[i].right_trigger = 0;
			key_state.gamepads[i].status.battery = 0;
		}
	}
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
#include <xinput.h>
#include <math.h>

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

	for (unsigned short i = 0; i < KGFW_GAMEPAD_MAX; ++i) {
		XINPUT_STATE xstate;
		XINPUT_BATTERY_INFORMATION bat_info;
		if (XInputGetState(i, &xstate) == ERROR_SUCCESS) {
			if (!key_state.gamepad_enabled) {
				key_state.gamepads[i].status.connected = 1;
				goto zero_gamepad_state;
			}

			XInputGetBatteryInformation(i, BATTERY_DEVTYPE_GAMEPAD, &bat_info);

			float lx = xstate.Gamepad.sThumbLX / 32767.0f;
			float ly = xstate.Gamepad.sThumbLY / 32767.0f;
			float rx = xstate.Gamepad.sThumbRX / 32767.0f;
			float ry = xstate.Gamepad.sThumbRY / 32767.0f;

			if (fabs(lx) < key_state.gamepads[i].deadzone.lx) {
				lx = 0;
			}
			if (fabs(ly) < key_state.gamepads[i].deadzone.ly) {
				ly = 0;
			}
			if (fabs(rx) < key_state.gamepads[i].deadzone.rx) {
				rx = 0;
			}
			if (fabs(ry) < key_state.gamepads[i].deadzone.ry) {
				ry = 0;
			}

			kgfw_gamepad_t gamepad;

			gamepad.left_stick_x = lx;
			gamepad.left_stick_y = ly;
			gamepad.right_stick_x = rx;
			gamepad.right_stick_y = ry;

			gamepad.id = i;
			gamepad.buttons = xstate.Gamepad.wButtons;
			gamepad.left_trigger = xstate.Gamepad.bLeftTrigger / 255.0f;
			gamepad.right_trigger = xstate.Gamepad.bRightTrigger / 255.0f;
			gamepad.status.battery = bat_info.BatteryLevel / 255.0f;
			gamepad.status.connected = 1;
			memcpy(&gamepad.deadzone, &key_state.gamepads[i].deadzone, sizeof(gamepad.deadzone));

			if (memcmp(&gamepad, &key_state.gamepads[i], sizeof(kgfw_gamepad_t)) != 0) {
				key_state.gamepads[i] = gamepad;
				for (unsigned long long int j = 0; j < key_state.gamepad_callback_count; ++j) {
					key_state.gamepad_callbacks[j](&key_state.gamepads[i]);
				}
			}
		} else {
			if (key_state.gamepads[i].status.connected) {
				key_state.gamepads[i].status.connected = 0;
				for (unsigned long long int j = 0; j < key_state.gamepad_callback_count; ++j) {
					key_state.gamepad_callbacks[j](&key_state.gamepads[i]);
				}
			}

		zero_gamepad_state:
			key_state.gamepads[i].left_stick_x = 0;
			key_state.gamepads[i].left_stick_y = 0;
			key_state.gamepads[i].right_stick_x = 0;
			key_state.gamepads[i].right_stick_y = 0;

			key_state.gamepads[i].id = i;
			key_state.gamepads[i].buttons = 0;
			key_state.gamepads[i].left_trigger = 0;
			key_state.gamepads[i].right_trigger = 0;
			key_state.gamepads[i].status.battery = 0;
		}
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

kgfw_gamepad_t * kgfw_input_gamepad_get(unsigned int gamepad_id) {
	if (gamepad_id >= KGFW_GAMEPAD_MAX + 1 || gamepad_id == 0) {
		for (unsigned short i = 0; i < KGFW_GAMEPAD_MAX; ++i) {
			if (key_state.gamepads[i].status.connected) {
				return &key_state.gamepads[i];
			}
		}
		return NULL;
	}

	return &key_state.gamepads[gamepad_id - 1];
}

unsigned char kgfw_input_gamepad_pressed(kgfw_gamepad_t * gamepad, unsigned short buttons) {
	if (gamepad->buttons & buttons) {
		return 1;
	}
	return 0;
}

unsigned char kgfw_input_gamepad_is_enabled(void) {
	return key_state.gamepad_enabled;
}

void kgfw_input_gamepad_enable(void) {
	key_state.gamepad_enabled = 1;
}

void kgfw_input_gamepad_disable(void) {
	key_state.gamepad_enabled = 0;
}

int kgfw_input_gamepad_register_callback(kgfw_input_gamepad_callback callback) {
	if (callback == NULL) {
		return 1;
	}

	key_state.gamepad_callbacks[key_state.gamepad_callback_count++] = callback;
	return 0;
}