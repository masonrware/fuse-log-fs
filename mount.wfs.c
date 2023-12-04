#define FUSE_USE_VERSION 30
#include <fuse.h>
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

int inode_count = 0;

static char* disk_path;
static char* mount_point;

char* head;
char* base;
struct wfs_sb* superblock;

struct wfs_inode root_inode;
struct wfs_log_entry root_log_entry;

// Helper function to get the full path of a file or directory
static void get_full_path(const char *path, char *full_path) {
    strcpy(full_path, disk_path);
    strcat(full_path, "/");
    strcat(full_path, path);
}

// Remove the top level part of a path
char* snip_top_level(const char* path) {
    if (path == NULL || strlen(path) == 0) {
        // Handle invalid input
        return NULL;
    }
    
    // Find the first occurrence of '/'
    const char* first_slash = strchr(path, '/');
    if (first_slash == NULL) {
        // No top-level part found, return an empty string or a copy of the input path
        return strdup("");
    }

    // Find the second occurrence of '/' starting from the position after the first slash
    const char* second_slash = strchr(first_slash + 1, '/');
    if (second_slash == NULL) {
        // No second slash found, return an empty string or a copy of the input path
        return strdup("");
    }
    
    // Calculate the length of the remaining path
    size_t remaining_length = strlen(second_slash);

    // Allocate memory for the remaining path
    char* remaining_path = (char*)malloc((remaining_length + 1) * sizeof(char));
    if (remaining_path == NULL) {
        // Memory allocation failed
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the remaining path into the new string
    strcpy(remaining_path, second_slash);

    return remaining_path;
}

// Remove all pre-mount portion and file extension from a path 
char* isolate_path(const char* path) {
    if (path == NULL || mount_point == NULL || strlen(path) == 0 || strlen(mount_point) == 0) {
        // Handle invalid input
        return NULL;
    }

    // Find the mount point in the path
    const char* mount_point_pos = strstr(path, mount_point);
    if (mount_point_pos == NULL) {
        // Mount point not found, return a copy of the input path
        return strdup(path);
    }

    // Move the pointer after the mount point
    mount_point_pos += strlen(mount_point);

    // Find the last occurrence of '/'
    const char* last_slash = strrchr(mount_point_pos, '/');
    if (last_slash == NULL) {
        // No '/' found after the mount point, return an empty string
        return strdup("");
    }

    // Calculate the length of the remaining path
    size_t remaining_length = last_slash - mount_point_pos;

    // Allocate memory for the remaining path
    char* remaining_path = (char*)malloc((remaining_length + 1) * sizeof(char));
    if (remaining_path == NULL) {
        // Memory allocation failed
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the remaining path into the new string
    strncpy(remaining_path, mount_point_pos, remaining_length);
    remaining_path[remaining_length] = '\0';

    return remaining_path;
}

// Get the log entry of the direct parent dir
static struct wfs_log_entry* get_log_entry(const char *path, int inode_number) {
    char* curr = (char *)malloc(strlen(base) + 1);
    strcpy(curr, base);

    // iterate past the superblock
    curr += sizeof(struct wfs_sb); // skip past superblock
    while(curr != head) {
        struct wfs_log_entry* curr_log_entry = (struct wfs_log_entry*)curr;
        // if the thing is not deleted
        if (curr_log_entry->inode.deleted != 1) {
            // we found the log entry of the inode we need
            if (curr_log_entry->inode.inode_number == inode_number) {
                // base case -- either "" or "/"
                if(strlen(path) == 0 || strlen(path) == 1) {
                    return curr_log_entry;
                } else {
                    char path_copy[100];  // Adjust the size according to your needs
                    strcpy(path_copy, path);
                    
                    // Use strtok to get the first token
                    char* ancestor = strtok(path_copy, "/");

                    char* data_addr = curr_log_entry->data;

                    // TODO -- THIS WILL CHANGE WITH OUR APPROACH TO .DATA IT ALSO NEEDS FIXING IN THE LOOP
                    // iterate over all dentries
                    while(data_addr != (curr_log_entry->data + curr_log_entry->inode.size)) {
                        // if the subdir is the current highest ancestor of our target
                        if (strcmp(((struct wfs_dentry*) data_addr)->name, ancestor) == 0) {
                            return get_log_entry(snip_top_level(path), ((struct wfs_dentry*) data_addr)->inode_number);
                        }
                        data_addr += sizeof(struct wfs_dentry);
                    }
                }
            }
        }
        // we design the inode's size to be updated with size of data member of log entry struct
        curr += curr_log_entry->inode.size;
    }

    return NULL;
}

// Get filename from a path
char* get_filename(const char* path) {
    if (path == NULL || strlen(path) == 0) {
        // Handle invalid input
        return NULL;
    }

    // Find the last occurrence of '/'
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        // No '/' found in the path, return a copy of the input path
        return strdup(path);
    }

    // Move the pointer after the last slash
    last_slash += 1;

    // Calculate the length of the filename
    size_t filename_length = strlen(last_slash);

    // Allocate memory for the filename
    char* filename = (char*)malloc((filename_length + 1) * sizeof(char));
    if (filename == NULL) {
        // Memory allocation failed
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the filename into the new string
    strcpy(filename, last_slash);

    return filename;
}

// Check if filename contains valid characters
int isValidFilename(const char *filename) {
    while (*filename != '\0') {
        if (!(isalnum(*filename) || *filename == '_')) {
            // If the character is not alphanumeric or underscore, the filename is invalid
            return 0;
        }
        filename++;
    }

    // All characters in the filename are valid
    return 1;
}

// Validity check for file creation
// take in path including new filename
// TODO FINISH BELOW
int canCreate(char *path){
    char* fname = get_filename(path);
    char* dir = get_filename(isolate_path(path));

    // Check return val from get_filename
    if (strcmp(fname, "") == 0){
        printf("Empty filename\n");
    }

    // Check filename
    if (!isValidFilename(fname)) return 0;

    // Check if filename is unique in directory
    
}


////// BELOW IS FOR FUSE ///////


// Function to get attributes of a file or directory
static int wfs_getattr(const char *path, struct stat *stbuf) {
    char full_path[PATH_MAX];
    get_full_path(path, full_path);

    struct wfs_log_entry* log_entry = get_log_entry(path, 0);

    // Update time of last access
    log_entry->inode.atime = time(NULL);

    stbuf->st_uid = log_entry->inode.uid;
    stbuf->st_gid = log_entry->inode.gid;
    stbuf->st_mtime = log_entry->inode.mtime;
    stbuf->st_mode = log_entry->inode.mode;
    stbuf->st_nlink = log_entry->inode.links;
    stbuf->st_size = log_entry->inode.size;

    return 0;
}

// Function to create a regular file
static int wfs_mknod(const char *path, mode_t mode, dev_t dev) {
    char full_path[PATH_MAX];
    get_full_path(path, full_path);

    // Verify file name
    if(!isValidFilename(get_filename(path))){
        printf("Invalid Filename");
        return -1;
    }
    
    // Verify file doesn't exist in its intended parent dir
    if(!canCreate(path)) {
        return -EEXIST; 
    }

    // Create an inode for the file
    struct wfs_inode new_inode;
    inode_count += 1;

    new_inode.inode_number = inode_count; // Use timestamp as a unique inode number
    new_inode.deleted = 0;
    new_inode.mode = S_IFREG;
    new_inode.uid = getuid();
    new_inode.gid = getgid();
    new_inode.flags = 0;
    // TODO what to do for below?
    new_inode.size = sizeof(struct wfs_inode);
    new_inode.atime = time(NULL);
    new_inode.mtime = time(NULL);
    new_inode.ctime = time(NULL);
    new_inode.links = 1;

    // Create a new dentry for the file
    struct wfs_dentry* new_dentry = (struct wfs_dentry*)malloc(sizeof(struct wfs_dentry));
    if (new_dentry != NULL) {
        // Copy the filename to the new_dentry->name using a function like strncpy
        strncpy(new_dentry->name, get_filename(path), MAX_FILE_NAME_LEN - 1);
        new_dentry->name[MAX_FILE_NAME_LEN - 1] = '\0';  // Ensure null-termination
        new_dentry->inode_number = new_inode.inode_number;
    } else {
        // Handle allocation failure
    }

    // Get parent directory log entry
    struct wfs_log_entry* old_log_entry = get_log_entry(path, 0);

    // Mark old log entry as deleted
    old_log_entry->inode.deleted = 1;

    // Make a copy of the old log entry and add the created dentry to its data field
    struct wfs_log_entry* log_entry_copy = (struct wfs_log_entry*)malloc(sizeof(struct wfs_log_entry));
    if (log_entry_copy != NULL) {
        // copy the entire old log entry (including it's data field) to the new log entry
        // TODO there might be an error here -- new log entry is of size wfs_log_entry, old_log_entry could be larger
        memcpy(log_entry_copy, old_log_entry, old_log_entry->inode.size);
        // TODO is this redundant?
        log_entry_copy->inode.size = old_log_entry->inode.size;

        // add the dentry to log_entry_copy's data and update new log entry's size
        memcpy(log_entry_copy->inode.size, new_dentry, sizeof(struct wfs_dentry));
        log_entry_copy->inode.size += sizeof(struct wfs_dentry);

        // update the head
        head += log_entry_copy->inode.size;
    } else {
        // Handle allocation failure
    }

    // Create a log entry for the file itself
    struct wfs_log_entry* new_log_entry = (struct wfs_log_entry*)malloc(sizeof(struct wfs_log_entry));
    if (new_log_entry != NULL) {
        // point the log entry at the created inode
        // TODO should I use memcpy?
        new_log_entry->inode = new_inode;

        // add log entry to the log
        memcpy(head, new_log_entry, sizeof(new_log_entry->inode.size));

        // update the head
        head += new_log_entry->inode.size;
    } else {
        // Handle allocation failure
    }

    return 0;
}

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

    // TODO FIX THIS
    base = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                fd, 0);

    // Check for errors in mmap
    if (base == MAP_FAILED) {
        // TODO Handle error
        return -1;
    }

    // Cast superblock
    superblock = (struct wfs_sb*)base;

    // Check magic number
    if (superblock->magic != WFS_MAGIC){
        return -1;
    }

    // Store head global
    head = (char*) &superblock->head;

    // FUSE options are passed to fuse_main, starting from argv[1]
    int fuse_argc = argc - 2;  // Adjust argc for FUSE options
    char **fuse_argv = argv + 1;

    // Disable multi-threading with -s option
    fuse_argv[0] = "-s";

    // Call fuse_main with your FUSE operations and data
    return fuse_main(fuse_argc, fuse_argv, &my_operations, NULL);
}
