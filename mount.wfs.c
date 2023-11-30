#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BLOCK_SIZE 4096

void initialize_filesystem(const char *disk_path) {
    int fd;
    char empty_block[BLOCK_SIZE] = {0};

    fd = open(disk_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Calculate the number of blocks needed for the filesystem
    off_t file_size = BLOCK_SIZE * 1024; // Adjust the size as needed
    off_t num_blocks = file_size / BLOCK_SIZE;

    // Write empty blocks to the file to initialize the filesystem
    for (off_t i = 0; i < num_blocks; ++i) {
        if (write(fd, empty_block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("Error writing to file");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    close(fd);
    printf("Filesystem initialized successfully.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *disk_path = argv[1];
    initialize_filesystem(disk_path);

    return 0;
}
// comment