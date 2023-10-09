#ifndef KRISVERS_KGFW_LOG_H
#define KRISVERS_KGFW_LOG_H

#include "kgfw_defines.h"

typedef enum kgfw_log_severity {
	KGFW_LOG_SEVERITY_CONSOLE = 0,
	KGFW_LOG_SEVERITY_TRACE,
	KGFW_LOG_SEVERITY_DEBUG,
	KGFW_LOG_SEVERITY_INFO,
	KGFW_LOG_SEVERITY_WARN,
	KGFW_LOG_SEVERITY_ERROR,
} kgfw_log_severity_enum;

typedef int (*kgfw_log_callback_func)(kgfw_log_severity_enum severity, char * string);
typedef int (*kgfw_logc_callback_func)(kgfw_log_severity_enum severity, char character);

KGFW_PUBLIC int kgfw_log_register_callback(kgfw_log_callback_func callback);
KGFW_PUBLIC int kgfw_logc_register_callback(kgfw_logc_callback_func callback);
KGFW_PUBLIC void kgfw_log(kgfw_log_severity_enum severity, char * message);
KGFW_PUBLIC void kgfw_logc(kgfw_log_severity_enum severity, char character);
KGFW_PUBLIC void kgfw_logf(kgfw_log_severity_enum severity, char * format, ...);

#endif
