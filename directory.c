#include "directory.h"

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
                *filename_length += dir->d_namlen + 1;
            }
        }
        closedir(d);
    }

}

void set_filenames(char *dir_path, char *filenames) {
    struct dirent *dir;
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        printf("Cannot open directory '%s'\n", dir_path);
        return;
    }
    if (d) {
        int start = 0;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                memcpy(filenames + start, dir->d_name, dir->d_namlen);
//                byte_copy(filenames, dir->d_name, start, dir->d_namlen);
                start += dir->d_namlen + 1;
                filenames[start - 1] = 0x00;
            }
        }
        closedir(d);
    }
}


char *get_file_list(char *dir_path, uint64_t *filenames_length) {
    int num_files = 0;
    *filenames_length = 0;
    get_dir_info(dir_path, &num_files, filenames_length);
    char *filenames = malloc(sizeof(char) * (*filenames_length));
    set_filenames(dir_path, filenames);
    return filenames;
}

//int main(void) {
//    uint64_t filenames_len;
//    char *filenames = get_file_list("./testing", &filenames_len);
//    for (int i = 0; i < filenames_len; ++i) {
//        printf("%c", filenames[i]);
//        if (filenames[i] == 0x00) {
//            printf(" ");
//        }
//    }
//}