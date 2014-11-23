#include <stdio.h>

#include "sfs_api.h"

int main() {
  if (mksfs(1) != 0) {
    printf("Failed to init the file system\n");
  }
}
