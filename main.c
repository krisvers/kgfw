#include <stdio.h>
#include <math.h>
#include "kgfw/kgfw.h"
#ifndef KGFW_WINDOWS
#include <unistd.h>
#endif
#include <linmath.h>

struct {
	kgfw_window_t window;
	kgfw_camera_t camera;
	unsigned char input;
} static state = {
	(kgfw_window_t) {},
	(kgfw_camera_t) { { 0, 0, -5 }, { 0, 0, 0 }, 90, 0.01f, 1000.0f, 1.3333f, 1 },
	1
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
	if (key == KGFW_KEY_GRAVE && action == 1) {
		unsigned char enabled = kgfw_console_is_input_enabled();
		state.input = enabled;
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "console input %sabled", !enabled ? "en" : "dis");
		kgfw_console_input_enable(!enabled);
	}
	if (state.input) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "key pressed: ('%c') %i %u", key, key, action);
		if (key == KGFW_KEY_B && action == 1) {
			kgfw_audio_play_sound("cantina", 0, 0, 0, 0.25, 1, 0, 0);
		}
	}
}

typedef struct player_movement {
	KGFW_DEFAULT_COMPONENT_MEMBERS
} player_movement_t;

typedef struct player_movement_state {
	KGFW_DEFAULT_COMPONENT_STATE_MEMBERS
	kgfw_camera_t * camera;
} player_movement_state_t;

int player_movement_init(player_movement_t * self, player_movement_state_t * mstate) {
	return 0;
}

int player_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (!state.input) {
		return 0;
	}

	float move_speed = 0.5f;
	float move_slow_speed = 0.125f;
	float look_sensitivity = 3.0f;
	float look_slow_sensitivity = 1.0f;

	if (kgfw_input_key(
		#ifndef KGFW_APPLE_MACOS
		KGFW_KEY_LCONTROL
		#else
		KGFW_KEY_LALT
		#endif
	)) {
		move_speed = move_slow_speed;
		look_sensitivity = look_slow_sensitivity;
	}

	if (kgfw_input_key(KGFW_KEY_RIGHT)) {
		mstate->camera->rot[1] += look_sensitivity;
	}
	if (kgfw_input_key(KGFW_KEY_LEFT)) {
		mstate->camera->rot[1] -= look_sensitivity;
	}
	if (kgfw_input_key(KGFW_KEY_UP)) {
		mstate->camera->rot[0] += look_sensitivity;
	}
	if (kgfw_input_key(KGFW_KEY_DOWN)) {
		mstate->camera->rot[0] -= look_sensitivity;
	}
	
	vec3 up;
	vec3 right;
	vec3 forward;
	up[0] = 0; up[1] = 1; up[2] = 0;
	right[0] = 0; right[1] = 0; right[2] = 0;
	forward[0] = sinf(mstate->camera->rot[1] * 3.141592f / 180.0f); forward[1] = 0; forward[2] = cosf(mstate->camera->rot[1] * 3.141592f / 180.0f);
	vec3_mul_cross(right, up, forward);
	vec3_scale(up, up, move_speed);
	vec3_scale(right, right, move_speed);
	vec3_scale(forward, forward, move_speed);

	if (kgfw_input_key(KGFW_KEY_W)) {
		vec3_add(mstate->camera->pos, mstate->camera->pos, forward);
	}
	if (kgfw_input_key(KGFW_KEY_S)) {
		vec3_sub(mstate->camera->pos, mstate->camera->pos, forward);
	}
	if (kgfw_input_key(KGFW_KEY_D)) {
		vec3_add(mstate->camera->pos, mstate->camera->pos, right);
	}
	if (kgfw_input_key(KGFW_KEY_A)) {
		vec3_sub(mstate->camera->pos, mstate->camera->pos, right);
	}
	if (kgfw_input_key(KGFW_KEY_E)) {
		vec3_add(mstate->camera->pos, mstate->camera->pos, up);
	}
	if (kgfw_input_key(KGFW_KEY_Q)) {
		vec3_sub(mstate->camera->pos, mstate->camera->pos, up);
	}

	return 0;
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

	if (kgfw_window_create(&state.window, 800, 600, "hello, worl") != 0) {
		kgfw_deinit();
		return 2;
	}

	if (kgfw_audio_init() != 0) {
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 2;
	}

	{
		kgfw_graphics_vertex_t vertices[] = {
			{  1.0f,  1.0f, -1.0f,	1.0f, 0.0f, 0.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
			{ -1.0f, -1.0f, -1.0f,	1.0f, 1.0f, 0.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
			{ -1.0f,  1.0f, -1.0f,	1.0f, 0.0f, 1.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
			{  1.0f, -1.0f, -1.0f,	1.0f, 1.0f, 1.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },

			{  1.0f,  1.0f,  1.0f,	0.0f, 1.0f, 0.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
			{ -1.0f, -1.0f,  1.0f,	0.0f, 1.0f, 1.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
			{ -1.0f,  1.0f,  1.0f,	1.0f, 1.0f, 0.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
			{  1.0f, -1.0f,  1.0f,	1.0f, 1.0f, 1.0f,	-1.0f, -1.0f, -1.0f,	0, 0 },
		};

		unsigned int indices[] = {
			0, 1, 2,	/* close */
			3, 1, 0,

			4, 6, 5,	/* far */
			7, 4, 5,

			0, 4, 7,	/* right */
			3, 0, 7,

			2, 5, 6,	/* left */
			1, 5, 2,

			0, 2, 6,	/* top */
			0, 6, 4,

			7, 5, 1,	/* bottom */
			7, 1, 3,
		};

		kgfw_graphics_mesh_t mesh = {
			vertices, sizeof(vertices) / sizeof(vertices[0]), indices, sizeof(indices) / sizeof(indices[0]),
		};

		if (kgfw_graphics_init(&state.window, &state.camera, &mesh) != 0) {
			kgfw_audio_deinit();
			kgfw_window_destroy(&state.window);
			kgfw_deinit();
			return 3;
		}
	}

	if (kgfw_console_init() != 0) {
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	if (kgfw_commands_init() != 0) {
		kgfw_console_deinit();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 5;
	}

	kgfw_input_key_register_callback(kgfw_key_handler);
	player_movement_t * mov = NULL;
	{
		player_movement_t m = {
			sizeof(player_movement_t),
			(int (*)(void *, void *)) player_movement_init,
			(int (*)(void *, void *)) player_movement_update,
		};

		mov = kgfw_ecs_get(kgfw_ecs_register((kgfw_ecs_component_t *) &m, "player_movement"));
	}

	player_movement_state_t mov_state = {
		sizeof(player_movement_state_t),
		&state.camera,
	};

	mov->init(mov, &mov_state);

	while (!state.window.closed) {
		kgfw_graphics_draw();

		if (kgfw_window_update(&state.window) != 0) {
			break;
		}

		kgfw_ecs_update();

		if (kgfw_update() != 0) {
			break;
		}

		if (kgfw_input_key(KGFW_KEY_ESCAPE)) {
			break;
		}
		
		mov->update(mov, &mov_state);

		kgfw_input_update();
		kgfw_audio_update();
	}

	kgfw_console_deinit();
	kgfw_ecs_cleanup();
	kgfw_graphics_deinit();
	kgfw_audio_deinit();
	kgfw_window_destroy(&state.window);
	kgfw_deinit();

	return 0;
}
