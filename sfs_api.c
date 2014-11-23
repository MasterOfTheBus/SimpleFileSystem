#include <time.h>

#include "sfs_api.h"
#include "disk_emu.h"

#define DISK_FILE "CCDisk"
#define BLOCKSIZE 512
#define NUM_BLOCKS 1024 // so .5 GB?
#define FAT_SIZE 5 // temp values
#define ROOT_SIZE 5 // temp values
#define MAX_FILES 50 // temp values
#define MAX_FILE_SIZE 1024 // temp values

#define err_msg(msg) \
  perror(msg); return (EXIT_FAILURE);

struct dir_entry {
  unsigned int file_size;
  time_t created;
  time_t last_modified;
};

struct fd_entry {
  int read_ptr;
  int write_ptr;
  int file_fat_root; // maybe need different data type
};

struct fat_entry {
  unsigned int data_block; // maybe need different data type
  int next_entry;
};

int mksfs(int fresh) {

  // init the disk
  if (fresh) {
    if (init_fresh_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
      err_msg("fresh disk init");

    // create the in memory tables
    directory_table = map_create(NULL);
    if (!directory_table) {
      err_msg("map create");
    }

    file_descriptor_table = map_create(NULL);
    if (!file_descriptor_table) {
      err_msg("map create");
    }

    file_allocation_table = map_create(NULL);
    if (!file_allocation_table) {
      err_msg("map create");
    }

    // create the disk tables

    // super block
    int super_block[] = {ROOT_SIZE, FAT_SIZE, 9989, 0}; // 9989 is temporary
    if (write_blocks(0, 1, super_block) == -1) {
      err_msg("write blocks");
    }

    // directory blocks - do i just store the map into the disk?
    if (write_blocks(1, ROOT_SIZE, directory_table) == -1) {
      err_msg("write blocks");
    }
    
    // FAT blocks
    if (write_blocks(1+ROOT_SIZE, FAT_SIZE, file_allocation_table) == -1) {
      err_msg("write blocks");
    }

  } else {
    if (init_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
      err_msg("disk init");

    // read the tables from disk

    // cache the disk tables into memory
  }

  return 0;

}

void sfs_ls(void) {

}

int sfs_fopen(char *name) {

}

int sfs_fclose(int fileID) {

}

int sfs_fwrite(int fileID, char *buf, int length) {

}

int sfs_fread(int fileID, char *buf, int length) {

}

int sfs_fseek(int fileID, int offset) {

}

int sfs_remove(char *file) {

}

