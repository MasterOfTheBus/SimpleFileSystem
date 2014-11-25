#include <time.h>

#include "sfs_api.h"
#include "disk_emu.h"

#define DISK_FILE "CCDisk"
#define BLOCKSIZE 512
#define NUM_BLOCKS 1024 // so .5 GB?
#define SUPER_BLOCK 0
#define FAT_SIZE 3 // temp values
#define ROOT_SIZE 4 // temp values
#define MAX_FILES 50 // temp values
#define MAX_FILE_SIZE 1024 // temp values

#define err_msg(msg) \
  perror(msg); return (EXIT_FAILURE);

struct directory_entry {
  unsigned int file_size;
  time_t created;
  time_t last_modified;
};

struct file_descriptor_entry {
  int read_ptr;
  int write_ptr;
  int file_fat_root; // maybe need different data type
};

struct file_allocation_entry {
  unsigned int data_block; // maybe need different data type
  int next_entry;
};

//==================Helper Methods====================

//==================Directory Methods================
// enum for the block number of the directory table entries
typedef enum {
    FILE_NAME = 1,
    FILE_SIZE,
    CREATED,
    LAST_MODIFIED
} DirBlockNum;

void dirToStr(char* str, DirEntry *de) {
    snprintf(str, 200, "file size: %u, created: %s, modified: %s\n", de->file_size, ctime(&de->created),
	     ctime(&de->last_modified));
}

int writeDirToDisk(Map *map) {
    unsigned int size = map_size(map);

    if (size == -1) {
	printf("Failed to get map size\n");
	return (-1);
    }
    if (size == 0) {
	unsigned int file_size[] = {0};
	if (write_blocks(FILE_SIZE, 1, file_size) == -1) {
	    printf("Failed to write blocks\n");
	    return (-1);
	}
	return (0);
    }

    char* names[size];
    unsigned int file_size[size+1];
    time_t created[size];
    time_t modified[size];

    Mapper *mapper;
    mapper = mapper_create(map);
    if (!mapper) {
	map_destroy(&map);
	return -1;
    }

    file_size[0] = size;

    int i = 0;
    while (mapper_has_next(mapper) == 1) {
	const Mapping *mapping = mapper_next_mapping(mapper);
	DirEntry* de = (DirEntry*)mapping_value(mapping);
	names[i] = (char *)mapping_key(mapping);
	file_size[i+1] = de->file_size;
	created[i] = de->created;
	modified[i] = de->last_modified;
    }

    if (write_blocks(FILE_NAME, 1, names) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }
    if (write_blocks(FILE_SIZE, 1, file_size) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }
    if (write_blocks(CREATED, 1, created) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }
    if (write_blocks(LAST_MODIFIED, 1, modified) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }

    mapper_destroy(&mapper);
    return 0;
}

int readDirFromDisk(Map *map) {
    unsigned int size = map_size(map);

    char* names[size];
    unsigned int file_size[size+1];
    time_t created[size];
    time_t modified[size];
 
    if (read_blocks(FILE_NAME, 1, names) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(FILE_SIZE, 1, file_size) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(CREATED, 1, created) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(LAST_MODIFIED, 1, modified) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }

    int i = 0;
    for (; i < size; i++) {
	DirEntry* de = malloc(sizeof(DirEntry));
	de->file_size = file_size[i+1];
	de->created = created[i];
	de->last_modified = modified[i];
	map_add(map, names[i], de);
    }

    return (0);
}

//==================End Helper Methods===================

int mksfs(int fresh) {

    // create the in memory tables
    directory_table = map_create(free);
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

    // init the disk
    if (fresh) {
	if (init_fresh_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
	    printf("Couldn't init fresh disk\n");

	// super block
	int super_block[] = {ROOT_SIZE, FAT_SIZE, 9989, 0}; // 9989 is temporary
	if (write_blocks(0, 1, super_block) == -1) {
	    err_msg("write blocks");
	}

	if (writeDirToDisk(directory_table) == -1) {
	    err_msg("write to disk");
	}

    } else {
	if (init_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
	    err_msg("disk init");

	// read the tables from disk
	if (readDirFromDisk(directory_table) == -1) {
	    err_msg("read from disk");
	}
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

