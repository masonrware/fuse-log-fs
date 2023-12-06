#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "wfs.h"

int total_size = 0;

void initialize_filesystem(const char *disk_path) {
    int fd;

    // Open file descriptor for file to init system with
    fd = open(disk_path, O_RDWR, 0666);
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
    char* base = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                fd, 0);

    // Check for errors in mmap
    if (base == MAP_FAILED) {
        printf("mmap failed\n");
        exit(0);
    }

    // initialize the superblock
    struct wfs_sb* superblock = (struct wfs_sb*)base;

    superblock->magic = WFS_MAGIC;
    superblock->head = sizeof(struct wfs_sb);

    // Initialize the root directory log entry
    struct wfs_inode root_inode;
    
    root_inode.inode_number = 0;
    root_inode.deleted = 0;
    root_inode.mode = S_IFDIR;        // Set to S_IFDIR for a directory
    root_inode.uid = getuid();        // Set to the user id of the process
    root_inode.gid = getgid();        // Set to the group id of the process
    root_inode.flags = 0;             // You can set flags based on your requirements
    root_inode.size = 4096;           // Set to an appropriate size for a directory (e.g., 4 KB)
    root_inode.atime = time(NULL);    // Set to the current time
    root_inode.mtime = time(NULL);    // Set to the current time
    root_inode.ctime = time(NULL);    // Set to the current time
    root_inode.links = 0;             // Number of hard links

    struct wfs_log_entry* root_log_entry = (struct wfs_log_entry *)malloc(sizeof(struct wfs_log_entry));
    root_log_entry->inode = root_inode;

    size_t root_log_entry_size = sizeof(struct wfs_log_entry);

    // Place the root log entry at the head address
    memcpy((char *)&superblock->head, root_log_entry, root_log_entry_size);
    
    // Update the head to be after the added root log entry
    superblock->head += root_log_entry_size;
    total_size += root_log_entry_size + sizeof(struct wfs_sb);

    // write to disk
    munmap(base, file_stat.st_size);

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
