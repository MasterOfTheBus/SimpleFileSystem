#include <stdio.h>

#include "sfs_api.h"

int main() {
  if (mksfs(1) != 0) {
    printf("Failed to init the file system\n");
  }

  if (sfs_fopen("test.txt") == -1) {
    printf("Failed to create file\n");
    return (1);
  }

  sfs_ls();

  close_disk();
}
