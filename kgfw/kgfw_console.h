#ifndef KRISVERS_KGFW_CONSOLE_H
#define KRISVERS_KGFW_CONSOLE_H

#include "kgfw_defines.h"

typedef int (*kgfw_console_command_callback)(int argc, char ** argv);

KGFW_PUBLIC int kgfw_console_init(void);
KGFW_PUBLIC int kgfw_console_run(int argc, char ** argv);
KGFW_PUBLIC int kgfw_console_register_command(char * name, kgfw_console_command_callback command);

#endif