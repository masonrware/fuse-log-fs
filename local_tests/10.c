#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "common/test.h"

#define BLOCK_SIZE 32
#define FILE_SIZE 1024

int main() {
  char buffer[BLOCK_SIZE] = {0};
  char read_buffer[BLOCK_SIZE] = {0};
  int is_nospace = 0;
  for (int i = 0; i < FILE_SIZE / BLOCK_SIZE; i++) {
    for (int j = 0; j < BLOCK_SIZE; j++) {
      buffer[j] = rand() % 256;
    }
    FILE* fp = fopen("mnt/fsckfile1.txt", "wb");
    if (fp == NULL) {
      if (errno == ENOSPC) {
        is_nospace = 1;
        break;
      }
      printf("Failed to fopen file\n");
      perror("fopen");
      return FAIL;
    }
    int res = fwrite(buffer, 1, sizeof(buffer), fp);
    if (res != sizeof(buffer)) {
      // if (i < (1 << 10)) {
      //   printf("Write failed after %d writes of 128B with errno %d\n", i,
      //          errno);
      // }
      if (errno == ENOSPC) {
        printf("Disk is full after %d writes\n", i);
        is_nospace = 1;
        break;
      }
      printf("Write failed with errno %d\n", errno);
      perror("write");
      return FAIL;
    }
    fclose(fp);
    fp = fopen("mnt/fsckfile1.txt", "rb");
    int read_byte = fread(read_buffer, 1, sizeof(buffer), fp);
    if (read_byte != sizeof(buffer) ||
        memcmp(buffer, read_buffer, sizeof(buffer)) != 0) {
      fclose(fp);
      printf("File content is different from what was written\n");
      return FAIL;
    }
    fclose(fp);
    if (remove("mnt/fsckfile1.txt") != 0) {
      if (errno == ENOSPC) {
        printf("Disk is full after %d writes\n", i);
        is_nospace = 1;
        break;
      }
      perror("Unable to delete file");
      return FAIL;
    }
  }

  if (!is_nospace) {
    printf("Write did not fail with ENOSPC after %u writes of %u bytes\n", FILE_SIZE/BLOCK_SIZE, BLOCK_SIZE);
    return FAIL;
  }

  int ret = system("./fsck.wfs disk");
  if (ret != 0) {
    printf("fsck.wfs failed with exit code %d\n", ret);
    return FAIL;
  }

  FILE* fp = fopen("mnt/fsckfile2.txt", "wb");
  if (fp == NULL) {
    printf("Failed to open file after fsck.wfs\n");
    return FAIL;
  }

  int res = fwrite(buffer, 1, sizeof(buffer), fp);
  if (res != sizeof(buffer)) {
    printf("Write after fsck failed with errno %d\n", errno);
    perror("write");
    return FAIL;
  }
  fclose(fp);
  fp = fopen("mnt/fsckfile2.txt", "rb");
  int read_byte = fread(read_buffer, 1, sizeof(buffer), fp);
  if (read_byte != sizeof(buffer) ||
      memcmp(buffer, read_buffer, sizeof(buffer)) != 0) {
    fclose(fp);
    printf("After fsck, File content is different from what was written\n");
    return FAIL;
  }

  fclose(fp);

  return PASS;
}
