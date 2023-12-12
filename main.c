#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "kgfw/kgfw.h"
#include "kgfw/ktga/ktga.h"
#include "kgfw/koml/koml.h"
#include "kgfw/kobj/kobj.h"
#ifndef KGFW_WINDOWS
#include <unistd.h>
#endif
#include <linmath.h>

struct {
	kgfw_window_t window;
	kgfw_camera_t camera;
	unsigned char input;
	unsigned char exit;
	struct {
		float movement;
		float arrow_speed;
		float mouse_speed;
		float jump_force;
		float gravity;
	} settings;
} static state = {
	{ 0 },
	{ { 0, 0, 5 }, { 0, 0, 0 }, { 1, 1 }, 90, 0.01f, 1000.0f, 1.3333f, 0 },
	1, 0,
	{
		10.0f,
		90.0f,
		0.15f,
		10.0f,
		25.0f,
	},
};

struct {
	vec3 pos;
	vec3 rot;
	vec3 vel;
} static player = {
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
};

#define STORAGE_MAX_TEXTURES 64
#define STORAGE_MAX_MESHES 64
#define EVALUATION_MAX_CYCLES 100

struct {
	ktga_t textures[STORAGE_MAX_TEXTURES];
	unsigned long long int textures_count;
	kgfw_hash_t texture_hashes[STORAGE_MAX_TEXTURES];
	kgfw_graphics_mesh_t meshes[STORAGE_MAX_MESHES];
	unsigned long long int meshes_count;
	kgfw_hash_t mesh_hashes[STORAGE_MAX_MESHES];
} static storage = {
	{ 0 },
	0,
	{ 0 },
	{ 0 },
	0,
	{ 0 },
};

static int kgfw_log_handler(kgfw_log_severity_enum severity, char * string);
static int kgfw_logc_handler(kgfw_log_severity_enum severity, char character);
static void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action);
static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action);
static ktga_t * texture_get(char * name);
static int textures_load(void);
static void textures_cleanup(void);
static kgfw_graphics_mesh_t * mesh_get(char * name);
static int meshes_load(void);
static void meshes_cleanup(void);

/*
static int player_movement_init(player_movement_t * self, player_movement_state_t * mstate);
static int player_ortho_movement_update(player_movement_t * self, player_movement_state_t * mstate);
static int player_movement_update(player_movement_t * self, player_movement_state_t * mstate);
*/

static int exit_command(int argc, char ** argv);
static int game_command(int argc, char ** argv);

/* components */
static void test_start(kgfw_component_t * self);
static void test_update(kgfw_component_t * self);
static void test_destroy(kgfw_component_t * self);

int main(int argc, char ** argv) {
	kgfw_log_register_callback(kgfw_log_handler);
	kgfw_logc_register_callback(kgfw_logc_handler);
	if (kgfw_init() != 0) {
		return 1;
	}

	kgfw_time_init();

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

	if (textures_load() != 0) {
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	if (meshes_load() != 0) {
		meshes_cleanup();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	if (kgfw_console_init() != 0) {
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	kgfw_console_register_command("exit", exit_command);
	kgfw_console_register_command("quit", exit_command);
	kgfw_console_register_command("game", game_command);

	if (kgfw_commands_init() != 0) {
		kgfw_console_deinit();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 5;
	}

	if (kgfw_ecs_init() != 0) {
		kgfw_console_deinit();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 6;
	}

	kgfw_input_key_register_callback(kgfw_key_handler);
	kgfw_input_mouse_button_register_callback(kgfw_mouse_button_handle);

	kgfw_entity_t * entity = kgfw_entity_new("bob");
	if (entity == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity creation failure");
		return 69420;
	}
	entity->transform.pos[1] = 5;
	kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity 0x%llx:\n  name: \"%s\"\n  transform:\n    pos:   %f, %f, %f\n    rot:   %f, %f, %f\n    scale: %f, %f, %f\n  components:\n    count: %llu\n    ptr: %016p", entity->id, entity->name, entity->transform.pos[0], entity->transform.pos[1], entity->transform.pos[2], entity->transform.rot[0], entity->transform.rot[1], entity->transform.rot[2], entity->transform.scale[0], entity->transform.scale[1], entity->transform.scale[2], entity->components.count, entity->components.handles);

	entity = kgfw_entity_copy(NULL, entity);
	if (entity == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity creation failure");
		return 69420;
	}
	kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity 0x%llx:\n  name: \"%s\"\n  transform:\n    pos:   %f, %f, %f\n    rot:   %f, %f, %f\n    scale: %f, %f, %f\n  components:\n    count: %llu\n    ptr: %016p", entity->id, entity->name, entity->transform.pos[0], entity->transform.pos[1], entity->transform.pos[2], entity->transform.rot[0], entity->transform.rot[1], entity->transform.rot[2], entity->transform.scale[0], entity->transform.scale[1], entity->transform.scale[2], entity->components.count, entity->components.handles);

	entity = kgfw_entity_get(entity->id);
	if (entity == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity get failure");
		return 69420;
	}
	kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity 0x%llx:\n  name: \"%s\"\n  transform:\n    pos:   %f, %f, %f\n    rot:   %f, %f, %f\n    scale: %f, %f, %f\n  components:\n    count: %llu\n    ptr: %016p", entity->id, entity->name, entity->transform.pos[0], entity->transform.pos[1], entity->transform.pos[2], entity->transform.rot[0], entity->transform.rot[1], entity->transform.rot[2], entity->transform.scale[0], entity->transform.scale[1], entity->transform.scale[2], entity->components.count, entity->components.handles);

	entity = kgfw_entity_get_via_name("bob");
	if (entity == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity get failure");
		return 69420;
	}
	kgfw_logf(KGFW_LOG_SEVERITY_INFO, "entity 0x%llx:\n  name: \"%s\"\n  transform:\n    pos:   %f, %f, %f\n    rot:   %f, %f, %f\n    scale: %f, %f, %f\n  components:\n    count: %llu\n    ptr: %016p", entity->id, entity->name, entity->transform.pos[0], entity->transform.pos[1], entity->transform.pos[2], entity->transform.rot[0], entity->transform.rot[1], entity->transform.rot[2], entity->transform.scale[0], entity->transform.scale[1], entity->transform.scale[2], entity->components.count, entity->components.handles);

	kgfw_component_t data = {
		.start = test_start,
		.update = test_update,
		.destroy = test_destroy,
	};

	int * list = kgfw_list_new(int);
	list = kgfw_list_reserve(list, 9);
	memset(list, 0xFFEEDDCC, sizeof(int) * 9);
	for (unsigned int i = 0; i < 9; ++i) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "%x", list[i]);
	}
	kgfw_list_destroy(list);

	kgfw_uuid_t test_ctype = kgfw_component_construct("test", sizeof(kgfw_component_t), &data, 0);
	kgfw_component_t * test_component = kgfw_entity_attach_component(entity, test_ctype);
	if (test_component == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "component creation failure");
		return 69420;
	}

	/*
	player_movement_t * mov = NULL;
	{
		player_movement_t m = {
			sizeof(player_movement_t),
			(int (*)(void *, void *)) player_movement_init,
			(int (*)(void *, void *)) player_movement_update,
		};

		if (state.camera.ortho) {
			m.update = (int (*)(void *, void *)) player_ortho_movement_update;
			state.camera.nplane = 0;
		}

		mov = kgfw_ecs_get(kgfw_ecs_register((kgfw_ecs_component_t *) &m, "player_movement"));
	}

	player_movement_state_t mov_state = {
		sizeof(player_movement_state_t),
		&state.camera,
	};

	mov_state.mesh = kgfw_graphics_mesh_new(mesh_get("capsule"), NULL);

	mov->init(mov, &mov_state);
	*/

	{
		kgfw_graphics_mesh_t * m = mesh_get("teapot");
		if (m == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "failed to load test obj");
			goto skip_load_m;
		}
		kgfw_graphics_mesh_node_t * node = kgfw_graphics_mesh_new(m, NULL);

		ktga_t * tga = texture_get("teapot");
		kgfw_graphics_texture_t tex = {
			.bitmap = tga->bitmap,
			.width = tga->header.img_w,
			.height = tga->header.img_h,
			.fmt = KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
			.u_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			.v_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			.filtering = KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
		};
		kgfw_graphics_mesh_texture(node, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
	skip_load_m:;
	}

	while (!state.window.closed && !state.exit) {
		kgfw_time_start();
		if (kgfw_graphics_draw() != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to draw");
			break;
		}

		if (kgfw_window_update(&state.window) != 0) {
			state.exit = 1;
			break;
		}

		//kgfw_ecs_update();

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
		
		/*
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
		*/

		kgfw_input_update();
		kgfw_audio_update();
		kgfw_time_end();
		if (state.input) {
			//kgfw_logf(KGFW_LOG_SEVERITY_INFO, "abs time: %f    frame time: %f    fps: %f", kgfw_time_get(), kgfw_time_delta(), 1 / kgfw_time_delta());
		}
	}

	kgfw_ecs_deinit();
	kgfw_console_deinit();
	meshes_cleanup();
	textures_cleanup();
	//kgfw_ecs_cleanup();
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
	if (key == KGFW_KEY_ESCAPE) {
		state.exit = 1;
	}

	if (state.input) {
		if (key == KGFW_KEY_R && action == 1) {
			char * argv[3] = { "gfx", "reload", "shaders" };
			kgfw_console_run(3, argv);
		}
	}
}

static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action) {
	return;
}

/*
static int player_movement_init(player_movement_t * self, player_movement_state_t * mstate) {
	return 0;
}

static int player_ortho_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (kgfw_input_mouse_button(KGFW_MOUSE_LBUTTON)) {
		float sx, sy;

		kgfw_input_mouse_pos(&sx, &sy);
		vec4 mouse = { (sx * 2 / state.window.width) - 1, (-sy * 2 / state.window.height) + 1, state.camera.pos[2], 1 };

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
	}
	
	if (kgfw_input_key(KGFW_KEY_LSHIFT)) {
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

	if (kgfw_input_key(KGFW_KEY_EQUAL)) {
		state.camera.scale[0] /= 1.05f;
		state.camera.scale[1] /= 1.05f;
	} else if (kgfw_input_key(KGFW_KEY_MINUS)) {
		state.camera.scale[0] *= 1.05f;
		state.camera.scale[1] *= 1.05f;
	}

	return 0;
}

static int player_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (!state.input) {
		return 0;
	}

	float move_speed = state.settings.movement;
	float move_fast_speed = state.settings.movement * 5.0f;
	float move_slow_speed = state.settings.movement / 5.0f;
	float look_sensitivity = state.settings.arrow_speed;
	float look_slow_sensitivity = state.settings.arrow_speed / 4.0f;
	float mouse_sensitivity = state.settings.mouse_speed;
	float jump_force = state.settings.jump_force;
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

	if (kgfw_input_key(KGFW_KEY_LSHIFT)) {
		move_speed = move_fast_speed;
	}

	if (kgfw_input_key(KGFW_KEY_RIGHT)) {
		player.rot[1] += look_sensitivity * delta;
	}
	if (kgfw_input_key(KGFW_KEY_LEFT)) {
		player.rot[1] -= look_sensitivity * delta;
	}
	if (kgfw_input_key(KGFW_KEY_UP)) {
		player.rot[0] += look_sensitivity * delta;
	}
	if (kgfw_input_key(KGFW_KEY_DOWN)) {
		player.rot[0] -= look_sensitivity * delta;
	}

	float dx, dy;
	kgfw_input_mouse_delta(&dx, &dy);
	player.rot[0] += dy * mouse_sensitivity;
	player.rot[1] -= dx * mouse_sensitivity;

	if (player.rot[0] > 90) {
		player.rot[0] = 90;
	}
	if (player.rot[0] < -90) {
		player.rot[0] = -90;
	}

	vec3 up;
	vec3 right;
	vec3 forward;
	up[0] = 0; up[1] = 1; up[2] = 0;
	right[0] = 0; right[1] = 0; right[2] = 0;
	forward[0] = -sinf(player.rot[1] * 3.141592f / 180.0f); forward[1] = 0; forward[2] = cosf(player.rot[1] * 3.141592f / 180.0f);
	vec3_mul_cross(right, up, forward);
	vec3_scale(up, up, move_speed * delta);
	vec3_scale(right, right, move_speed * delta);
	vec3_scale(forward, forward, -move_speed * delta);

	if (kgfw_input_key(KGFW_KEY_W)) {
		vec3_add(player.pos, player.pos, forward);
	}
	if (kgfw_input_key(KGFW_KEY_S)) {
		vec3_sub(player.pos, player.pos, forward);
	}
	if (kgfw_input_key(KGFW_KEY_D)) {
		vec3_add(player.pos, player.pos, right);
	}
	if (kgfw_input_key(KGFW_KEY_A)) {
		vec3_sub(player.pos, player.pos, right);
	}
	if (kgfw_input_key(KGFW_KEY_E)) {
		vec3_add(player.pos, player.pos, up);
	}
	if (kgfw_input_key(KGFW_KEY_Q)) {
		vec3_sub(player.pos, player.pos, up);
	}
	if (kgfw_input_key_down(KGFW_KEY_SPACE)) {
		player.vel[1] = jump_force;
	}

	if (player.pos[1] <= 0) {
		player.vel[1] = 0;
		player.pos[1] = 0;
	} else {
		player.vel[1] -= state.settings.gravity;
	}

	player.pos[0] += player.vel[0];
	player.pos[1] += player.vel[1];
	player.pos[2] += player.vel[2];

	mstate->camera->pos[0] = player.pos[0];
	mstate->camera->pos[1] = player.pos[1] + 4;
	mstate->camera->pos[2] = player.pos[2];
	mstate->camera->rot[0] = player.rot[0];
	mstate->camera->rot[1] = player.rot[1];
	mstate->camera->rot[2] = player.rot[2];

	mstate->mesh->transform.pos[0] = player.pos[0];
	mstate->mesh->transform.pos[1] = player.pos[1];
	mstate->mesh->transform.pos[2] = player.pos[2];

	return 0;
}
*/

static int exit_command(int argc, char ** argv) {
	state.exit = 1;

	return 0;
}

static int textures_load(void) {
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
			return 2;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"config.koml\"");
			return 3;
		}

		fclose(fp);
	}

	koml_table_t ktable;
	if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"config.koml\"");
		return 4;
	}

	koml_symbol_t * files = koml_table_symbol(&ktable, "textures:files");
	koml_symbol_t * names = koml_table_symbol(&ktable, "textures:names");
	if (files == NULL) {
		goto skip_tga_load;
	}
	storage.textures_count = files->data.array.length;

	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		FILE * fp = fopen(files->data.array.elements.string[i], "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"%s\"", files->data.array.elements.string[i]);
			return 5;
		}

		fseek(fp, 0L, SEEK_END);
		unsigned long long int size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		void * buffer = malloc(size);
		if (buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"%s\"", files->data.array.elements.string[i]);
			return 6;
		}

		if (fread(buffer, 1, size, fp) != size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"%s\" %llu", files->data.array.elements.string[i], size);
			return 7;
		}

		fclose(fp);
		ktga_load(&storage.textures[i], buffer, size);
		free(buffer);

		if (names == NULL) {
			unsigned long long int len = strlen(files->data.array.elements.string[i]);
			storage.texture_hashes[i] = kgfw_hash(files->data.array.elements.string[i]);
		}
		else {
			unsigned long long int len = strlen(names->data.array.elements.string[i]);
			storage.texture_hashes[i] = kgfw_hash(names->data.array.elements.string[i]);
		}
	}

skip_tga_load:;
	koml_table_destroy(&ktable);
	return 0;
}

static void textures_cleanup(void) {
	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		if (storage.textures[i].bitmap == NULL) {
			continue;
		}

		ktga_destroy(&storage.textures[i]);
		memset(&storage.textures[i], 0, sizeof(ktga_t));
		memset(&storage.texture_hashes[i], 0, sizeof(kgfw_hash_t));
	}
	storage.textures_count = 0;
}

static ktga_t * texture_get(char * name) {
	kgfw_hash_t hash = kgfw_hash(name);
	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		if (hash == storage.texture_hashes[i]) {
			return &storage.textures[i];
		}
	}

	return NULL;
}

static int meshes_load(void) {
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
			return 2;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"config.koml\"");
			return 3;
		}

		fclose(fp);
	}

	koml_table_t ktable;
	if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"config.koml\"");
		return 4;
	}

	koml_symbol_t * files = koml_table_symbol(&ktable, "meshes:files");
	koml_symbol_t * names = koml_table_symbol(&ktable, "meshes:names");
	if (files == NULL) {
		goto skip_mesh_load;
	}
	storage.meshes_count = files->data.array.length;

	for (unsigned long long int mi = 0; mi < storage.meshes_count; ++mi) {
		FILE * fp = fopen(files->data.array.elements.string[mi], "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"%s\"", files->data.array.elements.string[mi]);
			return 5;
		}

		fseek(fp, 0L, SEEK_END);
		unsigned long long int size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		void * buffer = malloc(size);
		if (buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"%s\"", files->data.array.elements.string[mi]);
			return 6;
		}

		if (fread(buffer, 1, size, fp) != size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"%s\" %llu", files->data.array.elements.string[mi], size);
			return 7;
		}

		fclose(fp);

		kobj_t kobj;
		kobj_load(&kobj, buffer, size);

		storage.meshes[mi] = (kgfw_graphics_mesh_t) {
			.pos = { 0, 0, 0 },
			.rot = { 0, 0, 0 },
			.scale = { 1, 1, 1 },
		};

		storage.meshes[mi].vertices = malloc(sizeof(kgfw_graphics_vertex_t) * kobj.vcount);
		if (storage.meshes[mi].vertices == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to allocate mesh vertices buffer");
			return 1;
		}
		storage.meshes[mi].indices = malloc(sizeof(unsigned int) * 3 * kobj.fcount);
		if (storage.meshes[mi].indices == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to allocate mesh indices buffer");
			return 2;
		}
		storage.meshes[mi].vertices_count = kobj.vcount;
		storage.meshes[mi].indices_count = kobj.fcount * 3;

		for (unsigned long long int i = 0; i < storage.meshes[mi].vertices_count; ++i) {
			storage.meshes[mi].vertices[i].x = kobj.vertices[i * 3 + 0];
			storage.meshes[mi].vertices[i].y = kobj.vertices[i * 3 + 1];
			storage.meshes[mi].vertices[i].z = kobj.vertices[i * 3 + 2];
			storage.meshes[mi].vertices[i].r = 1;
			storage.meshes[mi].vertices[i].g = 1;
			storage.meshes[mi].vertices[i].b = 1;
		}

		for (unsigned long long int i = 0; i < kobj.fcount; ++i) {
			if (kobj.faces[i].vn1 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].nx = kobj.normals[(kobj.faces[i].vn1 - 1) * 3 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].ny = kobj.normals[(kobj.faces[i].vn1 - 1) * 3 + 1];
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].nz = kobj.normals[(kobj.faces[i].vn1 - 1) * 3 + 2];
			}
			if (kobj.faces[i].vn2 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].nx = kobj.normals[(kobj.faces[i].vn2 - 1) * 3 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].ny = kobj.normals[(kobj.faces[i].vn2 - 1) * 3 + 1];
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].nz = kobj.normals[(kobj.faces[i].vn2 - 1) * 3 + 2];
			}
			if (kobj.faces[i].vn3 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].nx = kobj.normals[(kobj.faces[i].vn3 - 1) * 3 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].ny = kobj.normals[(kobj.faces[i].vn3 - 1) * 3 + 1];
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].nz = kobj.normals[(kobj.faces[i].vn3 - 1) * 3 + 2];
			}

			if (kobj.faces[i].vt1 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].u = kobj.uvs[(kobj.faces[i].vt1 - 1) * 2 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].v = kobj.uvs[(kobj.faces[i].vt1 - 1) * 2 + 1];
			}
			if (kobj.faces[i].vt2 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].u = kobj.uvs[(kobj.faces[i].vt2 - 1) * 2 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].v = kobj.uvs[(kobj.faces[i].vt2 - 1) * 2 + 1];
			}
			if (kobj.faces[i].vt3 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].u = kobj.uvs[(kobj.faces[i].vt3 - 1) * 2 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].v = kobj.uvs[(kobj.faces[i].vt3 - 1) * 2 + 1];
			}

			if (kobj.faces[i].v1 != 0) {
				storage.meshes[mi].indices[i * 3 + 0] = kobj.faces[i].v1 - 1;
			}
			if (kobj.faces[i].v2 != 0) {
				storage.meshes[mi].indices[i * 3 + 1] = kobj.faces[i].v2 - 1;
			}
			if (kobj.faces[i].v3 != 0) {
				storage.meshes[mi].indices[i * 3 + 2] = kobj.faces[i].v3 - 1;
			}
		}

		kobj_destroy(&kobj);

		free(buffer);

		if (names == NULL) {
			storage.mesh_hashes[mi] = kgfw_hash(files->data.array.elements.string[mi]);
		} else {
			storage.mesh_hashes[mi] = kgfw_hash(names->data.array.elements.string[mi]);
		}
	}

skip_mesh_load:;
	koml_table_destroy(&ktable);
	return 0;
}

static void meshes_cleanup(void) {
	for (unsigned long long int i = 0; i < storage.meshes_count; ++i) {
		if (storage.meshes[i].vertices != NULL) {
			free(storage.meshes[i].vertices);
		}
		if (storage.meshes[i].indices != NULL) {
			free(storage.meshes[i].indices);
		}
	}
	storage.meshes_count = 0;
}

static kgfw_graphics_mesh_t * mesh_get(char * name) {
	kgfw_hash_t hash = kgfw_hash(name);
	for (unsigned long long int i = 0; i < storage.meshes_count; ++i) {
		if (hash == storage.mesh_hashes[i]) {
			return &storage.meshes[i];
		}
	}

	return NULL;
}

static int game_command(int argc, char ** argv) {
	const char * subcommands = "mesh    fov    movement    arrow_speed    mouse_speed    jump_force    gravity    pos";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}

	if (strcmp(argv[1], "mesh") == 0) {
		if (argc < 3) {
			const char * args = "[mesh name] (optional texture name)";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}
		kgfw_graphics_mesh_node_t * node = NULL;
		if (argc >= 3) {
			kgfw_graphics_mesh_t * mesh = mesh_get(argv[2]);
			if (mesh == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "mesh does not exist \"%s\"", argv[2]);
				return 0;
			}
			node = kgfw_graphics_mesh_new(mesh, NULL);
			node->transform.pos[0] = state.camera.pos[0];
			node->transform.pos[1] = state.camera.pos[1];
			node->transform.pos[2] = state.camera.pos[2];

			node->transform.rot[1] = state.camera.rot[1];
		}
		if (argc >= 4) {
			ktga_t * tga = texture_get(argv[3]);
			if (tga == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "texture does not exist \"%s\"", argv[3]);
				return 0;
			}

			kgfw_graphics_texture_t tex = {
				.bitmap = tga->bitmap,
				.width = tga->header.img_w,
				.height = tga->header.img_h,
				.fmt = KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				.u_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				.v_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				.filtering = KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(node, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
	} else if (strcmp(argv[1], "fov") == 0) {
		if (argc < 3) {
			const char * args = "[field of view in degrees]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.camera.fov = f;
	} else if (strcmp(argv[1], "movement") == 0) {
		if (argc < 3) {
			const char * args = "[movement speed]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.movement = f;
	} else if (strcmp(argv[1], "arrow_speed") == 0) {
		if (argc < 3) {
			const char * args = "[arrow speed]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.arrow_speed = f;
	} else if (strcmp(argv[1], "mouse_speed") == 0) {
		if (argc < 3) {
			const char * args = "[mouse speed]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.mouse_speed = f;
	} else if (strcmp(argv[1], "jump_force") == 0) {
		if (argc < 3) {
			const char * args = "[jump force]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.jump_force = f;
	} else if (strcmp(argv[1], "gravity") == 0) {
		if (argc < 3) {
			const char * args = "[gravity]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.gravity = f;
	} else if (strcmp(argv[1], "pos") == 0) {
		if (argc < 5) {
			const char * args = "[pos]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		player.pos[0] = strtof(argv[2], NULL);
		player.pos[1] = strtof(argv[3], NULL);
		player.pos[2] = strtof(argv[4], NULL);
		state.camera.pos[0] = player.pos[0];
		state.camera.pos[1] = player.pos[1];
		state.camera.pos[2] = player.pos[2];
	} else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}

	return 0;
}


/* components */
static void test_start(kgfw_component_t * self) {
	kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "component started %p", self);
}

static void test_update(kgfw_component_t * self) {
	kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "component updated %p", self);
}

static void test_destroy(kgfw_component_t * self) {
	kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "component destroyed %p", self);
}
