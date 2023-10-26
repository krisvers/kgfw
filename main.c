#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "kgfw/kgfw.h"
#include "kgfw/ktga/ktga.h"
#include "kgfw/koml/koml.h"
#ifndef KGFW_WINDOWS
#include <unistd.h>
#endif
#include <linmath.h>

struct {
	kgfw_window_t window;
	kgfw_camera_t camera;
	unsigned char input;
	unsigned char exit;
} static state = {
	{ 0 },
	{ { 0, 0, 1 }, { 0, 0, 0 }, { 1, 1 }, 90, 0.01f, 1000.0f, 1.3333f, 1 },
	1, 0
};

#define STORAGE_MAX_MESHES 256
#define STORAGE_MAX_TEXTURES 16

struct {
	kgfw_graphics_mesh_node_t * meshes[STORAGE_MAX_MESHES];
	unsigned long long int meshes_count;
	ktga_t textures[STORAGE_MAX_TEXTURES];
	unsigned long long int textures_count;
	unsigned long long int texture_hashes[STORAGE_MAX_TEXTURES];
	kgfw_graphics_mesh_node_t ** current;
	kgfw_graphics_mesh_node_t * select_mesh;
} static storage = {
	{ 0 },
	0,
	{ 0 },
	0,
	{ 0 },
};

kgfw_graphics_vertex_t mesh_vertices[4] = {
	{ -1, -1, 0,	1, 1, 1,	0, 1, 0,	0, 0,	1, 0, 0,	0, 0, 1, },
	{  1, -1, 0,	1, 1, 1,	0, 1, 0,	1, 0,	1, 0, 0,	0, 0, 1, },
	{ -1,  1, 0,	1, 1, 1,	0, 1, 0,	0, 1,	1, 0, 0,	0, 0, 1, },
	{  1,  1, 0,	1, 1, 1,	0, 1, 0,	1, 1,	1, 0, 0,	0, 0, 1, },
};

unsigned int mesh_indices[6] = {
	0, 1, 2,
	1, 2, 3,
};

kgfw_graphics_mesh_t mesh = {
	mesh_vertices,
	4,
	mesh_indices,
	6,

	{ 0, 0, 0, },
	{ 0, 0, 0, },
	{ 1, 0.5f, 1, },
};

static int kgfw_log_handler(kgfw_log_severity_enum severity, char * string);
static int kgfw_logc_handler(kgfw_log_severity_enum severity, char character);
static void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action);
static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action);
static ktga_t * texture_get(char * name);
static kgfw_graphics_mesh_node_t ** click(void);

typedef struct player_movement {
	KGFW_DEFAULT_COMPONENT_MEMBERS
} player_movement_t;

typedef struct player_movement_state {
	KGFW_DEFAULT_COMPONENT_STATE_MEMBERS
	kgfw_camera_t * camera;
} player_movement_state_t;

static int player_movement_init(player_movement_t * self, player_movement_state_t * mstate);
static int player_ortho_movement_update(player_movement_t * self, player_movement_state_t * mstate);
static int player_movement_update(player_movement_t * self, player_movement_state_t * mstate);
static int exit_command(int argc, char ** argv);

int main(int argc, char ** argv) {
	kgfw_log_register_callback(kgfw_log_handler);
	kgfw_logc_register_callback(kgfw_logc_handler);
	if (kgfw_init() != 0) {
		return 1;
	}

	/* work-around for pylauncher bug */
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

	if (kgfw_graphics_init(&state.window, &state.camera) != 0) {
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 3;
	}

	{
		struct {
			void * buffer;
			unsigned long long int size;
		} file = {
			NULL, 0
		};
		{
			FILE * fp = fopen("./assets/config.koml", "rb");
			if (fp == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"config.koml\"");
				return 1;
			}

			fseek(fp, 0L, SEEK_END);
			file.size = ftell(fp);
			fseek(fp, 0L, SEEK_SET);

			file.buffer = malloc(file.size);
			if (file.buffer == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"config.koml\"");
				return 1;
			}

			if (fread(file.buffer, 1, file.size, fp) != file.size) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"config.koml\"");
				return 1;
			}

			fclose(fp);
		}

		koml_table_t ktable;
		if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"config.koml\"");
			return 1;
		}

		koml_symbol_t * files = koml_table_symbol(&ktable, "textures:files");
		koml_symbol_t * names = koml_table_symbol(&ktable, "textures:names");
		if (files == NULL) {
			goto skip_tga_load;
		}

		for (unsigned long long int i = 0; i < files->data.array.length % STORAGE_MAX_TEXTURES; ++i) {
			FILE * fp = fopen(files->data.array.elements.string[i], "rb");
			if (fp == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"%s\"", files->data.array.elements.string[i]);
				return 1;
			}

			fseek(fp, 0L, SEEK_END);
			unsigned long long int size = ftell(fp);
			fseek(fp, 0L, SEEK_SET);

			void * buffer = malloc(size);
			if (buffer == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"%s\"", files->data.array.elements.string[i]);
				return 1;
			}

			if (fread(buffer, 1, size, fp) != size) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"%s\" %llu", files->data.array.elements.string[i], size);
				return 1;
			}

			fclose(fp);
			ktga_load(&storage.textures[i], buffer, size);
			free(buffer);

			if (names == NULL) {
				unsigned long long int len = strlen(files->data.array.elements.string[i]);
				storage.texture_hashes[i] = kgfw_hash(files->data.array.elements.string[i]);
			} else {
				unsigned long long int len = strlen(names->data.array.elements.string[i]);
				storage.texture_hashes[i] = kgfw_hash(names->data.array.elements.string[i]);
			}
			++storage.textures_count;
		}

	skip_tga_load:;
	}

	if (kgfw_console_init() != 0) {
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	kgfw_console_register_command("exit", exit_command);
	kgfw_console_register_command("quit", exit_command);

	if (kgfw_commands_init() != 0) {
		kgfw_console_deinit();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 5;
	}

	kgfw_input_key_register_callback(kgfw_key_handler);
	kgfw_input_mouse_button_register_callback(kgfw_mouse_button_handle);
	player_movement_t * mov = NULL;
	{
		player_movement_t m = {
			sizeof(player_movement_t),
			(int (*)(void *, void *)) player_movement_init,
			(int (*)(void *, void *)) player_movement_update,
		};

		if (state.camera.ortho) {
			m.update = (int (*)(void *, void *)) player_ortho_movement_update;
		}

		mov = kgfw_ecs_get(kgfw_ecs_register((kgfw_ecs_component_t *) &m, "player_movement"));
	}

	player_movement_state_t mov_state = {
		sizeof(player_movement_state_t),
		&state.camera,
	};

	mov->init(mov, &mov_state);

	storage.select_mesh = kgfw_graphics_mesh_new(&mesh, NULL);
	{
		storage.select_mesh->transform.scale[0] = 0;
		ktga_t * tga = texture_get("selected");
		kgfw_graphics_texture_t tex = {
			tga->bitmap,
			tga->header.img_w, tga->header.img_h,
			KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
			KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
		};
		kgfw_graphics_mesh_texture(storage.select_mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
	}

	//kgfw_graphics_mesh_node_t * current = NULL;
	while (!state.window.closed && !state.exit) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "%p", click());
		kgfw_time_start();
		kgfw_graphics_draw();

		if (kgfw_window_update(&state.window) != 0) {
			state.exit = 1;
			break;
		}

		kgfw_ecs_update();

		{
			unsigned int w = state.window.width;
			unsigned int h = state.window.height;
			if (kgfw_update() != 0) {
				state.exit = 1;
				break;
			}
			if (w != state.window.width || h != state.window.height) {
				kgfw_graphics_viewport(state.window.width, state.window.height);
				state.camera.ratio = state.window.width / (float)state.window.height;
			}
		}

		kgfw_time_end();
		
		mov->update(mov, &mov_state);
		if (mov_state.camera->rot[0] >= 360 || mov_state.camera->rot[0] < 0) {
			mov_state.camera->rot[0] = fmod(mov_state.camera->rot[0], 360);
		}
		if (mov_state.camera->rot[1] >= 360 || mov_state.camera->rot[1] < 0) {
			mov_state.camera->rot[1] = fmod(mov_state.camera->rot[1], 360);
		}
		if (mov_state.camera->rot[2] >= 360 || mov_state.camera->rot[2] < 0) {
			mov_state.camera->rot[2] = fmod(mov_state.camera->rot[2], 360);
		}

		kgfw_input_update();
		kgfw_audio_update();
		kgfw_time_end();
		if (state.input) {
			//kgfw_logf(KGFW_LOG_SEVERITY_INFO, "frame time: %f    fps: %f", kgfw_time_delta(), 1 / kgfw_time_delta());
		}
	}

	kgfw_console_deinit();
	kgfw_ecs_cleanup();
	kgfw_graphics_deinit();
	kgfw_audio_deinit();
	kgfw_window_destroy(&state.window);
	kgfw_deinit();

	return 0;
}

static int kgfw_log_handler(kgfw_log_severity_enum severity, char * string) {
	char * severity_strings[] = { "CONSOLE", "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
	printf("[%s] %s\n", severity_strings[severity % 6], string);

	return 0;
}

static int kgfw_logc_handler(kgfw_log_severity_enum severity, char character) {
	putc(character, stdout);
	fflush(stdout);

	return 0;
}

static void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action) {
	if (key == KGFW_KEY_GRAVE && action == 1) {
		unsigned char enabled = kgfw_console_is_input_enabled();
		state.input = enabled;
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "console input %sabled", !enabled ? "en" : "dis");
		kgfw_console_input_enable(!enabled);
	}

	if (action == 1) {
		if (kgfw_input_key(KGFW_KEY_ESCAPE)) {
			state.exit = 1;
			return;
		}
		if (kgfw_input_key_down(KGFW_KEY_R)) {
			char * argv[3] = { "gfx", "reload", "shaders" };
			kgfw_console_run(3, argv);
		}
		if (kgfw_input_key_down(KGFW_KEY_N)) {
			if (storage.meshes_count > 255) {
				goto too_many_meshes;
			}

			mesh.pos[0] = state.camera.pos[0];
			mesh.pos[1] = state.camera.pos[1];
			mesh.pos[2] = state.camera.pos[2];
			kgfw_graphics_mesh_node_t * m = kgfw_graphics_mesh_new(&mesh, NULL);
			ktga_t * tga = texture_get("empty");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(m, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			storage.meshes[storage.meshes_count++] = m;
		too_many_meshes:;
		}
		if (key == KGFW_KEY_BACKSPACE) {
			if (storage.current != NULL && *storage.current) {
				kgfw_graphics_mesh_destroy(*storage.current);
				*storage.current = NULL;
				storage.current = NULL;
				storage.select_mesh->transform.scale[0] = 0;
			}
		}
	}
}

static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action) {
	if (button == KGFW_MOUSE_LBUTTON && action == 1) {
		storage.current = click();
		if (storage.current != NULL && *storage.current != NULL) {
			storage.select_mesh->transform.scale[0] = 1;
			storage.select_mesh->transform.pos[0] = (*storage.current)->transform.pos[0];
			storage.select_mesh->transform.pos[1] = (*storage.current)->transform.pos[1];
			storage.select_mesh->transform.pos[2] = (*storage.current)->transform.pos[2];
			//kgfw_graphics_mesh_texture_detach(*storage.current, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		} else {
			storage.select_mesh->transform.scale[0] = 0;
		}
	}
}

static int player_movement_init(player_movement_t * self, player_movement_state_t * mstate) {
	return 0;
}

static int player_ortho_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (kgfw_input_mouse_button(KGFW_MOUSE_LBUTTON)) {
		float x, y;
		kgfw_input_mouse_delta(&x, &y);
		state.camera.pos[0] += x / 30 * state.camera.scale[0];
		state.camera.pos[1] -= y / 30 * state.camera.scale[1];
	} else if (kgfw_input_key(KGFW_KEY_LSHIFT)) {
		float x, y;
		kgfw_input_mouse_scroll(&x, &y);
		state.camera.pos[0] += y * state.camera.scale[0] / 5;
	} else {
		float x, y;
		kgfw_input_mouse_scroll(&x, &y);
		state.camera.pos[0] -= x * state.camera.scale[0] / 5;
		state.camera.pos[1] += y * state.camera.scale[1] / 5;
	}

	if (kgfw_input_key(KGFW_KEY_LEFT)) {
		state.camera.pos[0] -= state.camera.scale[0] / 5;
	}
	if (kgfw_input_key(KGFW_KEY_RIGHT)) {
		state.camera.pos[0] += state.camera.scale[0] / 5;
	}
	if (kgfw_input_key(KGFW_KEY_DOWN)) {
		state.camera.pos[1] -= state.camera.scale[1] / 5;
	}
	if (kgfw_input_key(KGFW_KEY_UP)) {
		state.camera.pos[1] += state.camera.scale[1] / 5;
	}

	if (kgfw_input_key(KGFW_KEY_RBRACKET)) {
		state.camera.scale[0] /= 1.05f;
		state.camera.scale[1] /= 1.05f;
	} else if (kgfw_input_key(KGFW_KEY_LBRACKET)) {
		state.camera.scale[0] *= 1.05f;
		state.camera.scale[1] *= 1.05f;
	}

	return 0;
}

static int player_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (!state.input) {
		return 0;
	}

	float move_speed = 25.0f;
	float move_slow_speed = 10.0f;
	float look_sensitivity = 150.0f;
	float look_slow_sensitivity = 50.0f;
	float mouse_sensitivity = 0.3f;
	float delta = kgfw_time_delta();

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
		mstate->camera->rot[1] += look_sensitivity * delta;
	}
	if (kgfw_input_key(KGFW_KEY_LEFT)) {
		mstate->camera->rot[1] -= look_sensitivity * delta;
	}
	if (kgfw_input_key(KGFW_KEY_UP)) {
		mstate->camera->rot[0] += look_sensitivity * delta;
	}
	if (kgfw_input_key(KGFW_KEY_DOWN)) {
		mstate->camera->rot[0] -= look_sensitivity * delta;
	}

	float dx, dy;
	kgfw_input_mouse_delta(&dx, &dy);
	mstate->camera->rot[0] += dy * mouse_sensitivity;
	mstate->camera->rot[1] -= dx * mouse_sensitivity;

	if (mstate->camera->rot[0] > 90) {
		mstate->camera->rot[0] = 90;
	}
	if (mstate->camera->rot[0] < -90) {
		mstate->camera->rot[0] = -90;
	}

	vec3 up;
	vec3 right;
	vec3 forward;
	up[0] = 0; up[1] = 1; up[2] = 0;
	right[0] = 0; right[1] = 0; right[2] = 0;
	forward[0] = -sinf(mstate->camera->rot[1] * 3.141592f / 180.0f); forward[1] = 0; forward[2] = cosf(mstate->camera->rot[1] * 3.141592f / 180.0f);
	vec3_mul_cross(right, up, forward);
	vec3_scale(up, up, move_speed * delta);
	vec3_scale(right, right, move_speed * delta);
	vec3_scale(forward, forward, -move_speed * delta);

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

static kgfw_graphics_mesh_node_t ** click(void) {
	float sx, sy;
	kgfw_input_mouse_pos(&sx, &sy);
	vec4 mouse = { (sx * 2 / state.window.width) - 1, (-sy * 2 / state.window.height) + 1, state.camera.pos[2], 1};

	mat4x4 vp;
	mat4x4 ivp;
	mat4x4 v;
	{
		kgfw_camera_perspective(&state.camera, vp);
		kgfw_camera_view(&state.camera, v);
		mat4x4_mul(vp, vp, v);
		mat4x4_invert(ivp, vp);
	}

	vec4 pos = { 0, 0, 0, 1 };
	mat4x4_mul_vec4(pos, ivp, mouse);
	pos[0] *= pos[3];
	pos[1] *= pos[3];
	pos[2] *= pos[3];

	vec4 dir = { vp[2][0], vp[2][1], vp[2][2], 0 };

	kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "mouse %f %f screen %f %f world %f %f %f", sx, sy, mouse[0], mouse[1], pos[0], pos[1], pos[2]);

	for (unsigned long long int i = 0; i < storage.meshes_count; ++i) {
		if (storage.meshes[i] == NULL) {
			continue;
		}

		mat4x4 m;
		mat4x4_mul(m, storage.meshes[i]->matrices.scale, storage.meshes[i]->matrices.rotation);
		mat4x4_mul(m, m, storage.meshes[i]->matrices.translation);

		vec3 max = { storage.meshes[i]->transform.pos[0] + storage.meshes[i]->transform.scale[0], storage.meshes[i]->transform.pos[1] + storage.meshes[i]->transform.scale[1], storage.meshes[i]->transform.pos[2] + storage.meshes[i]->transform.scale[2] };
		vec3 min = { storage.meshes[i]->transform.pos[0] - storage.meshes[i]->transform.scale[0], storage.meshes[i]->transform.pos[1] - storage.meshes[i]->transform.scale[1], storage.meshes[i]->transform.pos[2] - storage.meshes[i]->transform.scale[2] };

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos %f %f %f %f dir %f %f %f %f", pos[0], pos[1], pos[2], pos[3], dir[0], dir[1], dir[2], dir[3]);
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "max %f %f %f min %f %f %f", max[0], max[1], max[2], min[0], min[1], min[2]);
		if ((pos[0] > min[0])) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos[0] > min[0]");
			if ((pos[0] < max[0])) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos[0] < max[0]");
				if ((pos[1] > min[1])) {
					kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos[1] > min[1]");
					if ((pos[1] < max[1])) {
						kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "hit");
						return &storage.meshes[i];
					}
				}
			}
		}
	}

	return NULL;
}

static int exit_command(int argc, char ** argv) {
	state.exit = 1;

	return 0;
}

static ktga_t * texture_get(char * name) {
	unsigned long long int hash = kgfw_hash(name);
	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		if (hash == storage.texture_hashes[i]) {
			return &storage.textures[i];
		}
	}

	return NULL;
}
