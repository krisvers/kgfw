#ifndef KRISVERS_KGFW_KTGA_H
#define KRISVERS_KGFW_KTGA_H

#include "../kgfw_defines.h"

typedef struct ktga_header {
	unsigned char id_len;
	unsigned char color_map_type;
	unsigned char img_type;
	unsigned short color_map_origin;
	unsigned short color_map_length;
	unsigned char color_map_depth;
	unsigned short img_x_origin;
	unsigned short img_y_origin;
	unsigned short img_w;
	unsigned short img_h;
	unsigned char bpp;
	unsigned char img_desc;
} ktga_header_t;

typedef struct ktga {
	ktga_header_t header;
	void * bitmap;
} ktga_t;

KGFW_PUBLIC int ktga_load(ktga_t * out_tga, void * buffer, unsigned long long int buffer_length);
KGFW_PUBLIC void ktga_destroy(ktga_t * tga);

#endif