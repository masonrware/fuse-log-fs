#include <stdio.h>
#include <string.h>
#include "common/test.h"
int main() {
  FILE* fp;
  fp = fopen("mnt/data11.txt", "w");
  if (fp < 0) return FAIL;

  fprintf(fp, "Hello");

  fclose(fp);

  fp = fopen("mnt/data11.txt", "r");
  char buffer[5] = {0};

  int bytesRead = fread(buffer, 1, sizeof(buffer), fp);

  const char* content = "Hello";

  if (bytesRead != strlen(content) || strcmp(content, buffer) != 0) {
    fclose(fp);
    printf("Wrong content! File contains %s\n", buffer);
    return FAIL;
  }
  fclose(fp);

  return PASS;
}