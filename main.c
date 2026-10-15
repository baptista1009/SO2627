#include <stdio.h>

#include "parser.h"
#include "datacenter.h"
#include "commands.h"
#include "filesystem.h"


int main(int argc, char **argv){
	DataCenter dc;
	datacenter_init(&dc);

	if (argc != 6) {
    fprintf(stderr, "Usage: %s <servers> <ram> <disk> <cpus>\n", argv[0]);
    return 1;
  }

	size_t servers;
	size_t ram;
	size_t disk;
	double cpu;

	if (parse_size_t_arg(argv[1], &servers) != 0 ||
			parse_size_t_arg(argv[2], &ram) != 0 ||
			parse_size_t_arg(argv[3], &disk) != 0 ||
			parse_double_arg(argv[4], &cpu) != 0) {
		fprintf(stderr, "Invalid command line arguments.\n");
		return 1;
	}

	Resources resources = {
    .ram = ram,
    .disk = disk,
    .cpu = cpu
	};

	if(datacenter_configure(&dc, servers, &resources) != 0){
		fprintf(stderr, "Failed to configure Data Center.\n");
		return 1;
	
	}

 	ConfigFileList files = {0};

    if (list_config_files(argv[5], &files) != 0) {
        datacenter_destroy(&dc);
        return 1;
    }

    int result = 0;

    for (size_t i = 0; i < files.count; i++) {
        printf("Processing: %s\n", files.paths[i]);

        if (process_file(&dc, files.paths[i]) != 0) {
            result = 1;
            break;
        }
    }

    free_config_files(&files);
    datacenter_destroy(&dc);
	return result;
}