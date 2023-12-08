#include <stdint.h>

#define PASS 0
#define FAIL 1
#define INTERNAL_ERR 2
#define MAX_FILE_NAME_LEN 32
#define WFS_MAGIC 0xdeadbeef
struct wfs_sb {
  uint32_t magic;
  uint32_t head;
};
struct wfs_inode {
  unsigned int inode_number;
  unsigned int deleted;
  unsigned int mode;
  unsigned int uid;
  unsigned int gid;
  unsigned int flags;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;
  unsigned int links;
};
struct wfs_dentry {
  char name[MAX_FILE_NAME_LEN];
  unsigned long inode_number;
};
struct wfs_log_entry {
  struct wfs_inode inode;
  char data[];
};