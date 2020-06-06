#include "directory.h"

/**
 * Get the information of files in a directory
 * @param dir_path The directory to be searched
 * @param num_files (Out param) The number of files
 * @param filename_length (Out param) The length of filenames of all files
 */
void get_dir_info(char *dir_path, int *num_files, uint64_t *filename_length) {
    struct dirent *dir;
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        printf("Cannot open directory '%s'\n", dir_path);
        return;
    }
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                *num_files += 1;
                *filename_length += strlen(dir->d_name) + 1;
            }
        }
        closedir(d);
    }
}

/**
 * Concatenate files in a string
 * @param dir_path The directory to be searched
 * @param filenames (Out param) stores the filenames
 */
void list_filenames(char *dir_path, char *filenames) {
    struct dirent *dir;
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        printf("Cannot open directory '%s'\n", dir_path);
        return;
    }
    if (d) {
        size_t start = 0;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                memcpy(filenames + start, dir->d_name, strlen(dir->d_name));
                start += strlen(dir->d_name) + 1;
                filenames[start - 1] = 0x00; // NULL terminated
            }
        }
        closedir(d);
    }
}

/**
 * Get the list of files in a specific directory
 * @param dir_path The directory to be searched
 * @param filenames_length (Out param) The total length of filenames of all files
 * @return The filenames
 */
char *get_file_list(char *dir_path, uint64_t *filenames_length) {
    int num_files = 0;
    *filenames_length = 0;
    get_dir_info(dir_path, &num_files, filenames_length);
    char *filenames = malloc(sizeof(char) * (*filenames_length));
    list_filenames(dir_path, filenames);
    return filenames;
}