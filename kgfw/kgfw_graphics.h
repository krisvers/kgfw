#ifndef KRISVERS_KGFW_GRAPHICS_H
#define KRISVERS_KGFW_GRAPHICS_H

#include "kgfw_defines.h"
#include "kgfw_window.h"
#include "kgfw_camera.h"

typedef struct kgfw_graphics_vertex {
	float x, y, z;
	float r, g, b;
	float nx, ny, nz;
	float u, v;
} kgfw_graphics_vertex_t;

typedef struct kgfw_graphics_mesh {
	kgfw_graphics_vertex_t * vertices;
	unsigned long long int vertices_count;
	unsigned int * indices;
	unsigned long long int indices_count;

	float pos[3];
	float rot[3];
	float scale[3];
} kgfw_graphics_mesh_t;

typedef struct kgfw_graphics_mesh_node {
	struct {
		float pos[3];
		float rot[3];
		float scale[3];
		unsigned char absolute;
	} transform;

	struct kgfw_graphics_mesh_node * parent;
	struct kgfw_graphics_mesh_node * child;
	struct kgfw_graphics_mesh_node * sibling;

	struct {
		unsigned int _a;
		unsigned int _b;
		unsigned int _c;
		unsigned int _d;

		unsigned long long int _e;
		unsigned long long int _f;
	} _internal;
} kgfw_graphics_mesh_node_t;

typedef enum kgfw_graphics_settings_action {
	KGFW_GRAPHICS_SETTINGS_ACTION_SET = 1,
	KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE,
	KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE,
} kgfw_graphics_settings_action_enum;

typedef enum kgfw_graphics_settings {
	KGFW_GRAPHICS_SETTINGS_VSYNC = 1,
} kgfw_graphics_settings_enum;

#define KGFW_GRAPHICS_SETTINGS_DEFAULT (0)

KGFW_PUBLIC int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera);
KGFW_PUBLIC void kgfw_graphics_set_window(kgfw_window_t * window);
KGFW_PUBLIC kgfw_window_t * kgfw_graphics_get_window(void);
KGFW_PUBLIC void kgfw_graphics_draw(void);
KGFW_PUBLIC void kgfw_graphics_viewport(unsigned int width, unsigned int height);
KGFW_PUBLIC kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent);
KGFW_PUBLIC void kgfw_graphics_deinit(void);
KGFW_PUBLIC void kgfw_graphics_settings_set(kgfw_graphics_settings_action_enum action, unsigned int settings);
KGFW_PUBLIC unsigned int kgfw_graphics_settings_get(void);

#endif
