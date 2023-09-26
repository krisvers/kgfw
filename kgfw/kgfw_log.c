#include "kgfw_log.h"

static kgfw_log_callback_func kgfw_log_callback = NULL;

int kgfw_log_register_callback(kgfw_log_callback_func callback) {
	kgfw_log_callback = callback;

	return 0;
}

void kgfw_log(kgfw_log_severity_enum severity, char * message) {
	if (kgfw_log_callback != NULL) {
		kgfw_log_callback(severity, message);
	}
}