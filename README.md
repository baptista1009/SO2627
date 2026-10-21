# CloudIST

CloudIST simulates a data center that manages physical servers,
VM types, reservations, and VM application execution.

This version implements exercises 1 and 2.

## Build and run

Build the program with `make`.

CPU limiting requires `cpulimit` to be installed and available in PATH.

Usage:

`./cloudIST <SERVERS> <RAM> <DISK> <CPU> <INPUT_DIR>`

Example:

`./cloudIST 2 8 100 4 configuracoes`

The program processes regular `.conf` files from INPUT_DIR
in alphabetical order.

RAM and disk values are expressed in GB.

## Commands

- `D <TYPE> <INPUT_DIR> <EXEC_PATH> <RAM> <DISK> <VCPU>`:
  define a VM type.
- `R <RESERVATION_ID> [<TYPE> <COUNT> <SERVER_ID>]+`:
  reserve VMs on the specified servers.
- `A <RESERVATION_ID>`:
  launch the reservation's VMs in separate processes.
- `L`:
  list the current Data Center state.
- `E <DELAY_MS>`:
  pause command processing for the specified number of milliseconds.
- `H`:
  display command help.

## VM execution

Before launching a VM, the program recursively copies its inputs
to `/tmp/CloudIST/<RESERVATION_ID>/<VM_ID>`.

The application runs in that directory, with memory and file size
limits configured using setrlimit and CPU usage limited using cpulimit.

The main process continues processing commands while applications run.
It checks for terminated VM processes between commands and periodically
during E commands, returning their allocated resources.

A reservation is removed when its last VM is collected.
Before exiting, the program waits for any remaining VM processes.

## Project structure

- `main.c`: command-line configuration and configuration file processing.
- `commands.c` / `commands.h`: command dispatch and command file processing.
- `parser.c` / `parser.h`: parsing command arguments and command-line values.
- `datacenter.c` / `datacenter.h`: Data Center operations and state.
- `datacenter_utils.c` / `datacenter_utils.h`: reservation helpers,
  process creation, process collection, and resource release.
- `filesystem.c` / `filesystem.h`: configuration file discovery,
  directory creation, recursive copying, and path utilities.
- `resources.c` / `resources.h`: CPU, RAM, and disk resource operations.
- `constants.h`: project limits.

## Clean

Run `make clean` to remove the executable and object files.