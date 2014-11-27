#include <stdio.h>
#include <stdlib.h>

#include "sfs_api.h"
#include "disk_emu.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Indicate freshness\n");
    return (1);
  }
  int fresh = atoi(argv[1]);

  if (mksfs(fresh) != 0) {
    printf("Failed to init the file system\n");
    return (1);
  }

  if (fresh) {
    if (sfs_fopen("test.txt") == -1) {
      printf("Failed to create file\n");
      return (1);
    }   
  } else {
#if 0
    if (sfs_fopen("test.pdf") == -1) {
      printf("Failed to create file\n");
      return (1);
    }
#endif
  }

  sfs_ls();

  close_disk();
}
