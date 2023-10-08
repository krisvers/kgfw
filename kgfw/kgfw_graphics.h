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

KGFW_PUBLIC int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera, kgfw_graphics_mesh_t * mesh);
KGFW_PUBLIC void kgfw_graphics_set_window(kgfw_window_t * window);
KGFW_PUBLIC kgfw_window_t * kgfw_graphics_get_window(void);
KGFW_PUBLIC void kgfw_graphics_draw(void);
KGFW_PUBLIC void * kgfw_graphics_mesh_upload(kgfw_graphics_mesh_t * mesh);
KGFW_PUBLIC void kgfw_graphics_deinit(void);

#endif
