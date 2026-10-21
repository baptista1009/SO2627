#include "datacenter_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#include <math.h>

VMType *VMType_exists(DataCenter *dc, const char* type_id){
	for(size_t i = 0; i < dc->num_vm_types; i++){
		if(strncmp(type_id, dc->vm_types[i].id, MAX_STRING_SIZE) == 0){
			return &dc->vm_types[i];
		}
	}
	return NULL;
}

/**
 * Checks if a reservation with the given id already exists in the Data Center.
 * 
 * @param dc Pointer to Data Center.
 * @param reservation_id Id of the reservation being checked.
 * 
 * @return 1 if the reservation exists.
 * @return 0 if the reservation does not exist.
 */	
static int reservation_exists(DataCenter *dc, const char* reservation_id){
	for(size_t i = 0; i < dc->num_reservations; i++){
		if(strncmp(reservation_id, dc->reservations[i].id, MAX_STRING_SIZE) == 0){
			return 1;
		}
	}
	return 0;
}

int reservation_validate(DataCenter *dc, Reservation *reservation) {
	if(reservation_exists(dc, reservation->id)){
		fprintf(stderr, "Reservation with id %s already exists.\n", reservation->id);
		return 1;
	}

	Resources available[dc->num_servers];
	size_t hosted_count[dc->num_servers];
	size_t vms_count = 0;

	/* Copy current server resources and hosted_vms so we can simulate the allocation. */
	for (size_t i = 0; i < dc->num_servers; i++) {
		available[i] = dc->servers[i].available;
		hosted_count[i] = dc->servers[i].num_hosted_vms;
	}

	for (size_t i = 0; i < reservation->items_count; i++) {
		ReservationItem *item = &reservation->items[i];

		if (item->amount == 0) {
			fprintf(stderr, "Reservation item must request at least one VM.\n");
			return 1;
		}

		VMType *vm_type = VMType_exists(dc, item->vm_type_id);

		if(vm_type == NULL){
			fprintf(stderr, "VM of type %s does not exist\n", item->vm_type_id);
			return 1;
		}

		if (item->server_id == 0 || item->server_id > dc->num_servers) {
			fprintf(stderr, "Invalid server in reservation.\n");
			return 1;
		}

		for (size_t j = 0; j < item->amount; j++) {
			if(vms_count >= MAX_RESERVATION_VMS){
				fprintf(stderr, "Reached max of VMS for a reservation.\n");
				return 1;
			}
			
			if (hosted_count[item->server_id - 1] >= MAX_HOSTED_VMS) {
        fprintf(stderr,
                "Server \"%zu\" reached the maximum number of hosted VMs.\n",
                item->server_id);
        return 1;
    	}

			if (!resources_can_fit(available[item->server_id-1], vm_type->required)) {
				fprintf(stderr, "Server \"%zu\" does not have enough resources.\n", item->server_id);
				return 1;
			}

			resources_sub(&available[item->server_id-1], vm_type->required);
			hosted_count[item->server_id-1]++;
			vms_count++; // One VM "reserved"
		}

		// Attach vm_type to item for later use.
		item->vm_type = vm_type;
	}

	return 0;
}

/**
 * Rolls back the changes made by a reservation operation.
 * 
 * @param reservation Pointer to the reservation being rolled back.
 * @param initial_vms Number of VMs that existed before the operation.
 */
static void reservation_rollback(Reservation *reservation, size_t initial_vms){
	while (reservation->num_vms > initial_vms) {
		VM *vm = reservation->vms[reservation->num_vms - 1];

		Server *server = vm->server;

		// Restore resources
		resources_add(&server->available, vm->type->required);

		// Remove VM from server hosted list
		server->num_hosted_vms--;

		server->hosted_vms[server->num_hosted_vms] = NULL;

		// Remove VM from reservation
		reservation->num_vms--;

		reservation->vms[reservation->num_vms] = NULL;

		free(vm);
	}
}

int reservation_commit(DataCenter *dc, Reservation *reservation){
	// Keep track of how many vms existed before.
	size_t initial_vms = reservation->num_vms;

	for (size_t i = 0; i < reservation->items_count; i++) {
		ReservationItem *item = &reservation->items[i];
		Server *server = &dc->servers[item->server_id - 1];

		for (size_t j = 0; j < item->amount; j++) {
			VM *new_vm = calloc(1, sizeof(VM));
			if (new_vm == NULL) {
				fprintf(stderr, "Failed to allocate VM.\n");
				reservation_rollback(reservation, initial_vms);
				return 1;
			}

			snprintf(new_vm->id,
				MAX_VM_ID_STRING,
				"%s_%zu",
				reservation->id,
				reservation->num_vms + 1
			);

			new_vm->type = item->vm_type;
			new_vm->server = server;
			new_vm->state = VM_STATE_RESERVED;

			resources_sub(&server->available, item->vm_type->required);

			server->hosted_vms[server->num_hosted_vms++] = new_vm;
			reservation->vms[reservation->num_vms++] = new_vm;
		}
	}

	reservation->state = RES_STATE_PENDING;

	return 0;
}

Reservation *find_pending_reservation(DataCenter *dc, const char *reservation_id) {
	Reservation *res = NULL;

	for (size_t i = 0; i < dc->num_reservations; i++) {
		if (strcmp(dc->reservations[i].id, reservation_id) == 0) {
			res = &dc->reservations[i];
			break;
		}
	}

	if (!res) {
		fprintf(stderr, "Reservation ID %s not found.\n", reservation_id);
		return NULL;
	}

	if (res->state != RES_STATE_PENDING) {
		fprintf(stderr, "Reservation ID %s is not in a pending state.\n", reservation_id);
		return NULL;
	}

	return res;
}

static void reservation_destroy_vm(Reservation *reservation, size_t vm_index) {
    VM *vm = reservation->vms[vm_index];
    Server *server = vm->server;

    resources_add(&server->available, vm->type->required);

    for (size_t j = 0; j < server->num_hosted_vms; j++) {
        if (server->hosted_vms[j] == vm) {
            memmove(&server->hosted_vms[j], &server->hosted_vms[j + 1], (server->num_hosted_vms - j - 1) * sizeof(VM *));

            server->num_hosted_vms--;
            server->hosted_vms[server->num_hosted_vms] = NULL;
            break;
        }
    }

    memmove(&reservation->vms[vm_index], &reservation->vms[vm_index + 1], (reservation->num_vms - vm_index - 1) * sizeof(VM *)
    );

    reservation->num_vms--;
    reservation->vms[reservation->num_vms] = NULL;

    free(vm);
}

void reservation_destroy(DataCenter *dc, Reservation *reservation) {
    while (reservation->num_vms > 0) {
        reservation_destroy_vm(reservation, reservation->num_vms - 1);
    }

    size_t idx;

    for (idx = 0; idx < dc->num_reservations; idx++) {
        if (&dc->reservations[idx] == reservation) {
            break;
        }
    }

    if (idx == dc->num_reservations) {
        return;
    }

    memmove(&dc->reservations[idx], &dc->reservations[idx + 1], (dc->num_reservations - idx - 1) * sizeof(Reservation));

    dc->num_reservations--;
}

static void finish_vm(DataCenter *dc, pid_t pid) {
    for (size_t i = 0; i < dc->num_reservations; i++) {
        Reservation *res = &dc->reservations[i];

        for (size_t j = 0; j < res->num_vms; j++) {
            VM *vm = res->vms[j];

            if (vm->pid != pid) {
                continue;
            }

            vm->state = VM_STATE_TERMINATED;
            reservation_destroy_vm(res, j);

            if (res->num_vms == 0) {
                res->state = RES_STATE_FINISHED;
                reservation_destroy(dc, res);
            }

            return;
        }
    }
}

void reap_finished_vms(DataCenter *dc) {
    while (1) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);

        if (pid > 0) {
            finish_vm(dc, pid);
            continue;
        }

        if (pid == 0) {
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno != ECHILD) {
            perror("Failed to check VM processes");
        }

        return;
    }
}

void spawn_vm_child(VM *vm, const char *reservation_id, size_t num_servers) {
    char exec_path[MAX_PATH_SIZE];

    if (absolute_path(vm->type->exec_path, exec_path, sizeof(exec_path)) != 0) {
        fprintf(stderr, "Failed to resolve executable path.\n");
        return;
    }

    char vm_path[MAX_PATH_SIZE];

    int length = snprintf(vm_path, sizeof(vm_path), "/tmp/CloudIST/%s/%s", reservation_id, vm->id);

    if (length < 0 || (size_t)length >= sizeof(vm_path)) {
        fprintf(stderr, "VM directory path is too long.\n");
        return;
    }

    if (chdir(vm_path) == -1) {
        perror("Failed to change to VM directory");
        return;
    }

    const rlim_t bytes_per_gb = (rlim_t)1024 * 1024 * 1024;

    struct rlimit ram_limit;
    ram_limit.rlim_cur =
        (rlim_t)vm->type->required.ram * bytes_per_gb;
    ram_limit.rlim_max = ram_limit.rlim_cur;

    if (setrlimit(RLIMIT_AS, &ram_limit) == -1) {
        perror("Failed to set VM memory limit");
        return;
    }

    struct rlimit file_limit;
    file_limit.rlim_cur =
        (rlim_t)vm->type->required.disk * bytes_per_gb;
    file_limit.rlim_max = file_limit.rlim_cur;

    if (setrlimit(RLIMIT_FSIZE, &file_limit) == -1) {
        perror("Failed to set VM file size limit");
        return;
    }

    double percentage = ceil(vm->type->required.cpu / vm->server->total.cpu * (double)num_servers * 100.0);

    char cpu_limit[32];

    length = snprintf(cpu_limit, sizeof(cpu_limit), "%.0f", percentage);

    if (length < 0 || (size_t)length >= sizeof(cpu_limit)) {
        fprintf(stderr, "CPU limit value is too long.\n");
        return;
    }

    execlp(
        "cpulimit",
        "cpulimit",
        "-q",
        "-f",
        "-l",
        cpu_limit,
        "--",
        exec_path,
        (char *)NULL
    );

    perror("Failed to execute cpulimit");
}

int spawn_all_vms(Reservation *res, size_t num_servers) {
    for (size_t i = 0; i < res->num_vms; i++) {
        VM *vm = res->vms[i];
        pid_t pid = fork();

        if (pid == -1) {
            perror("Failed to create VM process");

            for (size_t j = 0; j < i; j++) {
                VM *started_vm = res->vms[j];
                pid_t result;

                do {
                    result = waitpid(started_vm->pid, NULL, 0);
                } while (result == -1 && errno == EINTR);

                if (result == -1) {
                    perror("Failed to wait for VM process");
                }

                started_vm->pid = 0;
                started_vm->state = VM_STATE_RESERVED;
            }

            return 1;
        }

        if (pid == 0) {
            spawn_vm_child(vm, res->id, num_servers);
            _exit(1);
        }

        vm->pid = pid;
        vm->state = VM_STATE_RUNNING;
    }

    return 0;
}

void wait_for_all_vms(DataCenter *dc) {
    while (1) {
        pid_t pid = waitpid(-1, NULL, 0);

        if (pid > 0) {
            finish_vm(dc, pid);
            continue;
        }

        if (pid == -1 && errno == EINTR) {
            continue;
        }

        if (pid == -1 && errno != ECHILD) {
            perror("Failed to wait for VM processes");
        }

        return;
    }
}

const char *vm_state_to_string(VMState state) {
    switch (state) {
        case VM_STATE_RESERVED:   return "RESERVED";
        case VM_STATE_RUNNING:    return "RUNNING";
        case VM_STATE_TERMINATED: return "TERMINATED";
        default:                  return "UNKNOWN";
    }
}

const char *res_state_to_string(ReservationState state) {
    switch (state) {
			case RES_STATE_PENDING:  return "PENDING";
			case RES_STATE_RUNNING:  return "RUNNING";
			case RES_STATE_FINISHED: return "FINISHED";
			default:                 return "UNKNOWN";
    }
}