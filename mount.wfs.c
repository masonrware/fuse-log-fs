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
int total_size;

static char *disk_path;
static char *mount_point;

char *head;
char *base;
struct wfs_sb *superblock;

struct wfs_inode root_inode;
struct wfs_log_entry root_log_entry;

// Remove the top-most (left most) extension of a path
char *snip_top_level(const char *path)
{
    if (path == NULL || strlen(path) == 0)
    {
        // Handle invalid input
        return NULL;
    }

    // Find the first occurrence of '/'
    const char *first_slash = strchr(path, '/');
    if (first_slash == NULL)
    {
        // No top-level part found, return an empty string or a copy of the input path
        return strdup("");
    }

    // Find the second occurrence of '/' starting from the position after the first slash
    const char *second_slash = strchr(first_slash + 1, '/');
    if (second_slash == NULL)
    {
        // No second slash found, return an empty string or a copy of the input path
        return strdup("");
    }

    // Calculate the length of the remaining path
    size_t remaining_length = strlen(second_slash);

    // Allocate memory for the remaining path
    char *remaining_path = (char *)malloc((remaining_length + 1) * sizeof(char));
    if (remaining_path == NULL)
    {
        // Memory allocation failed
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the remaining path into the new string
    strcpy(remaining_path, second_slash);

    return remaining_path;
}

// Remove the bottom-most (right most) extension of a path
char *snip_bottom_level(const char *path)
{
   if (path == NULL || mount_point == NULL || strlen(path) == 0 || strlen(mount_point) == 0)
    {
        // Handle invalid input
        return NULL;
    }
    
    // Find the last dot (.) in the path, representing the start of the extension
    const char* last_slash = strrchr(path, '/');

    // Calculate the length of the remaining path without the extension
    size_t remaining_length = last_slash - path;

    // Allocate memory for the remaining path
    char* remaining_path = malloc(remaining_length + 1);

    // Copy the remaining path to the result string
    strncpy(remaining_path, path, remaining_length);

    // Null-terminate the result string
    remaining_path[remaining_length] = '\0';

    return remaining_path;
}

// Same as snip_bottom_level, just returns the final part of the path instead of the path itself
// Get bottom-level (right most) extention of a path
char *get_bottom_level(const char *path)
{
    if (path == NULL || strlen(path) == 0)
    {
        // Handle invalid input
        return NULL;
    }

    // Find the last occurrence of '/'
    const char *last_slash = strrchr(path, '/');

    // Move the pointer after the last slash
    last_slash += 1;

    // Calculate the length of the filename
    size_t last_part_length = strlen(last_slash);

    // Allocate memory for the filename
    char *last_part = (char *)malloc((last_part_length + 1) * sizeof(char));
    if (last_part == NULL)
    {
        // Memory allocation failed
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the filename into the new string
    strcpy(last_part, last_slash);

    return last_part;
}

// Get the log entry of the bottom-level (right most) extension of a path recursively
static struct wfs_log_entry *get_log_entry(const char *path, int inode_number)
{
    char *curr = base;

    // iterate past the superblock
    curr += sizeof(struct wfs_sb);
    while (curr != head)
    {
        struct wfs_log_entry *curr_log_entry = (struct wfs_log_entry *)curr;
        // if the thing is not deleted
        if (curr_log_entry->inode.deleted != 1)
        {
            // we found the log entry of the inode we need
            if (curr_log_entry->inode.inode_number == inode_number)
            {
                // base case -- either "" or "/"
                if (path == NULL)
                {
                    return curr_log_entry;
                }
                else
                {
                    char path_copy[100];
                    strcpy(path_copy, path);

                    // Use strtok to get the first token
                    char *ancestor = strtok(path_copy, "/");

                    char *data_addr = curr_log_entry->data;

                    // iterate over all dentries
                    while (data_addr != (char *)(curr_log_entry + curr_log_entry->inode.size))
                    {
                        // if the subdir is the current highest ancestor of our target
                        if (strcmp(((struct wfs_dentry *)data_addr)->name, ancestor) == 0)
                        {
                            return get_log_entry(snip_top_level(path), ((struct wfs_dentry *)data_addr)->inode_number);
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

// Remove any pre-mount portion (including the mount point) of a path
char *remove_pre_mount(const char *path)
{
    if (path == NULL || mount_point == NULL || strlen(path) == 0 || strlen(mount_point) == 0)
    {
        // Handle invalid input
        return NULL;
    }

    // Find the mount point in the path
    const char *mount_point_pos = strstr(path, mount_point);
    if (mount_point_pos == NULL)
    {
        // Mount point not found, move mount point back to start of path and continue
        // return strdup(path);
        mount_point_pos = path;
    }

    // Move the pointer after the mount point
    mount_point_pos += strlen(mount_point);

    // Calculate the length of the remaining path
    // size_t remaining_length = last_slash - mount_point_pos;
    size_t remaining_length = path+strlen(path) - mount_point_pos;


    // Allocate memory for the remaining path
    char *remaining_path = (char *)malloc((remaining_length + 1) * sizeof(char));
    if (remaining_path == NULL)
    {
        // Memory allocation failed
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the remaining path into the new string
    strncpy(remaining_path, mount_point_pos, remaining_length);
    remaining_path[remaining_length] = '\0';

    return remaining_path;
}

// Check if filename contains valid characters
// TODO check length as well?
int valid_name(const char *filename)
{
    while (*filename != '\0')
    {
        if (!(isalnum(*filename) || *filename == '_'))
        {
            // If the character is not alphanumeric or underscore, the filename is invalid
            return 0;
        }
        filename++;
    }

    // All characters in the filename are valid
    return 1;
}

// Check if file/subdir can be created -- validate name and (local) uniqueness
int can_create(const char *path)
{
    char *last_part = get_bottom_level(path);

    // TODO is the below necessary?
    // Check return val from get_last_part
    if (strcmp(last_part, "") == 0)
    {
        printf("Empty filename\n");
    }

    // Check if filename is unique in directory
    struct wfs_log_entry *parent = get_log_entry(snip_bottom_level(path), 0);

    if(parent == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    char *data_addr = parent->data;

    // iterate over all dentry's of parent
    while (data_addr != (char *)(parent + parent->inode.size))
    {
        // check if current dentry matches desired filename
        if (strcmp(((struct wfs_dentry *)data_addr)->name, last_part) == 0)
            return 0;
        data_addr += sizeof(struct wfs_dentry);
    }

    // file/subdir name is valid for its targeted parent directory
    return 1;
}

////// BELOW IS FOR FUSE ///////

// Function to get attributes of a file or directory
static int wfs_getattr(const char *path, struct stat *stbuf)
{
    // clean path (remove pre mount + mount)
    path = remove_pre_mount(path);

    struct wfs_log_entry *log_entry = get_log_entry(path, 0);

    if(log_entry == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    // Update time of last access
    log_entry->inode.atime = time(NULL);

    stbuf->st_uid = log_entry->inode.uid;
    stbuf->st_gid = log_entry->inode.gid;
    stbuf->st_atime = log_entry->inode.atime;
    stbuf->st_mtime = log_entry->inode.mtime;
    stbuf->st_mode = log_entry->inode.mode;
    stbuf->st_nlink = log_entry->inode.links;
    stbuf->st_size = log_entry->inode.size;

    return 0;
}

// Function to create a regular file
static int wfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    path = remove_pre_mount(path);

    // Verify filename
    if (!valid_name(get_bottom_level(path)))
    {
        printf("Invalid File Name\n");
        return 0;
    }

    // Verify file doesn't exist in its intended parent dir
    if (!can_create(path))
    {
        printf("File Name Already In Use Locally.\n");
        return -EEXIST;
    }

    // Create a new inode for the file
    struct wfs_inode new_inode;
    inode_count += 1;

    new_inode.inode_number = inode_count;
    new_inode.deleted = 0;
    new_inode.mode = mode;
    new_inode.uid = getuid();
    new_inode.gid = getgid();
    new_inode.flags = 0;
    new_inode.size = sizeof(struct wfs_inode);
    new_inode.atime = time(NULL);
    new_inode.mtime = time(NULL);
    new_inode.ctime = time(NULL);
    new_inode.links = 1;

    // Create a new dentry for the file
    struct wfs_dentry *new_dentry = (struct wfs_dentry *)malloc(sizeof(struct wfs_dentry));
    if (new_dentry != NULL)
    {
        // Copy the filename to the new_dentry->name using a function like strncpy
        strncpy(new_dentry->name, get_bottom_level(path), MAX_FILE_NAME_LEN - 1);
        new_dentry->name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
        new_dentry->inode_number = new_inode.inode_number;
    }
    else
    {
        // Handle allocation failure
    }

    // Get parent directory log entry
    struct wfs_log_entry *old_log_entry = get_log_entry(snip_bottom_level(path), 0);

    if(old_log_entry == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    // perform size bounds checking
    // current size + size of copy of parent + size of new log entry
    if ((total_size + old_log_entry->inode.size + sizeof(struct wfs_dentry) + sizeof(struct wfs_log_entry)) > MAX_SIZE) {
        printf("Insufficient Disk Space to Perform Operation.\n");
        return -ENOSPC;
    }

    // Make a copy of the old log entry and add the created dentry to its data field
    struct wfs_log_entry *log_entry_copy = (struct wfs_log_entry *)malloc(old_log_entry->inode.size + sizeof(struct wfs_dentry));
    if (log_entry_copy != NULL)
    {

        // copy the entire old log entry (including it's data field) to the new log entry
        memcpy(log_entry_copy, old_log_entry, old_log_entry->inode.size);

        // add the dentry to log_entry_copy's data and update new log entry's size
        memcpy(log_entry_copy + log_entry_copy->inode.size, new_dentry, sizeof(struct wfs_dentry));
        log_entry_copy->inode.size += sizeof(struct wfs_dentry);

        // write the log entry copy to the log
        memcpy(head, log_entry_copy, log_entry_copy->inode.size);

        // update total size count
        total_size += log_entry_copy->inode.size;

        // update the head
        head += log_entry_copy->inode.size;
    }
    else
    {
        // Handle allocation failure
    }

    // Mark old log entry as deleted
    old_log_entry->inode.deleted = 1;

    // Create a log entry for the file itself
    struct wfs_log_entry *new_log_entry = (struct wfs_log_entry *)malloc(sizeof(struct wfs_log_entry));
    if (new_log_entry != NULL)
    {
        // point the log entry at the created inode
        // TODO should I use memcpy?
        // memccpy(new_log_entry, &new_inode, new_inode.size);
        new_log_entry->inode = new_inode;

        // add log entry to the log
        memcpy(head, new_log_entry, new_log_entry->inode.size);

        // update total size count
        total_size += new_log_entry->inode.size;

        // update the head
        head += new_log_entry->inode.size;
    }
    else
    {
        // Handle allocation failure
    }

    return 0;
}

// Function to create a directory
static int wfs_mkdir(const char *path, mode_t mode)
{
    path = remove_pre_mount(path);


    // Verify filename
    if (!valid_name(get_bottom_level(path)))
    {
        printf("Invalid Directory Name\n");
        return 0;
    }

    // Verify file doesn't exist in its intended parent dir
    if (!can_create(path))
    {
        printf("Directory Name Already In Use Locally.\n");
        return -EEXIST;
    }

    // create a new inode for the subdirectory
    struct wfs_inode new_inode;
    inode_count += 1;

    new_inode.inode_number = inode_count;
    new_inode.deleted = 0;
    new_inode.mode = mode;
    new_inode.uid = getuid();
    new_inode.gid = getgid();
    new_inode.flags = 0;
    new_inode.size = sizeof(struct wfs_inode);
    new_inode.atime = time(NULL);
    new_inode.mtime = time(NULL);
    new_inode.ctime = time(NULL);
    new_inode.links = 1;

    // Create a new dentry for the directory
    struct wfs_dentry *new_dentry = (struct wfs_dentry *)malloc(sizeof(struct wfs_dentry));
    if (new_dentry != NULL)
    {
        // Copy the dirname to the new_dentry->name using a function like strncpy
        strncpy(new_dentry->name, get_bottom_level(path), MAX_FILE_NAME_LEN - 1);
        new_dentry->name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
        new_dentry->inode_number = new_inode.inode_number;
    }
    else
    {
        // Handle allocation failure
    }

    // Get parent directory log entry
    struct wfs_log_entry *old_log_entry = get_log_entry(snip_bottom_level(path), 0);

    if(old_log_entry == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    
    // perform size bounds checking
    // current size + size of copy of parent + size of new log entry
    if ((total_size + old_log_entry->inode.size + sizeof(struct wfs_dentry) + sizeof(struct wfs_log_entry)) > MAX_SIZE) {
        printf("Insufficient Disk Space to Perform Operation.\n");
        return -ENOSPC;
    }

    // Make a copy of the old log entry and add the created dentry to its data field
    struct wfs_log_entry *log_entry_copy = (struct wfs_log_entry *)malloc(old_log_entry->inode.size + sizeof(struct wfs_dentry));
    if (log_entry_copy != NULL)
    {
        // copy the entire old log entry (including it's data field) to the new log entry
        memcpy(log_entry_copy, old_log_entry, old_log_entry->inode.size);

        // add the dentry to log_entry_copy's data and update new log entry's size
        memcpy(log_entry_copy + log_entry_copy->inode.size, new_dentry, sizeof(struct wfs_dentry));
        log_entry_copy->inode.size += sizeof(struct wfs_dentry);

        // write the log entry copy to the log
        memcpy(head, log_entry_copy, log_entry_copy->inode.size);

        // update total size count
        total_size += log_entry_copy->inode.size;

        // update the head
        head += log_entry_copy->inode.size;
    }
    else
    {
        // Handle allocation failure
    }

    // Mark old log entry as deleted
    old_log_entry->inode.deleted = 1;

    // Create a log entry for the file itself
    struct wfs_log_entry *new_log_entry = (struct wfs_log_entry *)malloc(sizeof(struct wfs_log_entry));
    if (new_log_entry != NULL)
    {
        // point the log entry at the created inode -- this will create a shallow copy of values (not a problem)
        new_log_entry->inode = new_inode;

        // add log entry to the log
        memcpy(head, new_log_entry, new_log_entry->inode.size);

        // update total size count
        total_size += new_log_entry->inode.size;

        // update the head
        head += new_log_entry->inode.size;
    }
    else
    {
        // Handle allocation failure
    }

    return 0;
}

// Function to read data from a file
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    path = remove_pre_mount(path);

    // Grab log entry for desired file
    struct wfs_log_entry *f = get_log_entry(path, 0);

    if(f == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    int data_size = f->inode.size - sizeof(struct wfs_log_entry);

    // Check if offset is too large
    if (offset >= data_size)
        return 0;

    // Read file data into buffer
    memcpy(buf, f->data + offset, size);
    f->inode.atime = time(NULL);
    return size;
}

// Function to write data to a file
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    path = remove_pre_mount(path);

    // Grab log entry for desired file
    struct wfs_log_entry *f = get_log_entry(path, 0);

    if(f == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    // end of log entry - start of data field
    int data_size = f->inode.size - sizeof(struct wfs_log_entry);
    f->inode.atime = time(NULL);

    // Check if write exceeds current size of file data
    if ((f->data + offset + size) >= (f->data + data_size))
    {
        // Set data_size to incorporate extra data
        ptrdiff_t diff = (f->data + offset + size) - f->data;
        data_size = (int)diff;
    }

    // Check if write wwould exceed disk space
    if ((total_size + sizeof(struct wfs_log_entry) + data_size) > MAX_SIZE){
        printf("Insufficient disk space\n");
        return -ENOSPC;
    }

    // allocate memory for a log entry copy
    struct wfs_log_entry *log_entry_copy = (struct wfs_log_entry *)malloc(sizeof(struct wfs_log_entry) + data_size);

    // memcpy old log into copy
    memcpy(log_entry_copy, f, f->inode.size);

    // mark old log entry as deleted
    f->inode.deleted = 1;

    // write buffer to offset of data
    memcpy(log_entry_copy->data + offset, buf, size);

    // change size field of new entry to be updated size
    log_entry_copy->inode.size += data_size;

    // update modify time
    log_entry_copy->inode.ctime = time(NULL);
    log_entry_copy->inode.mtime = time(NULL);

    // add log entry to head
    memcpy(head, log_entry_copy, log_entry_copy->inode.size);

    // update total size count
    total_size += log_entry_copy->inode.size;

    // update head
    head += log_entry_copy->inode.size;

    return size;
}

// Function to read directory entries
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    path = remove_pre_mount(path);

    struct wfs_log_entry *dir_log_entry = get_log_entry(path, 0);

    if(dir_log_entry == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    dir_log_entry->inode.atime = time(NULL);

    // incorporate offset as multiple of dentry's
    char *data_addr = dir_log_entry->data + (offset * sizeof(struct wfs_dentry));

    // iterate over all dentries
    while (data_addr != (char *)(dir_log_entry + dir_log_entry->inode.size))
    {
        struct wfs_dentry *curr_dentry = (struct wfs_dentry *)data_addr;

        size_t len1 = strlen(path);
        size_t len2 = strlen(curr_dentry->name);
        size_t totalLength = len1 + len2;

        // Allocate memory for the new string
        char *total_path = (char *)malloc(totalLength + 1); // +1 for the null terminator

        if (total_path == NULL)
        {
            // Handle memory allocation failure
            return -1;
        }

        // Copy the contents of the original strings to the new string
        strcpy(total_path, path);
        strcpy(total_path + len1, curr_dentry->name);

        struct wfs_log_entry *curr_log_entry = get_log_entry(total_path, 0);

        if(curr_log_entry == NULL) {
            printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
            return -ENOENT;
        }

        // Update time of last access
        curr_log_entry->inode.atime = time(NULL);

        // create a struct stat for the log entry
        struct stat stbuf;
        // populate stat struct
        stbuf.st_uid = curr_log_entry->inode.uid;
        stbuf.st_gid = curr_log_entry->inode.gid;
        stbuf.st_atime = curr_log_entry->inode.atime;
        stbuf.st_mtime = curr_log_entry->inode.mtime;
        stbuf.st_mode = curr_log_entry->inode.mode;
        stbuf.st_nlink = curr_log_entry->inode.links;
        stbuf.st_size = curr_log_entry->inode.size;

        offset += sizeof(struct wfs_dentry);
        if (filler(buf, curr_dentry->name, &stbuf, offset) != 0)
        {
            return 0;
        }

        data_addr += sizeof(struct wfs_dentry);
    }

    return 0;
}

// Function to unlink (delete) a file
static int wfs_unlink(const char *path)
{
    path = remove_pre_mount(path);

    // get parent log entry
    struct wfs_log_entry *parent_log_entry = get_log_entry(snip_bottom_level(path), 0);

    if(parent_log_entry == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    parent_log_entry->inode.atime = time(NULL);

    // perform size bounds checking
    // current size + size of copy of parent
    if ((total_size + parent_log_entry->inode.size - sizeof(struct wfs_dentry)) > MAX_SIZE) {
        printf("Insufficient Disk Space to Perform Operation.\n");
        return -ENOSPC;
    }

    // get target log entry
    struct wfs_log_entry *log_entry = get_log_entry(path, 0);

    if(log_entry == NULL) {
        printf("Log Entry Associated With Path Does Not Exist.\nPath: %s\n", path);
        return -ENOENT;
    }

    log_entry->inode.atime = time(NULL);

    // mark file log entry as deleted
    log_entry->inode.deleted = 1;
    // decrement inode links count
    log_entry->inode.links -= 1;
    // update inode change time
    log_entry->inode.ctime = time(NULL);

    // Make a copy of the old parent log entry and remove the files dentry from the new data member (don't copy it over)
    struct wfs_log_entry *log_entry_copy = (struct wfs_log_entry *)malloc(parent_log_entry->inode.size - sizeof(struct wfs_dentry));
    if (log_entry_copy != NULL)
    {
        // find the address offset of the dentry for this file
        char *data_addr = parent_log_entry->data;
        // iterate over all dentry's
        while (data_addr != (char *)(parent_log_entry + parent_log_entry->inode.size))
        {
            // check the inode number against the target log entry (deleted file)
            if (((struct wfs_dentry *)data_addr)->inode_number == log_entry->inode.inode_number)
            {
                break;
            }
            data_addr += sizeof(struct wfs_dentry);
        }

        // memcpy up to target file's dentry to delete
        memcpy(log_entry_copy, parent_log_entry, data_addr - (char *)(parent_log_entry));

        // memcpy after the deleted dentry -- if this check does not hit, it means the deleted file was the last dentry of the parent log entry
        if (data_addr != (char *)(parent_log_entry->inode.size - sizeof(struct wfs_dentry)))
        {
            // the start of the remaining data portion of the copy of the parent's log entry
            char *data_start_addr = (char *)(log_entry_copy + (data_addr - (char *)(parent_log_entry)));
            // the address of the remaining data in the original parent (after the deleted target file's dentry)
            char *parent_after_data_addr = data_addr + sizeof(struct wfs_dentry);
            // the size remaining after the child's dentry
            uint parent_remaining_size = (char *)(parent_log_entry + parent_log_entry->inode.size) - parent_after_data_addr;

            memcpy(data_start_addr, parent_after_data_addr, parent_remaining_size);
        }
        // mark parent as deleted
        parent_log_entry->inode.deleted = 1;
        // update size of the new log entry
        log_entry_copy->inode.size -= sizeof(struct wfs_dentry);

        // write the log entry copy to the log
        memcpy(head, log_entry_copy, log_entry_copy->inode.size);

        // update total size count
        total_size += log_entry_copy->inode.size;

        // update the head
        head += log_entry_copy->inode.size;
    }
    else
    {
        // Handle allocation failure
    }

    return 0;
}

static struct fuse_operations my_operations = {
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .read = wfs_read,
    .write = wfs_write,
    .readdir = wfs_readdir,
    .unlink = wfs_unlink,
};

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s [FUSE options] disk_path mount_point\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse disk_path and mount_point from the command line arguments
    disk_path = argv[argc - 2];
    mount_point = argv[argc - 1];

    int fd;

    // Open file descriptor for file to init system with
    fd = open(disk_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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
    head = (char *)&superblock->head;

    // FUSE options are passed to fuse_main, starting from argv[1]
    // int fuse_argc = argc - 2; // Adjust argc for FUSE options
    // char **fuse_argv = argv + 1;

    // // Disable multi-threading with -s option
    // fuse_argv[0] = "-s";

    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    // Call fuse_main with your FUSE operations and data
    fuse_main(argc, argv, &my_operations, NULL);
    munmap(base, file_stat.st_size);

    return 0;
}
