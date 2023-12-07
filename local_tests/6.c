#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common/test.h"

int main() {
  const char *path = "mnt/data5";

  int status = mkdir(path, S_IRWXU);

  if (status == 0) {
    printf("Directory created inside root successfully.\n");
  } else {
    perror("Unable to create directory");
    return FAIL;
  }

  FILE *fp;
  fp = fopen("mnt/data5/../data9.txt", "w");
  if (fp < 0)
    return FAIL;
  else {
    fclose(fp);
    return PASS;
  }
}