#include "kgfw_console.h"
#include "kgfw_input.h"
#include "kgfw_log.h"
#include <stdlib.h>

static void console_key_callback(kgfw_input_key_enum key, unsigned char action);

int kgfw_console_init(void) {
	if (kgfw_input_key_register_callback(console_key_callback) != 0) {
		return 1;
	}

	return 0;
}

int kgfw_console_run(int argc, char ** argv) {
	for (int i = 0; i < argc; ++i) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%llu %p %s %c", i, argv[i], argv[i], *argv[i]);
	}
	return 0;
}

int kgfw_console_register_command(char * name, kgfw_console_command_callback command) {
	return 0;
}

struct {
	char buffer[256];
	unsigned int length;
} state;

char key_to_char[KGFW_KEY_MAX] = {
	[KGFW_KEY_SPACE] = ' ',
	[KGFW_KEY_APOSTROPHE] = '\'',
	[KGFW_KEY_COMMA] = ',',
	[KGFW_KEY_MINUS] = '-',
	[KGFW_KEY_PERIOD] = '.',
	[KGFW_KEY_SLASH] = '/',
	[KGFW_KEY_0] = '0',
	[KGFW_KEY_1] = '1',
	[KGFW_KEY_2] = '2',
	[KGFW_KEY_3] = '3',
	[KGFW_KEY_4] = '4',
	[KGFW_KEY_5] = '5',
	[KGFW_KEY_6] = '6',
	[KGFW_KEY_7] = '7',
	[KGFW_KEY_8] = '8',
	[KGFW_KEY_9] = '9',
	[KGFW_KEY_SEMICOLON] = ';',
	[KGFW_KEY_EQUAL] = '=',
	[KGFW_KEY_A] = 'A',
	[KGFW_KEY_B] = 'B',
	[KGFW_KEY_C] = 'C',
	[KGFW_KEY_D] = 'D',
	[KGFW_KEY_E] = 'E',
	[KGFW_KEY_F] = 'F',
	[KGFW_KEY_G] = 'G',
	[KGFW_KEY_H] = 'H',
	[KGFW_KEY_I] = 'I',
	[KGFW_KEY_J] = 'J',
	[KGFW_KEY_K] = 'K',
	[KGFW_KEY_L] = 'L',
	[KGFW_KEY_M] = 'M',
	[KGFW_KEY_N] = 'N',
	[KGFW_KEY_O] = 'O',
	[KGFW_KEY_P] = 'P',
	[KGFW_KEY_Q] = 'Q',
	[KGFW_KEY_R] = 'R',
	[KGFW_KEY_S] = 'S',
	[KGFW_KEY_T] = 'T',
	[KGFW_KEY_U] = 'U',
	[KGFW_KEY_V] = 'V',
	[KGFW_KEY_W] = 'W',
	[KGFW_KEY_X] = 'X',
	[KGFW_KEY_Y] = 'Y',
	[KGFW_KEY_Z] = 'Z',
	[KGFW_KEY_LBRACKET] = '[',
	[KGFW_KEY_BACKSLASH] = '\\',
	[KGFW_KEY_RBRACKET] = ']',
	[KGFW_KEY_GRAVE] = '`',
	[KGFW_KEY_ENTER] = '\n',
	[KGFW_KEY_TAB] = '\t',
	[KGFW_KEY_BACKSPACE] = '\b',
	[KGFW_KEY_NUMPAD_0] = '0',
	[KGFW_KEY_NUMPAD_1] = '1',
	[KGFW_KEY_NUMPAD_2] = '2',
	[KGFW_KEY_NUMPAD_3] = '3',
	[KGFW_KEY_NUMPAD_4] = '4',
	[KGFW_KEY_NUMPAD_5] = '5',
	[KGFW_KEY_NUMPAD_6] = '6',
	[KGFW_KEY_NUMPAD_7] = '7',
	[KGFW_KEY_NUMPAD_8] = '8',
	[KGFW_KEY_NUMPAD_9] = '9',
	[KGFW_KEY_NUMPAD_DECIMAL] = '.',
	[KGFW_KEY_NUMPAD_DIVIDE] = '/',
	[KGFW_KEY_NUMPAD_MULTIPLY] = '*',
	[KGFW_KEY_NUMPAD_SUBTRACT] = '-',
	[KGFW_KEY_NUMPAD_ADD] = '+',
	[KGFW_KEY_NUMPAD_ENTER] = '\n',
	[KGFW_KEY_NUMPAD_EQUAL] = '=',
};

static void console_key_callback(kgfw_input_key_enum key, unsigned char action) {
	if (action == 0) {
		return;
	}

	char c = key_to_char[key];
	if (c == '\0') {
		return;
	}

	state.buffer[state.length++] = c;
	if (c == '\n') {
		unsigned long long int argc = 0;
		unsigned long long int last = 0;
		char ** argv = NULL;

		for (unsigned int i = 0; i < state.length; ++i) {
			if (state.buffer[i] == ' ') {
				state.buffer[i] = '\0';
				if (argv != NULL) {
					char ** p = realloc(argv, sizeof(char *) * (argc + 1));
					if (p == NULL) {
						kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "allocation error");
						free(argv);
						return;
					}
					argv = p;
				} else {
					argv = malloc(sizeof(char *) * argc);
					if (argv == NULL) {
						kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "allocation error");
						return;
					}
				}
				argv[argc++] = &state.buffer[last];
				last = i + 1;
			}
			if (state.buffer[i] == '\n') {
				argv[argc++] = &state.buffer[last];
				state.buffer[i] = '\0';
				break;
			}
		}
		c = '\0';
		state.length = 0;
		kgfw_console_run(argc, argv);
		free(argv);
	}
}