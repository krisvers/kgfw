#ifndef KRISVERS_KWAV_H
#define KRISVERS_KWAV_H

typedef struct kwav_header {
	unsigned int filesize;
	unsigned short type;
	unsigned short channels;
	unsigned int rate;
	unsigned short bits;
	unsigned int datasize;
} kwav_header_t;

typedef struct kwav {
	kwav_header_t header;
	void * data;
} kwav_t;

int kwav_load(kwav_t * out_kwav, unsigned char * buffer, unsigned long long int buffer_length);

#endif