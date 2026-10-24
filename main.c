#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "parser.h"
#include "datacenter.h"
#include "commands.h"
#include "filesystem.h"

size_t g_local_cores = 1;

typedef struct {
	DataCenter *dc;
	ConfigFileList *files;
	size_t current_index;
	pthread_mutex_t mutex;
	int result;
} ThreadPoolContext;


// Função executada por cada worker thread
static void *worker_thread(void *arg) {
    ThreadPoolContext *ctx = (ThreadPoolContext *)arg;

    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->files->count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        size_t index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);

        printf("Processing: %s\n", ctx->files->paths[index]);

        if (process_file(ctx->dc, ctx->files->paths[index]) != 0) {
            pthread_mutex_lock(&ctx->mutex);
            ctx->result = 1;
            pthread_mutex_unlock(&ctx->mutex);
        }
    }
    return NULL;
}


int main(int argc, char **argv){
	DataCenter dc;
	datacenter_init(&dc);

    if (argc != 7 && argc != 8) {
		fprintf(stderr, "Usage: %s <servers> <ram> <disk> <cpus> <local_cores> <INPUT_DIR> [<MAX_THREADS>]\n", argv[0]);
    return 1;
}

	size_t servers;
	size_t ram;
	size_t disk;
	double cpu;
    size_t local_cores;
	size_t max_threads = 1;

	if (parse_size_t_arg(argv[1], &servers) != 0 ||
			parse_size_t_arg(argv[2], &ram) != 0 ||
			parse_size_t_arg(argv[3], &disk) != 0 ||
			parse_double_arg(argv[4], &cpu) != 0 ||
			parse_size_t_arg(argv[5], &local_cores) != 0 ||
			local_cores == 0 ||
			(argc == 8 && parse_size_t_arg(argv[7], &max_threads) != 0)) {
		fprintf(stderr, "Invalid command line arguments.\n");
		return 1;
	}
 
	g_local_cores = local_cores;

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

    if (list_config_files(argv[6], &files) != 0) {
        datacenter_destroy(&dc);
        return 1;
    }

    // Inicialização do contexto do pool de threads
    ThreadPoolContext ctx;
    ctx.dc = &dc;
    ctx.files = &files;
    ctx.current_index = 0;
    ctx.result = 0;
    pthread_mutex_init(&ctx.mutex, NULL);

    // O número de threads não precisa de ser superior ao número de ficheiros
    size_t num_threads = (max_threads < files.count) ? max_threads : files.count;
    if (num_threads == 0) {
        num_threads = 1;
    }

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        perror("Failed to allocate threads array");
        pthread_mutex_destroy(&ctx.mutex);
        free_config_files(&files);
        datacenter_destroy(&dc);
        return 1;
    }

    for (size_t i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, &ctx) != 0) {
            perror("Failed to create worker thread");
            // Caso falhe a criação, ajusta e aguarda as já criadas
            num_threads = i;
            break;
        }
    }

    for (size_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    pthread_mutex_destroy(&ctx.mutex);

    int result = ctx.result;

    free_config_files(&files);
    datacenter_destroy(&dc);
    return result;
}