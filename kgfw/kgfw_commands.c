#include "kgfw_commands.h"
#include "kgfw_console.h"
#include "kgfw_audio.h"
#include "kgfw_log.h"
#include <string.h>
#include <stdlib.h>

static int sound_command(int argc, char ** argv) {
	char * subcommands = "subcommands:    play    load";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", subcommands);
		return 0;
	}

	if (strcmp("play", argv[1]) == 0) {
		char * arguments = "arguments:    [sound name] (gain) (pitch) (x position) (y position) (z position) (is position relative)";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
			return 0;
		}

		float gain = 1;
		float pitch = 1;
		float x = 0;
		float y = 0;
		float z = 0;
		unsigned char relative = 1;
		switch (argc) {
			case 4:
				gain = strtod(argv[3], NULL);
				break;
			case 5:
				pitch = strtod(argv[4], NULL);
				break;
			case 6:
				x = strtod(argv[5], NULL);
				break;
			case 7:
				y = strtod(argv[6], NULL);
				break;
			case 8:
				z = strtod(argv[7], NULL);
				break;
			default:
				if (argc >= 9) {
					relative = (argv[8][0] == '1');
				}
				break;
		}

		if (kgfw_audio_play_sound(argv[2], x, y, z, gain, pitch, 0, relative) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "failed to play sound \"%s\"", argv[2]);
			return 0;
		}

		return 0;
	}

	if (strcmp("load", argv[1]) == 0) {
		char * arguments = "arguments:    [file name]    (optional sound name)";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
			return 0;
		}

		char * p = NULL;
		if (argc < 4) {
			p = argv[2];
		} else {
			p = argv[3];
		}
		int ret = kgfw_audio_load(argv[2], p);
		if (ret != 0) {
			if (ret == -1) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "there is already a sound by the name of \"%s\"", p);
			} else if (ret == 5) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "file does not exist \"%s\"", argv[2]);
			} else if (ret == 8 || ret == 10) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "file is not a valid pcm waveform \"%s\"", argv[2]);
			} else if (ret == 11) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "invalid bits per sample and stereo configuration \"%s\"", argv[2]);
			} else {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "failed to load audio");
			}
			return 0;
		}

		return 0;
	}

	kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", subcommands);
	return 0;
}

static int log_command(int argc, char ** argv) {
	char * subcommands = "subcommands:    console    trace    debug    info    warn    error";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", subcommands);
		return 0;
	}

	char * arguments = "arguments:    [string]";
	if (argc < 3) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
		return 0;
	}

	kgfw_log_severity_enum severity = -1;
	if (strcmp("console", argv[1]) == 0) {
		severity = KGFW_LOG_SEVERITY_CONSOLE;
	} else if (strcmp("trace", argv[1]) == 0) {
		severity = KGFW_LOG_SEVERITY_TRACE;
	} else if (strcmp("debug", argv[1]) == 0) {
		severity = KGFW_LOG_SEVERITY_DEBUG;
	} else if (strcmp("info", argv[1]) == 0) {
		severity = KGFW_LOG_SEVERITY_INFO;
	} else if (strcmp("warn", argv[1]) == 0) {
		severity = KGFW_LOG_SEVERITY_WARN;
	} else if (strcmp("error", argv[1]) == 0) {
		severity = KGFW_LOG_SEVERITY_ERROR;
	} else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", subcommands);
		return 0;
	}

	kgfw_logf(severity, "%s", argv[2]);
	return 0;
}

static int set_command(int argc, char ** argv) {
	char * arguments = "arguments:    [cvar name]    [value]";
	if (argc < 3) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
		return 0;
	}

	if (kgfw_console_set_var(argv[1], argv[2]) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no such cvar \"%s\"", argv[1]);
		return 0;
	}

	return 0;
}

static int get_command(int argc, char ** argv) {
	char * arguments = "arguments:    [cvar name]";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
		return 0;
	}

	char * cvar = kgfw_console_var(argv[1]);
	if (cvar == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no such cvar \"%s\"", argv[1]);
		return 0;
	}

	kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s = \"%s\"", argv[1], cvar);

	return 0;
}

static int new_command(int argc, char ** argv) {
	char * arguments = "arguments:    [cvar name]    (optional value)";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
		return 0;
	}

	char * cvar = kgfw_console_var(argv[1]);
	if (cvar != NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "cvar already exists \"%s\"", argv[1]);
		return 0;
	}

	char * p = argc < 3 ? NULL : argv[2];
	if (kgfw_console_create_var(argv[1], p) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "can't create new cvar \"%s\"", argv[1]);
		return 0;
	}

	return 0;
}

const char * command_names = "help    sound    log    set    get    new    exec";

static int help_command(int argc, char ** argv) {
	kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "commands:    %s", command_names);
	return 0;
}

static int test_command(int argc, char ** argv) {
	for (int i = 0; i < argc; ++i) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%i: \"%s\"", i, argv[i]);
	}
	return 0;
}

static int exec_command(int argc, char ** argv) {
	char * arguments = "arguments:    [cvar name]";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
		return 0;
	}

	char * cvar = kgfw_console_var(argv[1]);
	if (cvar == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no such cvar \"%s\"", argv[1]);
		return 0;
	}

	unsigned long long int len = strlen(cvar) + 1;
	unsigned long long int whitespaces = 0;

	for (unsigned long long int i = 0; i < len; ++i) {
		if (cvar[i] == ' ' || cvar[i] == '\0') {
			++whitespaces;
		}
	}

	char ** cargv = malloc(whitespaces * sizeof(char *));
	if (cargv == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "buffer allocation error");
		return 0;
	}

	int cargc = 0;
	unsigned long long int last = 0;

	for (unsigned long long int i = 0; i < len; ++i) {
		if (cvar[i] == '\"') {
			cvar[i] = cvar[i + 1];
			++i;
			while (cvar[i] != '\"') {
				cvar[i] = cvar[i + 1];
				++i;
				if (i >= len) {
					kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "console string literal never ended");
					return 0;
				}
			}
			cvar[i - 1] = '\0';
			continue;
		}

		if (cvar[i] == ' ' || cvar[i] == '\0') {
			cvar[i] = '\0';
			cargv[cargc++] = &cvar[last];
			last = i + 1;
		}

		if (cvar[i] == '\r') {
			cvar[i] = '\n';
		}
	}

	if (kgfw_console_run(cargc, cargv) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no command found \"%s\"", cargv[0]);
	}

	return 0;
}

int kgfw_commands_init(void) {
	kgfw_console_register_command("help", help_command);
	kgfw_console_register_command("sound", sound_command);
	kgfw_console_register_command("log", log_command);
	kgfw_console_register_command("set", set_command);
	kgfw_console_register_command("get", get_command);
	kgfw_console_register_command("new", new_command);
	kgfw_console_register_command("test", test_command);
	kgfw_console_register_command("exec", exec_command);

	return 0;
}
