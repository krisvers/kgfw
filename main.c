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
	{ { 0, 0, 5 }, { 0, 0, 0 }, { 1, 1 }, 90, 0.01f, 1000.0f, 1.3333f, 1 },
	1, 0
};

typedef enum gate_type_enum {
	GATE_NONE = 0,
	GATE_AND,
	GATE_OR,
	GATE_XOR,
	GATE_NOT,
	GATE_INPUT,
	GATE_OUTPUT,
	GATE_RELAY,
} gate_type_enum;

typedef struct gate_t {
	gate_type_enum type;

	kgfw_graphics_mesh_node_t * mesh;
	unsigned char value;
	unsigned char evaluated;

	struct wire_t * left;
	struct wire_t * right;
} gate_t;

typedef struct output_t {
	gate_type_enum type;

	kgfw_graphics_mesh_node_t * mesh;
	unsigned char value;
	unsigned char evaluated;

	struct wire_t * left;
} output_t;

typedef struct input_t {
	gate_type_enum type;

	kgfw_graphics_mesh_node_t * mesh;
	unsigned char value;
} input_t;

#define CIRCUIT_MAX_INPUTS 16
#define CIRCUIT_MAX_OUTPUTS 16

typedef struct circuit_t {
	input_t * inputs[CIRCUIT_MAX_INPUTS];
	unsigned long long int inputs_count;
	output_t * outputs[CIRCUIT_MAX_OUTPUTS];
	unsigned long long int outputs_count;
	char * name;
} circuit_t;

typedef struct wire_t {
	gate_t * start;
	gate_t * end;
	kgfw_graphics_mesh_node_t * mesh;
} wire_t;

#define STORAGE_MAX_GATES 256
#define STORAGE_MAX_WIRES 512
#define STORAGE_MAX_TEXTURES 16
#define EVALUATION_MAX_CYCLES 100

struct {
	wire_t wires[STORAGE_MAX_WIRES];
	unsigned long long int wires_count;
	unsigned char wires_available[STORAGE_MAX_WIRES];
	unsigned long long int wire_first_available;
	gate_t gates[STORAGE_MAX_GATES];
	unsigned long long int gates_count;
	unsigned char gates_available[STORAGE_MAX_GATES];
	unsigned long long int gate_first_available;
	circuit_t circuit;
	ktga_t textures[STORAGE_MAX_TEXTURES];
	unsigned long long int textures_count;
	unsigned long long int texture_hashes[STORAGE_MAX_TEXTURES];
	gate_t * current;
	unsigned long long int evaluation_cycles;
} static storage = {
	{ 0 },
	0,
	{ 0 },
	0,
	{ 0 },
	0,
	{ 0 },
	0,
	{ { NULL }, 0, { NULL }, 0, NULL },
	{ 0 },
	0,
	{ 0 },
	NULL,
	0
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
static int textures_load(void);
static void textures_cleanup(void);
static unsigned char evaluate(gate_t * gate);
static void reset_evaluation(gate_t * gate);

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
static int game_command(int argc, char ** argv);
static gate_t * click(void);

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

	if (textures_load() != 0) {
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
			state.camera.nplane = 0;
		}

		mov = kgfw_ecs_get(kgfw_ecs_register((kgfw_ecs_component_t *) &m, "player_movement"));
	}

	player_movement_state_t mov_state = {
		sizeof(player_movement_state_t),
		&state.camera,
	};

	mov->init(mov, &mov_state);

	//kgfw_graphics_mesh_node_t * current = NULL;
	while (!state.window.closed && !state.exit) {
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

		for (unsigned long long int i = 0; i < storage.gates_count; ++i) {
			wire_t * w = NULL;
			gate_t * g = NULL;
			if (storage.gates[i].left != NULL) {
				w = storage.gates[i].left;
				g = w->end;
				float wi = (g->mesh->transform.pos[0] - storage.gates[i].mesh->transform.pos[0]) / 2.0f;
				float he = (g->mesh->transform.pos[1] - storage.gates[i].mesh->transform.pos[1]) / 2.0f;
				float de = (g->mesh->transform.pos[2] - storage.gates[i].mesh->transform.pos[2]) / 2.0f;
				float dist = sqrtf(wi * wi + he * he + de * de);
				w->mesh->transform.scale[0] = dist;

				float angle = atan2f(g->mesh->transform.pos[1] - storage.gates[i].mesh->transform.pos[1], storage.gates[i].mesh->transform.pos[0] - g->mesh->transform.pos[0]);
				w->mesh->transform.rot[2] = -angle * 180.0f / 3.141592f;
				w->mesh->transform.pos[0] = storage.gates[i].mesh->transform.pos[0] + wi;
				w->mesh->transform.pos[1] = storage.gates[i].mesh->transform.pos[1] + he;
				w->mesh->transform.pos[2] = storage.gates[i].mesh->transform.pos[2] + de;
			}
			if (storage.gates[i].right != NULL) {
				w = storage.gates[i].right;
				g = w->end;
				float wi = (g->mesh->transform.pos[0] - storage.gates[i].mesh->transform.pos[0]) / 2.0f;
				float he = (g->mesh->transform.pos[1] - storage.gates[i].mesh->transform.pos[1]) / 2.0f;
				float de = (g->mesh->transform.pos[2] - storage.gates[i].mesh->transform.pos[2]) / 2.0f;
				float dist = sqrtf(wi * wi + he * he + de * de);
				w->mesh->transform.scale[0] = dist;

				float angle = atan2f(g->mesh->transform.pos[1] - storage.gates[i].mesh->transform.pos[1], storage.gates[i].mesh->transform.pos[0] - g->mesh->transform.pos[0]);
				w->mesh->transform.rot[2] = -angle * 180.0f / 3.141592f;
				w->mesh->transform.pos[0] = storage.gates[i].mesh->transform.pos[0] + wi;
				w->mesh->transform.pos[1] = storage.gates[i].mesh->transform.pos[1] + he;
				w->mesh->transform.pos[2] = storage.gates[i].mesh->transform.pos[2] + de;
			}
		}

		for (unsigned long long int i = 0; i < storage.circuit.outputs_count; ++i) {
			storage.evaluation_cycles = 0;
			evaluate((gate_t *) storage.circuit.outputs[i]);
		}

		for (unsigned long long int i = 0; i < storage.circuit.outputs_count; ++i) {
			storage.evaluation_cycles = 0;
			reset_evaluation((gate_t *) storage.circuit.outputs[i]);
		}
	}

	kgfw_console_deinit();
	textures_cleanup();
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
		if (key == KGFW_KEY_ESCAPE) {
			state.exit = 1;
			return;
		}
		if (key == KGFW_KEY_BACKSLASH) {
			state.camera.rot[0] = 0;
			state.camera.rot[1] = 0;
			state.camera.rot[2] = 0;
		}
		if (key == KGFW_KEY_R) {
			char * argv[3] = { "gfx", "reload", "shaders" };
			kgfw_console_run(3, argv);
			textures_cleanup();
			if (textures_load() != 0) {
				state.window.closed = 1;
			}
		}
		if (key == KGFW_KEY_N) {
			if (storage.gates_count > STORAGE_MAX_GATES) {
				goto too_many_gates;
			}
			mesh.pos[2] = 0;
			mesh.pos[0] = round((state.camera.pos[0] / (1 / 2.0f)) * (1 / 2.0f));
			mesh.pos[1] = round((state.camera.pos[1] / (1 / 2.0f)) * (1 / 2.0f));
			
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
			m->transform.scale[0] = tex.width / 16.0f;
			m->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(m, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			storage.current = &storage.gates[storage.gates_count];
			storage.gates[storage.gates_count].type = GATE_NONE;
			storage.gates[storage.gates_count].left = NULL;
			storage.gates[storage.gates_count].right = NULL;
			storage.gates[storage.gates_count].value = 0;
			storage.gates[storage.gates_count++].mesh = m;
		too_many_gates:;
		}
		if (key == KGFW_KEY_I) {
			if (storage.circuit.inputs_count > CIRCUIT_MAX_INPUTS) {
				goto too_many_inputs;
			}
			mesh.pos[2] = 0;
			mesh.pos[0] = round((state.camera.pos[0] / (1 / 2.0f)) * (1 / 2.0f));
			mesh.pos[1] = round((state.camera.pos[1] / (1 / 2.0f)) * (1 / 2.0f));
			
			kgfw_graphics_mesh_node_t * m = kgfw_graphics_mesh_new(&mesh, NULL);
			ktga_t * tga = texture_get("put_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			m->transform.scale[0] = tex.width / 16.0f;
			m->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(m, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			storage.current = &storage.gates[storage.gates_count];
			storage.circuit.inputs[storage.circuit.inputs_count++] = (input_t *) &storage.gates[storage.gates_count];
			storage.gates[storage.gates_count].type = GATE_INPUT;
			storage.gates[storage.gates_count].left = NULL;
			storage.gates[storage.gates_count].right = NULL;
			storage.gates[storage.gates_count].value = 0;
			storage.gates[storage.gates_count++].mesh = m;
		too_many_inputs:;
		}
		if (key == KGFW_KEY_O) {
			if (storage.circuit.outputs_count > CIRCUIT_MAX_OUTPUTS) {
				goto too_many_outputs;
			}
			mesh.pos[2] = 0;
			mesh.pos[0] = round((state.camera.pos[0] / (1 / 2.0f)) * (1 / 2.0f));
			mesh.pos[1] = round((state.camera.pos[1] / (1 / 2.0f)) * (1 / 2.0f));
			
			kgfw_graphics_mesh_node_t * m = kgfw_graphics_mesh_new(&mesh, NULL);
			ktga_t * tga = texture_get("put_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			m->transform.scale[0] = tex.width / 16.0f;
			m->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(m, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			storage.current = &storage.gates[storage.gates_count];
			storage.circuit.outputs[storage.circuit.outputs_count++] = (output_t *) &storage.gates[storage.gates_count];
			storage.gates[storage.gates_count].type = GATE_OUTPUT;
			storage.gates[storage.gates_count].left = NULL;
			storage.gates[storage.gates_count].right = NULL;
			storage.gates[storage.gates_count].value = 0;
			storage.gates[storage.gates_count++].mesh = m;
		too_many_outputs:;
		}
		if (key == KGFW_KEY_E) {
			if (storage.gates_count > STORAGE_MAX_GATES) {
				goto too_many_relays;
			}
			mesh.pos[2] = 0;
			mesh.pos[0] = round((state.camera.pos[0] / (1 / 2.0f)) * (1 / 2.0f));
			mesh.pos[1] = round((state.camera.pos[1] / (1 / 2.0f)) * (1 / 2.0f));
			
			kgfw_graphics_mesh_node_t * m = kgfw_graphics_mesh_new(&mesh, NULL);
			ktga_t * tga = texture_get("relay_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			m->transform.scale[0] = tex.width / 16.0f;
			m->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(m, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			storage.current = &storage.gates[storage.gates_count];
			storage.gates[storage.gates_count].type = GATE_RELAY;
			storage.gates[storage.gates_count].left = NULL;
			storage.gates[storage.gates_count].right = NULL;
			storage.gates[storage.gates_count].value = 0;
			storage.gates[storage.gates_count++].mesh = m;
		too_many_relays:;
		}
		if (key == KGFW_KEY_F && storage.current != NULL && storage.current->type != GATE_AND && storage.current->mesh != NULL) {
			if (storage.current->type == GATE_INPUT) {
				storage.current->value = !storage.current->value;
				kgfw_graphics_mesh_texture_detach(storage.current->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
				ktga_t * tga = texture_get((storage.current->value) ? "put_on" : "put_off");
				kgfw_graphics_texture_t tex = {
					tga->bitmap,
					tga->header.img_w, tga->header.img_h,
					KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
					KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
					KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
					KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
				};
				storage.current->mesh->transform.scale[0] = tex.width / 16.0f;
				storage.current->mesh->transform.scale[1] = tex.height / 16.0f;
				kgfw_graphics_mesh_texture(storage.current->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			}
		}
		if (key == KGFW_KEY_1 && storage.current != NULL && storage.current->type != GATE_AND && storage.current->mesh != NULL) {
			storage.current->type = GATE_AND;
			kgfw_graphics_mesh_texture_detach(storage.current->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get("and");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			storage.current->mesh->transform.scale[0] = tex.width / 16.0f;
			storage.current->mesh->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(storage.current->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (key == KGFW_KEY_2 && storage.current != NULL && storage.current->type != GATE_OR && storage.current->mesh != NULL) {
			storage.current->type = GATE_OR;
			kgfw_graphics_mesh_texture_detach(storage.current->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get("or");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			storage.current->mesh->transform.scale[0] = tex.width / 16.0f;
			storage.current->mesh->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(storage.current->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (key == KGFW_KEY_3 && storage.current != NULL && storage.current->type != GATE_XOR && storage.current->mesh != NULL) {
			storage.current->type = GATE_XOR;
			kgfw_graphics_mesh_texture_detach(storage.current->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get("xor");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			storage.current->mesh->transform.scale[0] = tex.width / 16.0f;
			storage.current->mesh->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(storage.current->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (key == KGFW_KEY_4 && storage.current != NULL && storage.current->type != GATE_NOT && storage.current->mesh != NULL) {
			storage.current->type = GATE_NOT;
			kgfw_graphics_mesh_texture_detach(storage.current->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get("not");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			storage.current->mesh->transform.scale[0] = tex.width / 16.0f;
			storage.current->mesh->transform.scale[1] = tex.height / 16.0f;
			kgfw_graphics_mesh_texture(storage.current->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (key == KGFW_KEY_BACKSPACE) {
			if (storage.current != NULL && storage.current->mesh != NULL) {
				/*kgfw_graphics_mesh_destroy(storage.current->mesh);
				storage.current->mesh = NULL;
				storage.current = NULL;*/
			}
		}
	}
}

static gate_t * click(void) {
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

	vec4 dir = { vp[2][0], vp[2][1], vp[2][2], 0 };

	//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "mouse %f %f screen %f %f world %f %f %f", sx, sy, mouse[0], mouse[1], pos[0], pos[1], pos[2]);

	for (unsigned long long int i = 0; i < storage.gates_count; ++i) {
		if (storage.gates[i].mesh == NULL) {
			continue;
		}

		mat4x4 m;
		mat4x4_mul(m, storage.gates[i].mesh->matrices.scale, storage.gates[i].mesh->matrices.rotation);
		mat4x4_mul(m, m, storage.gates[i].mesh->matrices.translation);

		vec3 max = { storage.gates[i].mesh->transform.pos[0] + storage.gates[i].mesh->transform.scale[0], storage.gates[i].mesh->transform.pos[1] + storage.gates[i].mesh->transform.scale[1], storage.gates[i].mesh->transform.pos[2] + storage.gates[i].mesh->transform.scale[2] };
		vec3 min = { storage.gates[i].mesh->transform.pos[0] - storage.gates[i].mesh->transform.scale[0], storage.gates[i].mesh->transform.pos[1] - storage.gates[i].mesh->transform.scale[1], storage.gates[i].mesh->transform.pos[2] - storage.gates[i].mesh->transform.scale[2] };

		//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos %f %f %f %f dir %f %f %f %f", pos[0], pos[1], pos[2], pos[3], dir[0], dir[1], dir[2], dir[3]);
		//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "max %f %f %f min %f %f %f", max[0], max[1], max[2], min[0], min[1], min[2]);
		if ((pos[0] > min[0])) {
			//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos[0] > min[0]");
			if ((pos[0] < max[0])) {
				//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos[0] < max[0]");
				if ((pos[1] > min[1])) {
					//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "pos[1] > min[1]");
					if ((pos[1] < max[1])) {
						//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "hit");
						return &storage.gates[i];
					}
				}
			}
		}
	}

	return NULL;
}

static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action) {
	if (button == KGFW_MOUSE_LBUTTON && action == 1) {
		storage.current = click();
		if (storage.current != NULL && storage.current->mesh != NULL) {
			kgfw_audio_play_sound("click", (storage.current->mesh->transform.pos[0] - state.camera.pos[0]) / 25.0f, (storage.current->mesh->transform.pos[1] - state.camera.pos[1]) / 25.0f, (storage.current->mesh->transform.pos[2] - state.camera.pos[2]) / 25.0f, 1, 1, 0, 1);
			//kgfw_graphics_mesh_texture_detach(storage.current, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		} else {
			
		}
	}
	if (button == KGFW_MOUSE_RBUTTON && action == 1) {
		if (storage.current != NULL && storage.current->mesh != NULL) {
			gate_t * g = click();
			if (g == NULL || g == storage.current || g->type == GATE_OUTPUT || storage.current->type == GATE_INPUT) {
				goto skip_wire;
			}

			wire_t ** w = NULL;
			if (storage.current->type == GATE_NOT || storage.current->type == GATE_RELAY || storage.current->type == GATE_OUTPUT) {
				w = &storage.current->left;
				if (w == NULL || *w != NULL) {
					goto skip_wire;
				}
			} else {
				w = &storage.current->right;
				if (storage.current->left == NULL) {
					w = &storage.current->left;
				} else if (*w != NULL) {
					goto skip_wire;
				}
			}

			if (storage.wires_count > STORAGE_MAX_WIRES) {
				goto skip_wire;
			}
			*w = &storage.wires[storage.wires_count++];
			
			(*w)->start = storage.current;
			(*w)->end = g;

			kgfw_graphics_mesh_node_t * m = kgfw_graphics_mesh_new(&mesh, NULL);
			ktga_t * tga = texture_get("wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};

			float wi = (g->mesh->transform.pos[0] - storage.current->mesh->transform.pos[0]) / 2.0f;
			float he = (g->mesh->transform.pos[1] - storage.current->mesh->transform.pos[1]) / 2.0f;
			float de = (g->mesh->transform.pos[2] - storage.current->mesh->transform.pos[2]) / 2.0f;			float dist = sqrtf(wi * wi + he * he + de * de);
			m->transform.scale[0] = dist;
			m->transform.scale[1] = tex.height / 16.0f;

			float angle = atan2f(g->mesh->transform.pos[1] - storage.current->mesh->transform.pos[1], storage.current->mesh->transform.pos[0] - g->mesh->transform.pos[0]);
			m->transform.rot[2] = -angle * 180.0f / 3.141592f;
			m->transform.pos[0] = storage.current->mesh->transform.pos[0] + wi;
			m->transform.pos[1] = storage.current->mesh->transform.pos[1] + he;
			m->transform.pos[2] = storage.current->mesh->transform.pos[2] + de;
			kgfw_graphics_mesh_texture(m, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			(*w)->mesh = m;
		}
		goto past_skip;
	skip_wire:
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "skipped");
	past_skip:;
	}
}

static int player_movement_init(player_movement_t * self, player_movement_state_t * mstate) {
	return 0;
}

static int player_ortho_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (kgfw_input_mouse_button(KGFW_MOUSE_LBUTTON)) {
		float sx, sy;

		if (storage.current == NULL || storage.current->mesh == NULL) {
			kgfw_input_mouse_delta(&sx, &sy);
			state.camera.pos[0] += sx / 30 * state.camera.scale[0];
			state.camera.pos[1] -= sy / 30 * state.camera.scale[1];
		} else {
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
			storage.current->mesh->transform.pos[0] = pos[0];
			storage.current->mesh->transform.pos[1] = pos[1];
		}
	} else if (storage.current != NULL) {
		storage.current->mesh->transform.pos[0] = round((*storage.current).mesh->transform.pos[0] / (1 / 2.0f)) * (1 / 2.0f);
		storage.current->mesh->transform.pos[1] = round((*storage.current).mesh->transform.pos[1] / (1 / 2.0f)) * (1 / 2.0f);
	}
	
	if (kgfw_input_key(KGFW_KEY_LSHIFT)) {
		state.camera.scale[0] = 2;
		state.camera.scale[1] = 2;
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
		memset(&storage.texture_hashes[i], 0, sizeof(unsigned long long int));
	}
	storage.textures_count = 0;
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

static int game_command(int argc, char ** argv) {


	return 0;
}

static unsigned char evaluate(gate_t * gate) {
	if (storage.evaluation_cycles >= EVALUATION_MAX_CYCLES) {
		return 0;
	}
	++storage.evaluation_cycles;
	if (gate == NULL) {
		return 0;
	}

	if (gate->type != GATE_INPUT && gate->evaluated) {
		return gate->value;
	}

	unsigned char v = 0;
	unsigned char lv = 0;
	unsigned char rv = 0;
	if (gate->type == GATE_INPUT) {
		return gate->value;
	} else if (gate->type == GATE_OUTPUT) {
		if (gate->left == NULL) {
			v = 0;
		} else {
			v = evaluate(gate->left->end);
			if (gate->left->mesh != NULL) {
				kgfw_graphics_mesh_texture_detach(gate->left->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
				ktga_t * tga = texture_get((v) ? "wire_on" : "wire_off");
				kgfw_graphics_texture_t tex = {
					tga->bitmap,
					tga->header.img_w, tga->header.img_h,
					KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
					KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
					KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
					KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
				};
				kgfw_graphics_mesh_texture(gate->left->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			}
		}

		kgfw_graphics_mesh_texture_detach(gate->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		ktga_t * tga = texture_get((v) ? "put_on" : "put_off");
		kgfw_graphics_texture_t tex = {
			tga->bitmap,
			tga->header.img_w, tga->header.img_h,
			KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
			KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
		};
		kgfw_graphics_mesh_texture(gate->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		gate->value = v;
		gate->evaluated = 1;
		return v;
	} else if (gate->type == GATE_AND) {
		wire_t * l = gate->left;
		wire_t * r = gate->right;
		if (l == NULL) {
			lv = 0;
		} else {
			lv = evaluate(l->end);
		}
		if (r == NULL) {
			rv = 0;
		} else {
			rv = evaluate(r->end);
		}
		v = lv && rv;
		if (l != NULL && l->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(l->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((lv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(l->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (r != NULL && r->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(r->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((rv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(r->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		gate->value = v;
		gate->evaluated = 1;
		return v;
	} else if (gate->type == GATE_OR) {
		wire_t * l = gate->left;
		wire_t * r = gate->right;
		if (l == NULL) {
			lv = 0;
		}
		else {
			lv = evaluate(l->end);
		}
		if (r == NULL) {
			rv = 0;
		}
		else {
			rv = evaluate(r->end);
		}
		v = lv || rv;
		if (l != NULL && l->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(l->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((lv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(l->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (r != NULL && r->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(r->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((rv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(r->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		gate->value = v;
		gate->evaluated = 1;
		return v;
	} else if (gate->type == GATE_XOR) {
		wire_t * l = gate->left;
		wire_t * r = gate->right;
		if (l == NULL) {
			lv = 0;
		}
		else {
			lv = evaluate(l->end);
		}
		if (r == NULL) {
			rv = 0;
		}
		else {
			rv = evaluate(r->end);
		}
		v = lv ^ rv;
		if (l != NULL && l->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(l->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((lv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(l->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		if (r != NULL && r->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(r->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((rv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(r->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		gate->value = v;
		gate->evaluated = 1;
		return v;
	}
	else if (gate->type == GATE_NOT) {
		wire_t * l = gate->left;
		if (l == NULL) {
			lv = 0;
		}
		else {
			lv = evaluate(l->end);
		}

		v = !lv;
		if (l != NULL && l->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(l->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((lv) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(l->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
		gate->value = v;
		gate->evaluated = 1;
		return v;
	} else if (gate->type == GATE_RELAY) {
		wire_t * l = gate->left;
		if (l == NULL) {
			v = 0;
		}
		else {
			v = evaluate(l->end);
		}

		if (l != NULL && l->mesh != NULL) {
			kgfw_graphics_mesh_texture_detach(l->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			ktga_t * tga = texture_get((v) ? "wire_on" : "wire_off");
			kgfw_graphics_texture_t tex = {
				tga->bitmap,
				tga->header.img_w, tga->header.img_h,
				KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(l->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}

		kgfw_graphics_mesh_texture_detach(gate->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		ktga_t * tga = texture_get((v) ? "relay_on" : "relay_off");
		kgfw_graphics_texture_t tex = {
			tga->bitmap,
			tga->header.img_w, tga->header.img_h,
			KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
			KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
		};
		kgfw_graphics_mesh_texture(gate->mesh, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		gate->value = v;
		gate->evaluated = 1;
		return v;
	}

	gate->value = v;
	gate->evaluated = 1;
	return v;
}

static void reset_evaluation(gate_t * gate) {
	if (storage.evaluation_cycles >= EVALUATION_MAX_CYCLES) {
		return;
	}
	++storage.evaluation_cycles;
	if (gate == NULL) {
		return;
	}

	if (gate->type != GATE_INPUT) {
		gate->evaluated = 0;
	}

	if (gate->left != NULL) {
		reset_evaluation(gate->left->end);
	}

	if (gate->right != NULL) {
		reset_evaluation(gate->right->end);
	}
}
