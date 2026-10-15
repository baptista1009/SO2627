#define _XOPEN_SOURCE 700

#include "filesystem.h"
#include "constants.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>


int path_exists(const char *path){
  struct stat st;

  if (stat(path, &st) != 0)
    return 0;

  return S_ISDIR(st.st_mode);
}

int file_exists(const char *path){
  struct stat st;

  if (stat(path, &st) != 0)
    return 0;

  return S_ISREG(st.st_mode);
}

int absolute_path(const char *path, char *buffer, size_t size){
  char *resolved = realpath(path, NULL);

  if (resolved == NULL)
    return 1;

  if (strlen(resolved) >= size) {
    free(resolved);
    return 1;
  }

  strcpy(buffer, resolved);

  free(resolved);
  return 0;
}

static int is_config_entry(const struct dirent *entry) {
    size_t length = strlen(entry->d_name);

    return length >= 5 &&
           strcmp(entry->d_name + length - 5, ".conf") == 0;
}

void free_config_files(ConfigFileList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }

    free(list->paths);

    list->paths = NULL;
    list->count = 0;
}

int list_config_files(const char *dir_path, ConfigFileList *list) {
    list->paths = NULL;
    list->count = 0;

    struct dirent **entries = NULL;

    int count = scandir(dir_path, &entries, is_config_entry, alphasort);

    if (count == -1) {
        perror("Failed to scan configuration directory");
        return 1;
    }

    if (count == 0) {
        free(entries);
        return 0;
    }

    list->paths = malloc((size_t)count * sizeof(*list->paths));

    if (list->paths == NULL) {
        perror("Failed to allocate configuration list");

        for (int i = 0; i < count; i++) {
            free(entries[i]);
        }

        free(entries);
        return 1;
    }

    int result = 0;

    for (int i = 0; i < count; i++) {
        size_t length =
            strlen(dir_path) + strlen(entries[i]->d_name) + 2;

        char *path = malloc(length);

        if (path == NULL) {
            perror("Failed to allocate configuration path");
            result = 1;
            break;
        }

        snprintf(path, length, "%s/%s", dir_path,entries[i]->d_name);

        if (!file_exists(path)) {
            free(path);
            continue;
        }

        list->paths[list->count++] = path;
    }

    for (int i = 0; i < count; i++) {
        free(entries[i]);
    }

    free(entries);

    if (result != 0) {
        free_config_files(list);
    }

    return result;
}

static int ensure_directory(const char *path) {
    if (mkdir(path, 0700) == 0) {
        return 0;
    }

    if (errno == EEXIST && path_exists(path)) {
        return 0;
    }

    perror("Failed to create directory");
    return 1;
}

int prepare_vm_directory(const char *reservation_id, const char *vm_id,  char *buffer, size_t size) {
    if (ensure_directory("/tmp/CloudIST") != 0) {
        return 1;
    }

    char reservation_path[MAX_PATH_SIZE];

    int length = snprintf(reservation_path, sizeof(reservation_path), "/tmp/CloudIST/%s", reservation_id);

    if (length < 0 || (size_t)length >= sizeof(reservation_path)) {
        fprintf(stderr, "Reservation directory path is too long.\n");
        return 1;
    }

    if (ensure_directory(reservation_path) != 0) {
        return 1;
    }

    length = snprintf(buffer, size, "%s/%s", reservation_path, vm_id);

    if (length < 0 || (size_t)length >= size) {
        fprintf(stderr, "VM directory path is too long.\n");
        return 1;
    }

    return ensure_directory(buffer);
}

static int copy_regular_file(const char *source, const char *destination) {
    int input_fd = open(source, O_RDONLY);

    if (input_fd == -1) {
        perror("Failed to open source file");
        return 1;
    }

    int output_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (output_fd == -1) {
        perror("Failed to open destination file");
        close(input_fd);
        return 1;
    }

    int result = 0;
    char buffer[4096];

    while (1) {
        ssize_t bytes_read = read(input_fd, buffer, sizeof(buffer));

        if (bytes_read == 0) {
            break;
        }

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("Failed to read source file");
            result = 1;
            break;
        }

        size_t offset = 0;

        while (offset < (size_t)bytes_read) {
            ssize_t bytes_written = write(output_fd, buffer + offset, (size_t)bytes_read - offset);

            if (bytes_written == -1 && errno == EINTR) {
                continue;
            }

            if (bytes_written <= 0) {
                fprintf(stderr, "Failed to write destination file.\n");
                result = 1;
                break;
            }

            offset += (size_t)bytes_written;
        }

        if (result != 0) {
            break;
        }
    }

    if (close(output_fd) == -1) {
        perror("Failed to close destination file");
        result = 1;
    }

    if (close(input_fd) == -1) {
        perror("Failed to close source file");
        result = 1;
    }

    return result;
}

int copy_directory(const char *source, const char *destination) {
    DIR *directory = opendir(source);

    if (directory == NULL) {
        perror("Failed to open source directory");
        return 1;
    }

    if (ensure_directory(destination) != 0) {
        closedir(directory);
        return 1;
    }

    int result = 0;

    while (1) {
        errno = 0;

        struct dirent *entry = readdir(directory);

        if (entry == NULL) {
            if (errno != 0) {
                perror("Failed to read source directory");
                result = 1;
            }

            break;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char source_path[MAX_PATH_SIZE];
        char destination_path[MAX_PATH_SIZE];

        int source_length = snprintf(source_path, sizeof(source_path), "%s/%s", source, entry->d_name);

        int destination_length = snprintf(destination_path, sizeof(destination_path), "%s/%s", destination, entry->d_name);

        if (source_length < 0 || (size_t)source_length >= sizeof(source_path) ||
            destination_length < 0 || (size_t)destination_length >= sizeof(destination_path)) {
            fprintf(stderr, "Copy path is too long.\n");
            result = 1;
            break;
        }

        struct stat info;

        if (lstat(source_path, &info) == -1) {
            perror("Failed to inspect source entry");
            result = 1;
            break;
        }

        if (S_ISDIR(info.st_mode)) {
            result = copy_directory(source_path, destination_path);
        } else if (S_ISREG(info.st_mode)) {
            result = copy_regular_file(source_path, destination_path);
        } else {
            fprintf(stderr, "Unsupported source entry: %s\n", source_path);
            result = 1;
        }

        if (result != 0) {
            break;
        }
    }

    if (closedir(directory) == -1) {
        perror("Failed to close source directory");
        result = 1;
    }

    return result;
}