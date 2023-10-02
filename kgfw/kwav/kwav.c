#include "kwav.h"
#include <string.h>

int kwav_load(kwav_t * out_kwav, unsigned char * buffer, unsigned long long int buffer_length) {
	if (out_kwav == NULL || buffer == NULL || buffer_length < 50) {
		return -1;
	}

	char * sig = (char *) &buffer[0];
	if (sig[0] != 'R' || sig[1] != 'I' || sig[2] != 'F' || sig[3] != 'F') {
		return -2;
	}
	sig = (char *) &buffer[8];
	if (sig[0] != 'W' || sig[1] != 'A' || sig[2] != 'V' || sig[3] != 'E') {
		return -2;
	}

	out_kwav->header.filesize = *((unsigned int *) &buffer[4]);
	out_kwav->header.type = *((unsigned short *) &buffer[20]);
	out_kwav->header.channels = *((unsigned short *) &buffer[22]);
	out_kwav->header.rate = *((unsigned int *) &buffer[24]);
	out_kwav->header.bits = *((unsigned short *) &buffer[34]);
	out_kwav->header.datasize = *((unsigned int *) &buffer[40]);

	if (out_kwav->data == NULL) {
		return 0;
	}

	memcpy(out_kwav->data, ((void *) &buffer[44]), out_kwav->header.datasize);

	return 0;
}