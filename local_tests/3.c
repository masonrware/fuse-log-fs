#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common/test.h"

int main() {
   
    const char *path = "mnt/data2";

    int status = mkdir(path, S_IRWXU);

    if (status == 0) {
        printf("Directory created inside root successfully.\n");
    } else {
        perror("Unable to create directory");
        return FAIL;
    }

    path = "mnt/data2/data3";

    status = mkdir(path, S_IRWXU);

    if (status == 0) {
        printf("Directory created inside data successfully.\n");
    } else {
        perror("Unable to create directory");
        return FAIL;
    }

    return PASS;
}
