#define FUSE_USE_VERSION 30
// #include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <fcntl.h>
#include <dirent.h>

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include "wfs.h"

char * base;
char * head;

struct wfs_sb *superblock;

int set_free_start = 0;
char * free_start;
char * free_end;

int clean_log() {
    char *curr = base;

    // iterate past the superblock
    curr += sizeof(struct wfs_sb);

    while (curr != head)
    {
        struct wfs_log_entry *curr_log_entry = (struct wfs_log_entry *)curr;
        // find all deleted entries
        if (curr_log_entry->inode.deleted)
        {
            if(set_free_start == 0) {
                free_start = (char *)curr_log_entry;
                set_free_start = 1;
            } 
            // curr_log_entry = (char *)(curr_log_entry) + curr_log_entry->inode.size;
        } else {
            char * old_alloc_start = (char *)curr_log_entry;
            memcpy(free_start, curr_log_entry, curr_log_entry->inode.size);
            free_start += curr_log_entry->inode.size;
            free_end = free_start += curr_log_entry->inode.size;
            curr = old_alloc_start + curr_log_entry->inode.size;
        }
    }
    head = free_end;
    superblock->head = free_end - (base + sizeof(struct wfs_sb));

    return 0;
}

int main(int argc, char *argv[]) {
    int fd;

    // Open file descriptor for file to init system with
    fd = open(argv[1], O_RDWR, 0666);
    if (fd == -1)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get file info (for file size)
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1)
    {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    base = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                fd, 0);
    // Check for errors in mmap
    if (base == MAP_FAILED)
    {
        // TODO Handle error
        return -1;
    }

    // Cast superblock
    superblock = (struct wfs_sb *)base;

    // Check magic number
    if (superblock->magic != WFS_MAGIC)
    {
        return -1;
    }

    // Store head global
    head = base + superblock->head;

    clean_log();

    munmap(base, file_stat.st_size);

    return 0;
}