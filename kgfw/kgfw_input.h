#ifndef KRISVERS_KGFW_INPUT_H
#define KRISVERS_KGFW_INPUT_H

#include "kgfw_defines.h"
#include "kgfw_window.h"

typedef enum kgfw_input_key_enum {
	KGFW_KEY_UNKNOWN = 0,

	KGFW_KEY_SPACE = 32,

	KGFW_KEY_APOSTROPHE = 39,

	KGFW_KEY_COMMA = 44,
	KGFW_KEY_MINUS,
	KGFW_KEY_PERIOD,
	KGFW_KEY_SLASH,
	KGFW_KEY_0,
	KGFW_KEY_1,
	KGFW_KEY_2,
	KGFW_KEY_3,
	KGFW_KEY_4,
	KGFW_KEY_5,
	KGFW_KEY_6,
	KGFW_KEY_7,
	KGFW_KEY_8,
	KGFW_KEY_9,

	KGFW_KEY_SEMICOLON = 59,

	KGFW_KEY_EQUAL = 61,
	
	KGFW_KEY_A = 65,
	KGFW_KEY_B,
	KGFW_KEY_C,
	KGFW_KEY_D,
	KGFW_KEY_E,
	KGFW_KEY_F,
	KGFW_KEY_G,
	KGFW_KEY_H,
	KGFW_KEY_I,
	KGFW_KEY_J,
	KGFW_KEY_K,
	KGFW_KEY_L,
	KGFW_KEY_M,
	KGFW_KEY_N,
	KGFW_KEY_O,
	KGFW_KEY_P,
	KGFW_KEY_Q,
	KGFW_KEY_R,
	KGFW_KEY_S,
	KGFW_KEY_T,
	KGFW_KEY_U,
	KGFW_KEY_V,
	KGFW_KEY_W,
	KGFW_KEY_X,
	KGFW_KEY_Y,
	KGFW_KEY_Z,
	KGFW_KEY_LBRACKET,
	KGFW_KEY_BACKSLASH,
	KGFW_KEY_RBRACKET,

	KGFW_KEY_GRAVE = 96,

	KGFW_KEY_ESCAPE,
	KGFW_KEY_ENTER,
	KGFW_KEY_TAB,
	KGFW_KEY_BACKSPACE,
	KGFW_KEY_INSERT,
	KGFW_KEY_DELETE,
	KGFW_KEY_RIGHT,
	KGFW_KEY_LEFT,
	KGFW_KEY_DOWN,
	KGFW_KEY_UP,
	KGFW_KEY_PAGE_UP,
	KGFW_KEY_PAGE_DOWN,
	KGFW_KEY_HOME,
	KGFW_KEY_END,
	KGFW_KEY_CAPS_LOCK,
	KGFW_KEY_SCROLL_LOCK,
	KGFW_KEY_NUM_LOCK,
	KGFW_KEY_PRTSCR,
	KGFW_KEY_PAUSE,
	KGFW_KEY_F1,
	KGFW_KEY_F2,
	KGFW_KEY_F3,
	KGFW_KEY_F4,
	KGFW_KEY_F5,
	KGFW_KEY_F6,
	KGFW_KEY_F7,
	KGFW_KEY_F8,
	KGFW_KEY_F9,
	KGFW_KEY_F10,
	KGFW_KEY_F11,
	KGFW_KEY_F12,
	KGFW_KEY_F13,
	KGFW_KEY_F14,
	KGFW_KEY_F15,
	KGFW_KEY_F16,
	KGFW_KEY_F17,
	KGFW_KEY_F18,
	KGFW_KEY_F19,
	KGFW_KEY_F20,
	KGFW_KEY_F21,
	KGFW_KEY_F22,
	KGFW_KEY_F23,
	KGFW_KEY_F24,
	KGFW_KEY_NUMPAD_0,
	KGFW_KEY_NUMPAD_1,
	KGFW_KEY_NUMPAD_2,
	KGFW_KEY_NUMPAD_3,
	KGFW_KEY_NUMPAD_4,
	KGFW_KEY_NUMPAD_5,
	KGFW_KEY_NUMPAD_6,
	KGFW_KEY_NUMPAD_7,
	KGFW_KEY_NUMPAD_8,
	KGFW_KEY_NUMPAD_9,
	KGFW_KEY_NUMPAD_DECIMAL,
	KGFW_KEY_NUMPAD_DIVIDE,
	KGFW_KEY_NUMPAD_MULTIPLY,
	KGFW_KEY_NUMPAD_SUBTRACT,
	KGFW_KEY_NUMPAD_ADD,
	KGFW_KEY_NUMPAD_ENTER,
	KGFW_KEY_NUMPAD_EQUAL,
	KGFW_KEY_LSHIFT,
	KGFW_KEY_LCONTROL,
	KGFW_KEY_LALT,
	KGFW_KEY_LSUPER,
	KGFW_KEY_RSHIFT,
	KGFW_KEY_RCONTROL,
	KGFW_KEY_RALT,
	KGFW_KEY_RSUPER,
	KGFW_KEY_MAX,
} kgfw_input_key_enum;

/* takes key value and action value (0: immediate keyup, 1: immediate keydown, 2: repeated keydown, 3: repeated keyup) */
typedef void (*kgfw_input_key_callback)(kgfw_input_key_enum key, unsigned char action);

KGFW_PUBLIC void kgfw_input_update(void);
/* registers window input to redirect to main input */
KGFW_PUBLIC int kgfw_input_register_window(kgfw_window_t * window);
/* returns if key is currently pressed */
KGFW_PUBLIC unsigned char kgfw_input_key(kgfw_input_key_enum key);
/* returns if key was just pressed */
KGFW_PUBLIC unsigned char kgfw_input_key_down(kgfw_input_key_enum key);
/* returns if key was just depressed */
KGFW_PUBLIC unsigned char kgfw_input_key_up(kgfw_input_key_enum key);
/* register a callback for key input */
KGFW_PUBLIC int kgfw_input_key_register_callback(kgfw_input_key_callback callback);

#endif
