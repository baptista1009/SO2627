#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "commands.h"
#include "parser.h"
#include "constants.h"


static void process_commands(DataCenter *dc, int fd) {
    while (1) {
        switch (get_next_command(fd)) {
            case CMD_DEFINE: {
                VMType vmtype;

                if (parse_define(fd, &vmtype) != 0) {
                    fprintf(stderr,
                            "Invalid define command. See H (help) for usage.\n");
                    continue;
                }

                if (datacenter_define_VM(dc, &vmtype) != 0) {
                    fprintf(stderr, "Failed to define VM.\n");
                    continue;
                }

                printf("VM successfully defined!\n");
                break;
            }

            case CMD_RESERVE: {
                Reservation reservation = {0};

                size_t num_items = parse_reserve(fd, &reservation, MAX_RESERVATIONS_ITEMS);

                if (num_items == 0) {
                    fprintf(stderr,"Invalid reserve command. See H (help) for usage.\n");
                    continue;
                }

                if (datacenter_reserve(dc, &reservation) != 0) {
                    fprintf(stderr, "Failed to reserve VMs.\n");
                    continue;
                }

                printf("Reservation made successfully!\n");
                break;
            }

            case CMD_EXECUTE: {
                char id[MAX_STRING_SIZE];

                if (parse_execute(fd, id) != 0) {
                    fprintf(stderr, "Invalid execute command. See H (help) for usage.\n");
                    continue;
                }

                if (datacenter_execute(dc, id) != 0) {
                    fprintf(stderr, "Failed to execute reservation.\n");
                    continue;
                }

                printf("Finished reservation execution!\n");
                break;
            }

            case CMD_LIST: {
                if (datacenter_list(dc) != 0) {
                    fprintf(stderr, "Failed to list VMs.\n");
                    continue;
                }

                break;
            }

            case CMD_WAIT: {
                unsigned int delay;

                if (parse_wait(fd, &delay) != 0) {
                    fprintf(stderr, "Invalid wait command. See H (help) for usage.\n");
                    continue;
                }

                datacenter_wait(delay);
                break;
            }

            case CMD_INVALID:
                fprintf(stderr, "Invalid Command. See H (help) for usage.\n");
                break;

            case CMD_HELP:
                printf(
                    "Spaces between arguments are allowed, but not after command end.\n"
                    "Available commands:\n"
                    " D <VM_TYPE_ID> <INPUT_FOLDER> <EXECUTABLE_PATH> <RAM_NEEDED> <DISK_NEEDED> <VCPU_NEEDED_COUNT>\n"
                    " R <RESERVATION_ID> [<VM_TYPE_ID> <COUNT> <SERVER_ID>]+\n"
                    " A <RESERVATION_ID>\n"
                    " L\n"
                    " E <DELAY_MS>\n"
                    " H\n"
                );
                break;

            case CMD_EMPTY:
                break;

            case EOC:
                return;
        }
    }
}

int process_file(DataCenter *dc, const char *path) {
    int fd = open(path, O_RDONLY);

    if (fd == -1) {
        perror("Failed to open configuration file");
        return 1;
    }

    process_commands(dc, fd);

    if (close(fd) == -1) {
        perror("Failed to close configuration file");
        return 1;
    }

    return 0;
}