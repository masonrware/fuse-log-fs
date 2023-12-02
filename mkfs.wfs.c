#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "wfs.h"

char* base;

void initialize_filesystem(const char *disk_path) {
    int fd;

    // Open file descriptor for file to init system with
    fd = open(disk_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get file info (for file size)
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Memory map file
    base = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                fd, 0);

    // initialize the superblock
    // TODO pointer to superblock pointer -- have it point to base??
    // TODO we will be using memcpy somewhere??

    struct wfs_sb superblock = {
        .magic = WFS_MAGIC,
        .head = 0
    };

    // Initialize the root directory log entry
    struct wfs_inode root_inode = {
        .inode_number = 0,
        // TODO ... other fields ...
    };

    struct wfs_log_entry root_log_entry = {
        .inode = root_inode,
    };

    // TODO PUT THE SUPERBLOCK AND THE ROOT LOG ENTRY ON THE LOG (AFTER BASE) TO CALL MUNMAP WITH 

    // TODO call munmap to write to disk

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
