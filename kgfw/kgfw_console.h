#ifndef KRISVERS_KGFW_CONSOLE_H
#define KRISVERS_KGFW_CONSOLE_H

#include "kgfw_defines.h"

typedef int (*kgfw_console_command_callback)(int argc, char ** argv);

KGFW_PUBLIC int kgfw_console_init(void);
KGFW_PUBLIC void kgfw_console_deinit(void);
KGFW_PUBLIC int kgfw_console_run(int argc, char ** argv);
KGFW_PUBLIC int kgfw_console_register_command(char * name, kgfw_console_command_callback command);
KGFW_PUBLIC void kgfw_console_input_enable(unsigned char enabled);
KGFW_PUBLIC unsigned char kgfw_console_is_input_enabled(void);
KGFW_PUBLIC char * kgfw_console_var(char * name);
KGFW_PUBLIC int kgfw_console_create_var(char * name, char * value);
KGFW_PUBLIC int kgfw_console_set_var(char * name, char * value);

#endif
