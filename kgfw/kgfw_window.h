#ifndef KRISVERS_KGFW_WINDOW_H
#define KRISVERS_KGFW_WINDOW_H

#include "kgfw_defines.h"

typedef struct kgfw_window {
	void * internal;
	unsigned char closed;
	unsigned char focused;
	unsigned char disable_gamepad_on_unfocus;
	unsigned int width;
	unsigned int height;
} kgfw_window_t;

KGFW_PUBLIC int kgfw_window_create(kgfw_window_t * out_window, unsigned int width, unsigned int height, char * title);
KGFW_PUBLIC void kgfw_window_destroy(kgfw_window_t * window);
KGFW_PUBLIC int kgfw_window_update(kgfw_window_t * window);

#endif
