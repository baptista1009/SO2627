#ifndef COMMANDS_H
#define COMMANDS_H

#include "datacenter.h"

/*
 * Opens, processes, and closes a command file.
 * Returns 0 on success and 1 if opening
 * or closing the file fails.
 */
int process_file(DataCenter *dc, const char *path);

#endif