#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <fcntl.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "wfs.h"

static char* disk_path;
static char* mount_point;

struct wfs_inode root_inode;
struct wfs_log_entry root_log_entry;

// Helper function to get the full path of a file or directory
static void get_full_path(const char *path, char *full_path) {
    strcpy(full_path, disk_path);
    strcat(full_path, "/");
    strcat(full_path, path);
}

static struct wfs_log_entry get_log_entry(const char *path, int inode_number) {
    char* curr;
    // iterate past the superblock
    base += sizeof(struct wfs_sb);
    while(curr != head) {
        struct wfs_log_entry* curr_log_entry = (struct wfs_log_entry*)curr;
        // if the thing is not deleted
        if (curr_log_entry->inode.deleted != 1) {
            // we found the log entry of the inode we need
            if (curr_log_entry->inode.inode_number == inode_number) {

            }
        }
        // we design the inode's size to be updated with size of data member of log entry struct
        curr += curr_log_entry->inode.size;
    }
}

// TODO maybe create a helper to get a log entry?

// Function to get attributes of a file or directory
static int wfs_getattr(const char *path, struct stat *stbuf) {
    char full_path[PATH_MAX];
    get_full_path(path, full_path);

    // TODO
    // get the inode for the path and do the below:

    // stbuf->st_uid = inode.uid;
    // stbuf->st_gid = inode.gid;
    // stbuf->st_mtime = inode.mtime;
    // stbuf->st_mode = inode.mode;
    // stbuf->st_nlink = inode.links;
    // stbuf->st_size = inode.size;

    return 0;
}

// Function to create a regular file
static int wfs_mknod(const char *path, mode_t mode, dev_t dev) {
    char full_path[PATH_MAX];
    get_full_path(path, full_path);

    // TODO 
    // IF root level:
    //      we create an inode
    //      we create a dentry and put it in the root log entry
    //      we create a log entry for the file itself and store the inode
    // ELSE not root level:
    //      we create an inode
    //      we find the dentry->log_entry->.data->dentry->log_entry ... for the parent dir specified by path
    //      we create a new entry for the above and add it to the head with the same inode
    //      finally we would add a new dentry to this entry copy at the end of the log
    //      we add the entry associated with that dentry to the end of the log as well


    // dissect the path:
    // if the path is root level, we need to create a log_entry
    //  first, we create a new log entry, then we populate its inode.
    // if the path is NOT root level, we need to create a dentry AND a log entry
    //  find the dentry (if it is not root level) for the 
    //  



    struct wfs_inode inode;
    inode.inode_number = time(NULL); // Use timestamp as a unique inode number
    inode.deleted = 0;
    inode.mode = S_IFREG | mode;
    inode.uid = getuid();
    inode.gid = getgid();
    inode.flags = 0;
    inode.size = 0;
    inode.atime = time(NULL);
    inode.mtime = time(NULL);
    inode.ctime = time(NULL);
    inode.links = 1;

    FILE *fp = fopen(full_path, "wb");
    if (fp == NULL) {
        return -errno;
    }

    fwrite(&inode, sizeof(struct wfs_inode), 1, fp);
    fclose(fp);

    return 0;
}

// TODO -- create dentry? log entry?? update head of superblock...
// Function to create a directory
static int wfs_mkdir(const char *path, mode_t mode) {
    char full_path[PATH_MAX];
    get_full_path(path, full_path);

    struct wfs_inode inode;
    inode.inode_number = time(NULL);
    inode.deleted = 0;
    inode.mode = S_IFDIR | mode;
    inode.uid = getuid();
    inode.gid = getgid();
    inode.flags = 0;
    inode.size = 0;
    inode.atime = time(NULL);
    inode.mtime = time(NULL);
    inode.ctime = time(NULL);
    inode.links = 2; // "." and ".." entries

    FILE *fp = fopen(full_path, "wb");
    if (fp == NULL) {
        return -errno;
    }

    fwrite(&inode, sizeof(struct wfs_inode), 1, fp);
    fclose(fp);

    // Create "." and ".." entries
    char dot_path[PATH_MAX];
    char dotdot_path[PATH_MAX];
    sprintf(dot_path, "%s/.", full_path);
    sprintf(dotdot_path, "%s/..", full_path);
    wfs_mknod(dot_path, S_IFDIR | mode, 0);
    wfs_mknod(dotdot_path, S_IFDIR | mode, 0);

    return 0;
}

// TODO redo all of below functions:

// Function to read data from a file
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int fd;
    int res;

    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);

    return res;
}

// Function to write data to a file
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int fd;
    int res;

    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);

    return res;
}

// Function to read directory entries
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    DIR *dir;
    struct dirent *dp;
    (void) offset;
    (void) fi;

    dir = opendir(path);
    if (dir == NULL)
        return -errno;

    while ((dp = readdir(dir)) != NULL) {
        filler(buf, dp->d_name, NULL, 0);
    }

    closedir(dir);

    return 0;
}

// Function to unlink (delete) a file
static int wfs_unlink(const char *path) {
    int res = unlink(path);
    if (res == -1)
        return -errno;

    return res;
}

static struct fuse_operations my_operations = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    .unlink    	= wfs_unlink,
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s [FUSE options] disk_path mount_point\n", argv[0]);
        exit(EXIT_FAILURE);
    }



    // Parse disk_path and mount_point from the command line arguments
    disk_path = argv[argc - 2];
    mount_point = argv[argc - 1];

    // FUSE options are passed to fuse_main, starting from argv[1]
    int fuse_argc = argc - 2;  // Adjust argc for FUSE options
    char **fuse_argv = argv + 1;

    // Disable multi-threading with -s option
    fuse_argv[0] = "-s";

    // Call fuse_main with your FUSE operations and data
    return fuse_main(fuse_argc, fuse_argv, &my_operations, NULL);
}
