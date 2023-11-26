#ifndef KRISVERS_KOBJ_H
#define KRISVERS_KOBJ_H

#include "../kgfw_defines.h"

typedef struct kobj_face {
	unsigned int v1, v2, v3;
	unsigned int vt1, vt2, vt3;
	unsigned int vn1, vn2, vn3;
} kobj_face_t;

typedef struct kobj {
	float * vertices;
	float * normals;
	float * uvs;
	kobj_face_t * faces;
	unsigned int vcount;
	unsigned int uvcount;
	unsigned int ncount;
	unsigned int fcount;
} kobj_t;

int kobj_load(kobj_t * out_obj, void * buffer, unsigned long long int length);
void kobj_destroy(kobj_t * obj);

#endif