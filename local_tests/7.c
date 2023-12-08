#include <stdio.h>
#include "common/test.h"

int main() {
    
    const char *filename = "mnt/data1.txt";

    if (remove(filename) == 0) {
        printf("File deleted successfully.\n");
    } else {
        perror("Unable to delete file");
        return FAIL;
    }

    return PASS;
}
