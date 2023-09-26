#include <stdio.h>
#include "kgfw/kgfw.h"

int kgfw_log_handler(kgfw_log_severity_enum severity, char * string) {
	severity += 2;
	char * severity_strings[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
	printf("[%s] %s\n", severity_strings[severity % 5], string);

	return 0;
}

int main(void) {
	printf("Hello, World\n");

	kgfw_log_register_callback(kgfw_log_handler);
	if (kgfw_init() != 0) {
		return 1;
	}

	kgfw_window_t window;
	if (kgfw_window_create(&window, 800, 600, "hello, worl") != 0) {
		return 2;
	}

	while (!window.closed) {
		if (kgfw_update() != 0) {
			break;
		}

		if (kgfw_window_update(&window) != 0) {
			break;
		}
	}

	kgfw_window_destroy(&window);

	kgfw_deinit();

	return 0;
}