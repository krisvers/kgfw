#ifndef KRISVERS_KGFW_CAMERA_H
#define KRISVERS_KGFW_CAMERA_H

#include "kgfw_defines.h"

typedef struct kgfw_camera {
	struct {
		float x, y, z;
	} pos;

	struct {
		float x, y, z;
	} rot;

	float fov;
	float nplane;
	float fplane;
	unsigned char ortho;
} kgfw_camera_t;

#endif