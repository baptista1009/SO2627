#ifndef COMMANDS_H
#define COMMANDS_H

#include "datacenter.h"

/**
 * Opens, processes, and closes a command file.
 *
 * @param dc Pointer to the Data Center used to process the commands.
 * @param path Path to the command file.
 *
 * @return 0 if the file was opened and closed successfully.
 * @return 1 if opening or closing the file fails.
 */
int process_file(DataCenter *dc, const char *path);

#endif