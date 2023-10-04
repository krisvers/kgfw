#include "kgfw_commands.h"
#include "kgfw_console.h"
#include "kgfw_audio.h"
#include "kgfw_log.h"
#include <string.h>

static int sound_command(int argc, char ** argv) {
	char * subcommands = "subcommands:    play";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", subcommands);
		return 0;
	}

	if (strcmp("play", argv[1]) == 0) {
		char * arguments = "arguments:    [sound name]";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%s", arguments);
			return 0;
		}

		if (kgfw_audio_play_sound(argv[2], 0, 0, 0, 1, 1, 0, 1) != 0) {
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

int kgfw_commands_init(void) {
	kgfw_console_register_command("sound", sound_command);
	kgfw_console_register_command("log", log_command);
	kgfw_console_register_command("set", set_command);
	kgfw_console_register_command("get", get_command);
	kgfw_console_register_command("new", new_command);

	return 0;
}
