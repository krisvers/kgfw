#ifndef KRISVERS_KGFW_LOG_H
#define KRISVERS_KGFW_LOG_H

#include "kgfw_defines.h"

typedef enum kgfw_log_severity {
	KGFW_LOG_SEVERITY_TRACE = -2,
	KGFW_LOG_SEVERITY_DEBUG = -1,
	KGFW_LOG_SEVERITY_INFO = 0,
	KGFW_LOG_SEVERITY_WARN,
	KGFW_LOG_SEVERITY_ERROR,
} kgfw_log_severity_enum;

typedef int (*kgfw_log_callback_func)(kgfw_log_severity_enum severity, char * string);

KGFW_PUBLIC int kgfw_log_register_callback(kgfw_log_callback_func callback);
KGFW_PUBLIC void kgfw_log(kgfw_log_severity_enum severity, char * message);

#endif