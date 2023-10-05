#include <stdio.h>
#include <math.h>
#include "kgfw/kgfw.h"
#ifndef KGFW_WINDOWS
#include <unistd.h>
#endif

struct {
	kgfw_window_t * window;
	unsigned char input;
} static state = {
	NULL, 1
};

int kgfw_log_handler(kgfw_log_severity_enum severity, char * string) {
	char * severity_strings[] = { "CONSOLE", "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
	printf("[%s] %s\n", severity_strings[severity % 6], string);

	return 0;
}

int kgfw_logc_handler(kgfw_log_severity_enum severity, char character) {
	putc(character, stdout);
	fflush(stdout);

	return 0;
}

void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action) {
	//kgfw_logf(KGFW_LOG_SEVERITY_INFO, "key pressed: ('%c') %i %u", key, key, action);
	if (key == KGFW_KEY_B && action == 1) {
		//kgfw_audio_play_sound("cantina", 0, 0, 0, 0.25, 1, 0, 0);
	}
	if (key == KGFW_KEY_GRAVE && action == 1) {
		unsigned char enabled = kgfw_console_is_input_enabled();
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "console input %sabled", !enabled ? "en" : "dis");
		kgfw_console_input_enable(!enabled);
	}
}

int main(int argc, char ** argv) {
	kgfw_log_register_callback(kgfw_log_handler);
	kgfw_logc_register_callback(kgfw_logc_handler);
	if (kgfw_init() != 0) {
		return 1;
	}

	#ifndef KGFW_WINDOWS
	if (argc > 1) {
		if (chdir(argv[1]) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to chdir to %s", argv[1]);
		}
	}
	#endif

	kgfw_window_t window;
	if (kgfw_window_create(&window, 800, 600, "hello, worl") != 0) {
		kgfw_deinit();
		return 2;
	}

	if (kgfw_audio_init() != 0) {
		kgfw_window_destroy(&window);
		kgfw_deinit();
		return 2;
	}

	int ret = kgfw_audio_load("assets/jeff.wav", "googus");
	if (ret != 0) {
		return ret;
	}

	if (kgfw_graphics_init(&window) != 0) {
		kgfw_audio_deinit();
		kgfw_window_destroy(&window);
		kgfw_deinit();
		return 3;
	}

	if (kgfw_console_init() != 0) {
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&window);
		kgfw_deinit();
		return 4;
	}

	if (kgfw_commands_init() != 0) {
		kgfw_console_deinit();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&window);
		kgfw_deinit();
		return 5;
	}

	{
		kgfw_graphics_vertex_t vertices[] = {
			{
				 1.0f,  1.0f, -1.0f,	1.0f, 0.0f, 0.0f,	-1.0f, -1.0f, -1.0f,	0, 0
			},
			{
				-1.0f, -1.0f, -1.0f,	0.0f, 1.0f, 0.0f,	-1.0f, -1.0f, -1.0f,	0, 0
			},
			{
				-1.0f,  1.0f, -1.0f,	0.0f, 0.0f, 1.0f,	-1.0f, -1.0f, -1.0f,	0, 0
			}
		};

		unsigned int indices[] = {
			0, 1, 2,
		};

		kgfw_graphics_mesh_t mesh = {
			vertices, sizeof(vertices) / sizeof(vertices[0]), indices, sizeof(indices) / sizeof(indices[0]),
		};

		kgfw_graphics_update(&mesh);
	}

	kgfw_input_key_register_callback(kgfw_key_handler);

	int time = 0;
	while (!window.closed) {
		++time;
		kgfw_graphics_draw();

		if (kgfw_window_update(&window) != 0) {
			break;
		}

		kgfw_ecs_update();

		if (kgfw_update() != 0) {
			break;
		}

		if (kgfw_input_key(KGFW_KEY_ESCAPE)) {
			break;
		}
		
		if (kgfw_input_key(KGFW_KEY_A)) {
			int ret = 0;//kgfw_audio_play_sound("jeff", cos((double) time) * (time % 10), sin((double) time) * (time % 10), 5, 1, 1, 0, 1);
			if (ret != 0) {
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "play sound failed %u", ret);
			}
		}

		kgfw_input_update();
		kgfw_audio_update();
	}

	kgfw_console_deinit();
	kgfw_ecs_cleanup();
	kgfw_graphics_deinit();
	kgfw_audio_deinit();
	kgfw_window_destroy(&window);
	kgfw_deinit();

	return 0;
}
