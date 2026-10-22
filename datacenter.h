#ifndef DATACENTER__H
#define DATACENTER__H

#include <stdio.h>
#include <sys/types.h>

#include "resources.h"
#include "constants.h"
#include "filesystem.h"

typedef struct VMType{
  char id[MAX_STRING_SIZE];        // Id of VM type. eg: "V1"
  char input_folder[MAX_PATH_SIZE];  // Folder with input for executable
  char exec_path[MAX_PATH_SIZE];     // Path for executable
  Resources required;
} VMType;

typedef enum {
  VM_STATE_RESERVED,  // VM reserved; waiting for the 'A' command.
  VM_STATE_RUNNING,   // Child process created; PID recorded.
  VM_STATE_TERMINATED // Child process collected using waitpid().
} VMState;

typedef struct Server Server;

typedef struct VM {
  char id[MAX_VM_ID_STRING]; // VM id. eg: "R1_V1_1"
  VMType *type;   // VM type.
  Server *server; // Server where the VM is placed.
  pid_t pid; // PID of the child process running cpulimit; valid after launch.
  VMState state;  // State of the VM.
} VM;

typedef struct Server {
	size_t id;		     // Server ID (1, 2, 3, ...)
	Resources total;     // Resources that Server has.
	Resources available; // Resources still available.

	VM *hosted_vms[MAX_HOSTED_VMS];
    size_t num_hosted_vms;
} Server;

typedef enum {
    RES_STATE_PENDING,   // Reserved via 'R', waiting for 'A'
    RES_STATE_RUNNING,   // 'A' was called
    RES_STATE_FINISHED   // All VMs in this reservation have terminated
} ReservationState;

typedef struct ReservationPair {
    char vm_type_id[MAX_STRING_SIZE]; // VM type. e.g: "V1"       
    size_t amount;                    // Amount of VMs with the given type being reserved.
    size_t server_id;                 // Server where each VM will be running on.

    VMType *vm_type;                   // Used for search optimization.
} ReservationItem;

typedef struct Reservation {
	char id[MAX_STRING_SIZE];                       // id of reservation.

	ReservationItem items[MAX_RESERVATIONS_ITEMS];  // List of reservations items 
    size_t items_count;

	VM *vms[MAX_RESERVATION_VMS];                   // List of VMs in this reservation.
	size_t num_vms;                                 // Number of VMs in this reservation.
	ReservationState state;                         // State of the reservation.
} Reservation;

typedef struct DataCenter {
	Server *servers;			     // List of Servers.
	size_t num_servers;              // Number of Servers.

	VMType vm_types[MAX_VM_TYPES];
    size_t num_vm_types;

	Reservation reservations[MAX_RESERVATIONS];
    size_t num_reservations;

	int configured;		      // Wheter the datacenter is configured or not.

    pthread_mutex_t lock;
} DataCenter;

/**
 * Initializes a Data Center.
 *
 * @param dc DataCenter being initialized.
 */
void datacenter_init(DataCenter *dc);

/**
 * Waits for all remaining VM child processes and frees
 * the memory allocated by the Data Center.
 *
 * @param dc Pointer to the Data Center being destroyed.
 */
void datacenter_destroy(DataCenter *dc);

/**
 * Configures the Data Center with the specified number of physical
 * servers, assigning the same resources to each server.
 *
 * @param dc Pointer to the Data Center being configured.
 * @param num_pcs Number of physical servers.
 * @param resources Pointer to the resources assigned to each server.
 *
 * @return 0 on success.
 * @return 1 on error.
 */
int datacenter_configure(DataCenter *dc, size_t num_pcs, Resources *resources);

/**
 * Defines a VM for the Data Center
 *
 * @param dc Pointer to a Data Center.
 * @param type Pointer to VM type.
 * 
 * @return 0 on success.
 * @return 1 on error.
 */
int datacenter_define_VM(DataCenter *dc, struct VMType *type);

/**
 * Reserves a set of VMs for a given reservation.
 *
 * @param dc Pointer to a Data Center.
 * @param reservation Pointer to a Reservation.
 * 
 * @return 0 on success.
 * @return 1 on error.
 */
int datacenter_reserve(DataCenter *dc, Reservation *reservation);

/**
 * Lists the current state of the Data Center.
 *
 * @param dc Pointer to a Data Center.
 * 
 * @return 0 on success.
 * @return 1 on error.
 */
int datacenter_list(DataCenter *dc);

/**
 * Launches one child process per VM in the reservation.
 * Each child prepares its input directory before running the application.
 * Returns without waiting for the children to finish.
 *
 * @param dc Pointer to the Data Center.
 * @param reservation_id ID of the reservation to execute.
 *
 * @return 0 if all child processes were created successfully.
 * @return 1 if the reservation cannot be launched.
 */
int datacenter_execute(DataCenter *dc, const char *reservation_id);
/**
 * Pauses command processing while collecting terminated VMs.
 *
 * @param dc Pointer to the Data Center.
 * @param delay_ms Delay in milliseconds.
 */
void datacenter_wait(DataCenter *dc, unsigned int delay_ms);

#endif // DATACENTER__H