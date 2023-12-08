#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/test.h"
int main(int argc, char** argv) {
  const char* disk_path = "disk";
  if (argc == 2) {
    disk_path = argv[1];
  } else if (argc > 2) {
    printf("Usage: %s <disk image>\n", argv[0]);
    return INTERNAL_ERR;
  }

  int fd = open(disk_path, O_RDWR);
  if (fd < 0) {
    perror("open");
    return INTERNAL_ERR;
  }

  struct wfs_sb sb;
  if (read(fd, &sb, sizeof(sb)) != sizeof(sb)) {
    perror("read");
    return INTERNAL_ERR;
  }

  if (sb.magic != WFS_MAGIC) {
    printf("Wrong magic number (0x%x)\n", sb.magic);
    return FAIL;
  }

  if (sb.head != sizeof(sb) + sizeof(struct wfs_log_entry)) {
    printf("Wrong head (0x%x)\n", sb.head);
    return FAIL;
  }

  struct wfs_log_entry root;
  if (read(fd, &root, sizeof(root)) != sizeof(root)) {
    perror("read");
    return INTERNAL_ERR;
  }

  if (root.inode.inode_number != 0) {
    printf("Wrong inode number (0x%x)\n", root.inode.inode_number);
    return FAIL;
  }

  if (root.inode.deleted != 0) {
    printf("Wrong deleted (0x%x)\n", root.inode.deleted);
    return FAIL;
  }

  if (!(root.inode.mode & S_IFDIR)) {
    printf("Wrong mode (0x%x)\n", root.inode.mode);
    return FAIL;
  }

  close(fd);

  return PASS;
}