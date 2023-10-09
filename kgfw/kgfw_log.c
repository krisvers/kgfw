#include "kgfw_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static kgfw_log_callback_func kgfw_log_callback = NULL;
static kgfw_logc_callback_func kgfw_logc_callback = NULL;

int kgfw_log_register_callback(kgfw_log_callback_func callback) {
	kgfw_log_callback = callback;

	return 0;
}

int kgfw_logc_register_callback(kgfw_logc_callback_func callback) {
	kgfw_logc_callback = callback;

	return 0;
}

void kgfw_logc(kgfw_log_severity_enum severity, char character) {
	if (kgfw_logc_callback != NULL) {
		kgfw_logc_callback(severity, character);
	}
}

void kgfw_log(kgfw_log_severity_enum severity, char * message) {
	if (kgfw_log_callback != NULL) {
		kgfw_log_callback(severity, message);
	}
}

void kgfw_logf(kgfw_log_severity_enum severity, char * format, ...) {
	va_list args;
	va_start(args, format);

	char buffer[1024];

	vsnprintf(buffer, 1024, format, args);
	va_end(args);

	if (kgfw_log_callback != NULL) {
		kgfw_log_callback(severity, buffer);
	}
}
