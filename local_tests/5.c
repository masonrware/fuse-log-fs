#include <stdio.h>
#include "common/test.h"
int main() {
  FILE *fp;
  fp = fopen("mnt/./data8.txt", "w");
  if (fp < 0)
    return FAIL;

  else {
    fclose(fp);
    return PASS;
  }
}