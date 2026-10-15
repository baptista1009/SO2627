#ifndef FILESYSTEM__H
#define FILESYSTEM__H

#include <stddef.h>

/**
 * Checks whether a path exists and is a directory.
 *
 * @param path Directory path.
 *
 * @return 1 if it exists and is a directory, 0 otherwise.
 */
int path_exists(const char *path);

/**
 * Checks whether a path exists and is a regular file.
 *
 * @param path File path.
 *
 * @return 1 if it exists and is a regular file, 0 otherwise.
 */
int file_exists(const char *path);

/**
 * Converts the given path into an absolute path, resolving symbolic links,
 * relative components ('.' and '..'), and redundant separators. The resolved
 * path is copied into the provided buffer.
 *
 * @param path Path to resolve.
 * @param buffer Destination buffer where the absolute path will be stored.
 * @param size Size of the destination buffer, in bytes.
 *
 * @return 0 if the path was successfully resolved and copied to the buffer
 * @return 1 if the path could not be resolved or the buffer is too small.
 */
int absolute_path(const char *path, char *buffer, size_t size);

typedef struct {
    char **paths;
    size_t count;
} ConfigFileList;

/*
 * Retrieves the paths of the .conf files in a directory,
 * sorted alphabetically.
 * Returns 0 on success and 1 on error.
 * The resulting list must be freed using free_config_files().
 */
int list_config_files(const char *dir_path, ConfigFileList *list);

/*
 * Frees the memory used by the list of paths.
 */
void free_config_files(ConfigFileList *list);

/*
 * Creates the VM directory inside the reservation directory.
 * Stores the full path in buffer.
 * Returns 0 on success and 1 on error.
 */
int prepare_vm_directory(const char *reservation_id, const char *vm_id, char *buffer, size_t size);

/*
 * Copies the contents of source to destination,
 * including files and subdirectories.
 * Returns 0 on success and 1 on error.
 */
int copy_directory(const char *source, const char *destination);
#endif // FILESYSTEM__H