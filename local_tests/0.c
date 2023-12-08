#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "common/test.h"

int main() {
   
 const char* paths[] = {
        "mnt/file0",
        "mnt/file1",
        "mnt/dir0/file00",
        "mnt/dir0/file01",
        "mnt/dir1/file10",
        "mnt/dir1/file11"
    };

    for (size_t i = 0; i < 6; i++)
    {
        int fd = open(paths[i], O_RDONLY);
        if (fd == -1) {
          printf("Unable to open file: %s", paths[i]);
          perror("open");
          return FAIL;
        }

        char buffer[7] = {0};
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

        const char* content = "content";

        if (bytesRead != strlen(content) || strcmp(content, buffer) !=0 ) {
            close(fd);
            printf("Wrong content for file %s: file contains %s\n", paths[i], buffer);
            return FAIL;
        }
        close(fd);        
    }

    return PASS;
}
