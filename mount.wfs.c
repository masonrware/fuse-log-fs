#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

// static const char *wfs_path = "/";  // Mount point

// Function to get attributes of a file or directory
static int wfs_getattr(const char *path, struct stat *stbuf) {
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        res = -ENOENT;
    }

    return res;
}

// Function to create a regular file
static int wfs_mknod(const char *path, mode_t mode, dev_t dev) {
    int res = 0;

    if (S_ISREG(mode)) {
        res = mknod(path, mode, dev);
        if (res == -1)
            return -errno;
    } else {
        res = -EINVAL;
    }

    return res;
}

// Function to create a directory
static int wfs_mkdir(const char *path, mode_t mode) {
    int res = 0;

    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return res;
}
//
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
    // Initialize FUSE with specified operations
    // TODO: Filter argc and argv here and then pass it to fuse_main
    // You need to pass [FUSE options] along with the mount_point to fuse_main as argv. 
    // You may assume -s is always passed to mount.wfs as a FUSE option to disable multi-threading.
    
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc -= 1;

    return fuse_main(argc, argv, &my_operations, NULL);
}
