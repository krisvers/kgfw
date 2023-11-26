#include "kobj.h"
#include <stdlib.h>
#include <string.h>

int kobj_load(kobj_t * out_obj, void * buffer, unsigned long long int length) {
	if (out_obj == NULL || buffer == NULL || length == 0) {
		return 1;
	}

	char * str = buffer;

	unsigned int vindex;
	unsigned int uvindex;
	unsigned int nindex;
	unsigned int findex;
	char c;
	char nextc;
	char * endptr;
	float f;
	unsigned int u;
	unsigned char runthrough = 0;

top:;
	vindex = 0;
	uvindex = 0;
	nindex = 0;
	findex = 0;
	c = 0;
	nextc = 0;
	endptr = NULL;
	f = 0;
	u = 0;

	for (unsigned int i = 0; i < length; ++i) {
		c = str[i];
		nextc = (i + 1 < length) ? str[i + 1] : 0;
		if (c == '#' || c == 'o' == c == 'm' || c == 'u' || c == 'l' || c == 's' || (c == 'v' && nextc == 'p')) {
			while (c != '\n') {
				c = str[++i];
			}
		}
		else if (c == 'v') {
			if (nextc == ' ') {
				if (!runthrough) { ++out_obj->vcount; }
				i += 2;
				f = strtof(&str[i], &endptr);
				if (out_obj->vertices != NULL) {
					out_obj->vertices[vindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				f = strtof(&str[i], &endptr);
				if (out_obj->vertices != NULL) {
					out_obj->vertices[vindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				f = strtof(&str[i], &endptr);
				if (out_obj->vertices != NULL) {
					out_obj->vertices[vindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
			}
			else if (nextc == 'n') {
				if (!runthrough) { ++out_obj->ncount; }
				i += 3;
				f = strtof(&str[i], &endptr);
				if (out_obj->normals != NULL) {
					out_obj->normals[nindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				f = strtof(&str[i], &endptr);
				if (out_obj->normals != NULL) {
					out_obj->normals[nindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				f = strtof(&str[i], &endptr);
				if (out_obj->normals != NULL) {
					out_obj->normals[nindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
			}
			else if (nextc == 't') {
				if (!runthrough) { ++out_obj->uvcount; }
				i += 3;
				f = strtof(&str[i], &endptr);
				if (out_obj->uvs != NULL) {
					out_obj->uvs[uvindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				f = strtof(&str[i], &endptr);
				if (out_obj->uvs != NULL) {
					out_obj->uvs[uvindex++] = f;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
			}
		}
		else if (c == 'f') {
			i += 2;

			u = strtoul(&str[i], &endptr, 10);
			if (out_obj->faces != NULL) {
				out_obj->faces[findex].v1 = u;
			}
			i = ((unsigned long long int) endptr - ((unsigned long long int) str));
			if (str[i] == '/') {
				++i;
				u = strtoul(&str[i], &endptr, 10);
				if (out_obj->faces != NULL) {
					out_obj->faces[findex].vt1 = u;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				if (str[i] == '/') {
					++i;
					u = strtoul(&str[i], &endptr, 10);
					if (out_obj->faces != NULL) {
						out_obj->faces[findex].vn1 = u;
					}
					i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				}
			}

			if (str[i] == ' ') {
				u = strtoul(&str[i], &endptr, 10);
				if (out_obj->faces != NULL) {
					out_obj->faces[findex].v2 = u;
				}
				i = ((unsigned long long int) endptr - ((unsigned long long int) str));
				if (str[i] == '/') {
					++i;
					u = strtoul(&str[i], &endptr, 10);
					if (out_obj->faces != NULL) {
						out_obj->faces[findex].vt2 = u;
					}
					i = ((unsigned long long int) endptr - ((unsigned long long int) str));
					if (str[i] == '/') {
						++i;
						u = strtoul(&str[i], &endptr, 10);
						if (out_obj->faces != NULL) {
							out_obj->faces[findex].vn2 = u;
						}
						i = ((unsigned long long int) endptr - ((unsigned long long int) str));
					}
				}

				if (str[i] == ' ') {
					u = strtoul(&str[i], &endptr, 10);
					if (out_obj->faces != NULL) {
						out_obj->faces[findex].v3 = u;
					}
					i = ((unsigned long long int) endptr - ((unsigned long long int) str));
					if (str[i] == '/') {
						++i;
						u = strtoul(&str[i], &endptr, 10);
						if (out_obj->faces != NULL) {
							out_obj->faces[findex].vt3 = u;
						}
						i = ((unsigned long long int) endptr - ((unsigned long long int) str));
						if (str[i] == '/') {
							++i;
							u = strtoul(&str[i], &endptr, 10);
							if (out_obj->faces != NULL) {
								out_obj->faces[findex].vn3 = u;
							}
							i = ((unsigned long long int) endptr - ((unsigned long long int) str));
						}
					}
				}
			}
			++findex;
			if (!runthrough) { ++out_obj->fcount; }
		}
	}

	if (out_obj->vertices == NULL) {
		out_obj->vertices = malloc(out_obj->vcount * 3 * sizeof(float));
		memset(out_obj->vertices, 0, out_obj->vcount * 3 * sizeof(float));
	}
	if (out_obj->normals == NULL) {
		out_obj->normals = malloc(out_obj->ncount * 3 * sizeof(float));
		memset(out_obj->normals, 0, out_obj->ncount * 3 * sizeof(float));
	}
	if (out_obj->uvs == NULL) {
		out_obj->uvs = malloc(out_obj->uvcount * 2 * sizeof(float));
		memset(out_obj->uvs, 0, out_obj->uvcount * 2 * sizeof(float));
	}
	if (out_obj->faces == NULL) {
		out_obj->faces = malloc(out_obj->fcount * sizeof(kobj_face_t));
		memset(out_obj->faces, 0, out_obj->fcount * sizeof(kobj_face_t));
		runthrough = 1;
		goto top;
	}

	return 0;
}

void kobj_destroy(kobj_t * obj) {
	if (obj->vertices != NULL) {
		free(obj->vertices);
	}
	if (obj->normals != NULL) {
		free(obj->normals);
	}
	if (obj->uvs != NULL) {
		free(obj->uvs);
	}
	if (obj->faces != NULL) {
		free(obj->faces);
	}
}