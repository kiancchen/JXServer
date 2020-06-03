#include "directory.h"


int main(void) {
    struct dirent *dir;
    DIR *d = opendir("./testing");
    if (d == NULL) {
        printf ("Cannot open directory '%s'\n", ".");
        return 1;
    }
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG){
                printf("%s\n", dir->d_name);
            }

        }
        closedir(d);
    }
    return(0);
}