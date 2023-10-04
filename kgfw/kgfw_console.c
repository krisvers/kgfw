#include "kgfw_console.h"
#include "kgfw_input.h"
#include "kgfw_log.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static void console_key_callback(kgfw_input_key_enum key, unsigned char action);
static unsigned long long int internal_hash(char * string);

#define COMMAND_NUM 512
#define VAR_NUM 512

kgfw_console_command_callback commands[COMMAND_NUM];
unsigned long long int command_hashes[COMMAND_NUM];
unsigned long long int commands_length = 0;

unsigned long long int console_var_hashes[VAR_NUM];
char * console_vars[VAR_NUM];
unsigned long long int console_vars_length = 0;

int kgfw_console_init(void) {
	if (kgfw_input_key_register_callback(console_key_callback) != 0) {
		return 1;
	}

	return 0;
}

void kgfw_console_deinit(void) {
	for (unsigned long long int i = 0; i < console_vars_length; ++i) {
		if (console_vars[i] != NULL) {
			free(console_vars[i]);
		}
	}
}

int kgfw_console_run(int argc, char ** argv) {
	if (argc < 1) {
		return 1;
	}
	unsigned long long int hash = internal_hash(argv[0]);
	for (unsigned long long int i = 0; i < commands_length; ++i) {
		if (command_hashes[i] == hash) {
			if (commands[i] != NULL) {
				commands[i](argc, argv);
				return 0;
			}
		}
	}

	return 2;
}

int kgfw_console_register_command(char * name, kgfw_console_command_callback command) {
	if (commands_length >= COMMAND_NUM) {
		return 1;
	}

	commands[commands_length++] = command;
	command_hashes[commands_length - 1] = internal_hash(name);

	return 0;
}

char * kgfw_console_var(char * name) {
	unsigned long long int hash = internal_hash(name);

	for (unsigned long long int i = 0; i < console_vars_length; ++i) {
		if (console_var_hashes[i] == hash) {
			return console_vars[i];
		}
	}

	return NULL;
}

int kgfw_console_create_var(char * name, char * value) {
	if (console_vars_length >= VAR_NUM) {
		return 1;
	}

	char * p = NULL;
	if (value == NULL) {
		p = malloc(1);
		if (p == NULL) {
			return 1;
		}
		p[0] = '\0';
	} else {
		unsigned long long int len = strlen(value);
		p = malloc(len + 1);
		if (p == NULL) {
			return 1;
		}

		memcpy(p, value, len);
		p[len] = '\0';
		for (unsigned long long int i = 0; i < len; ++i) {
			if (p[i] == '\'') {
				p[i] = '"';
			}
		}
	}

	console_vars[console_vars_length++] = p;

	unsigned long long int hash = internal_hash(name);
	console_var_hashes[console_vars_length - 1] = hash;

	return 0;
}

int kgfw_console_set_var(char * name, char * value) {
	unsigned long long int hash = internal_hash(name);

	for (unsigned long long int i = 0; i < console_vars_length; ++i) {
		if (console_var_hashes[i] == hash) {
			unsigned long long int len = strlen(value);
			char * p = NULL;
			if (console_vars[i] == NULL) {
				p = malloc(len + 1);
				if (p == NULL) {
					return 1;
				}
			} else {
				p = realloc(console_vars[i], len + 1);
				if (p == NULL) {
					return 1;
				}
			}
			console_vars[i] = p;

			memcpy(p, value, len);
			p[len] = '\0';
			for (unsigned long long int i = 0; i < len; ++i) {
				if (p[i] == '\'') {
					p[i] = '"';
				}
			}
			return 0;
		}
	}

	return 1;
}

#define BUFFER_SIZE 1024

struct {
	char buffer[BUFFER_SIZE];
	unsigned long long int length;
	unsigned char shift;
	unsigned long long int whitespaces;
	unsigned char enabled;
} state;

unsigned char kgfw_console_is_input_enabled(void) {
	return state.enabled;
}

void kgfw_console_input_enable(unsigned char enabled) {
	state.enabled = enabled;
	if (state.enabled) {
		kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, '>');
		kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, ' ');
	}
}

static char key_to_char[KGFW_KEY_MAX] = {
	[KGFW_KEY_SPACE] = ' ',
	[KGFW_KEY_DELETE] = '\1',
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
	[KGFW_KEY_A] = 'a',
	[KGFW_KEY_B] = 'b',
	[KGFW_KEY_C] = 'c',
	[KGFW_KEY_D] = 'd',
	[KGFW_KEY_E] = 'e',
	[KGFW_KEY_F] = 'f',
	[KGFW_KEY_G] = 'g',
	[KGFW_KEY_H] = 'h',
	[KGFW_KEY_I] = 'i',
	[KGFW_KEY_J] = 'j',
	[KGFW_KEY_K] = 'k',
	[KGFW_KEY_L] = 'l',
	[KGFW_KEY_M] = 'm',
	[KGFW_KEY_N] = 'n',
	[KGFW_KEY_O] = 'o',
	[KGFW_KEY_P] = 'p',
	[KGFW_KEY_Q] = 'q',
	[KGFW_KEY_R] = 'r',
	[KGFW_KEY_S] = 's',
	[KGFW_KEY_T] = 't',
	[KGFW_KEY_U] = 'u',
	[KGFW_KEY_V] = 'v',
	[KGFW_KEY_W] = 'w',
	[KGFW_KEY_X] = 'x',
	[KGFW_KEY_Y] = 'y',
	[KGFW_KEY_Z] = 'z',
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
	[KGFW_KEY_LSHIFT] = '\2',
	[KGFW_KEY_RSHIFT] = '\2',
};

static char toshift(char c) {
	if (isalpha(c)) {
		return toupper(c);
	}
	if (isalnum(c)) {
		switch (c) {
			case '1': return '!';
			case '2': return '@';
			case '3': return '#';
			case '4': return '$';
			case '5': return '%';
			case '6': return '^';
			case '7': return '&';
			case '8': return '*';
			case '9': return '(';
			case '0': return ')';
		}
	}

	switch (c) {
		case '`': return '~';
		case '-': return '_';
		case '=': return '+';
		case '[': return '{';
		case ']': return '}';
		case '\\': return '|';
		case ';': return ':';
		case '\'': return '"';
		case ',': return '<';
		case '.': return '>';
		case '/': return '?';
		case '\n': return '\r';
	}

	return c;
}

static void console_key_callback(kgfw_input_key_enum key, unsigned char action) {
	if (!state.enabled || key == KGFW_KEY_GRAVE) {
		return;
	}

	char c = key_to_char[key];
	if (c == '\2') {
		if (action == 0) {
			state.shift = 0;
		} else if (action == 1) {
			state.shift = 1;
		}
		return;
	}

	if (action == 0) {
		return;
	}

	if (c == '\0') {
		return;
	}

	c = state.shift ? toshift(c) : c;
	if (c == '\b') {
		if (state.length > 0) {
			--state.length;
			kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, '\b');
			kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, ' ');
			kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, '\b');
		}
		return;
	}
	kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, c);

	state.buffer[state.length++] = c;
	if (state.length >= BUFFER_SIZE) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "console buffer overrun");
		state.length = 0;
		state.whitespaces = 0;
		return;
	}

	if (c == ' ' || c == '\n') {
		++state.whitespaces;
	}

	if (c == '\n') {
		if (state.length == 1) {
			state.length = 0;
			state.whitespaces = 0;
			kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, '>');
			kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, ' ');
			return;
		}
		state.buffer[state.length - 1] = '\0';
		char ** argv = malloc(state.whitespaces * sizeof(char *));
		if (argv == NULL) {
			kgfw_log(KGFW_LOG_SEVERITY_ERROR, "buffer allocation error");
			state.length = 0;
			state.whitespaces = 0;
			return;
		}
		unsigned long long int argc = 0;
		unsigned long long int last = 0;

		for (unsigned long long int i = 0; i < state.length; ++i) {
			if (state.buffer[i] == '\"') {
				state.buffer[i] = state.buffer[i + 1];
				++i;
				while (state.buffer[i] != '\"') {
					state.buffer[i] = state.buffer[i + 1];
					++i;
					if (i >= state.length) {
						kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "console string literal never ended");
						state.length = 0;
						state.whitespaces = 0;
						return;
					}
				}
				state.buffer[i - 1] = '\0';

				continue;
			}
			if (state.buffer[i] == ' ' || state.buffer[i] == '\0') {
				state.buffer[i] = '\0';
				argv[argc++] = &state.buffer[last];
				last = i + 1;
			}
			if (state.buffer[i] == '\r') {
				state.buffer[i] = '\n';
			}
		}

		if (kgfw_console_run(argc, argv) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no command found \"%s\"", argv[0]);
		}
		state.length = 0;
		state.whitespaces = 0;
		kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, '>');
		kgfw_logc(KGFW_LOG_SEVERITY_CONSOLE, ' ');
	}
}

static unsigned long long int internal_hash(char * string) {
	unsigned long long int hash = 5381;

	for (unsigned long long int i = 0; string[i] != '\0'; ++i) {
		hash = (hash << 5) + hash + string[i];
	}

	return hash;
}
