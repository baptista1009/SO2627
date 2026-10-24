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

/**
 * Retrieves the paths of regular .conf files in a directory,
 * sorted alphabetically.
 * The resulting list must be freed using free_config_files().
 *
 * @param dir_path Path to the configuration directory.
 * @param list Pointer to the list where the paths will be stored.
 *
 * @return 0 on success.
 * @return 1 on error.
 */
int list_config_files(const char *dir_path, ConfigFileList *list);

/**
 * Frees the memory used by the list of paths
 * and resets its fields.
 *
 * @param list Pointer to the list to free.
 */
void free_config_files(ConfigFileList *list);

/**
 * Ensures that /tmp/CloudIST-114851, the reservation directory,
 * and the VM directory exist.
 * Stores the full VM directory path in buffer.
 *
 * @param reservation_id ID of the reservation containing the VM.
 * @param vm_id ID of the VM.
 * @param buffer Destination buffer for the VM directory path.
 * @param size Size of the destination buffer, in bytes.
 *
 * @return 0 on success.
 * @return 1 on error.
 */
int prepare_vm_directory(
    const char *reservation_id,
    const char *vm_id,
    char *buffer,
    size_t size
);

/**
 * Recursively copies regular files and subdirectories
 * from source to destination, creating destination directories
 * as needed. Existing files are overwritten.
 *
 * @param source Path to the source directory.
 * @param destination Path to the destination directory.
 *
 * @return 0 on success.
 * @return 1 on error.
 */
int copy_directory(const char *source, const char *destination);

#endif // FILESYSTEM__H