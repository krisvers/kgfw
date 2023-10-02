#include <stdio.h>
#include <math.h>
#include "kgfw/kgfw.h"

int kgfw_log_handler(kgfw_log_severity_enum severity, char * string) {
	char * severity_strings[] = { "CONSOLE", "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
	printf("[%s] %s\n", severity_strings[severity % 6], string);

	return 0;
}

int kgfw_logc_handler(kgfw_log_severity_enum severity, char character) {
	putc(character, stdout);

	return 0;
}

void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action) {
	kgfw_logf(KGFW_LOG_SEVERITY_INFO, "key pressed: ('%c') %i %u", key, key, action);
	
}

int main(void) {
	kgfw_log_register_callback(kgfw_log_handler);
	if (kgfw_init() != 0) {
		return 1;
	}

	kgfw_window_t window;
	if (kgfw_window_create(&window, 800, 600, "hello, worl") != 0) {
		kgfw_deinit();
		return 2;
	}

	if (kgfw_audio_init() != 0) {
		kgfw_window_destroy(&window);
		kgfw_deinit();
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

	{
		kgfw_graphics_vertex_t vertices[] = {
			{  0,  1, 0,	1, 1, 0 },
			{ -1, -1, 0,	0, 1, 1 },
			{  1, -1, 0,	1, 0, 1 },
		};

		unsigned int indices[] = {
			1, 0, 2,
		};

		kgfw_graphics_mesh_t mesh = {
			vertices, 3, indices, 3,
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

		if (kgfw_input_key(KGFW_KEY_B)) {
			kgfw_audio_play_sound("cantina", 0, 0, 0, 1, 1, 0, 0);
		}
		
		if (kgfw_input_key(KGFW_KEY_A)) {
			int ret = kgfw_audio_play_sound("jeff", cos((double) time) * (time % 10), sin((double) time) * (time % 10), 5, 1, 1, 0, 1);
			if (ret != 0) {
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "play sound failed %u", ret);
			} else {
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "spawned the audio");
			}
		}

		kgfw_input_update();
		kgfw_audio_update();
	}

	kgfw_ecs_cleanup();
	kgfw_graphics_deinit();
	kgfw_audio_deinit();
	kgfw_window_destroy(&window);
	kgfw_deinit();

	return 0;
}
