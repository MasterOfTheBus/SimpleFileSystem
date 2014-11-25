#include <time.h>

#include "sfs_api.h"
#include "disk_emu.h"

#define DISK_FILE "CCDisk"
#define BLOCKSIZE 512
#define NUM_BLOCKS 1024 // so .5 GB?
#define SUPER_BLOCK 0
#define FAT_SIZE 2 // temp values
#define ROOT_SIZE 4 // temp values
#define FREE_SIZE 2
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

struct free_block_list_entry {
  int block;
  int next;
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
    sprintf(str, "file size: %u, created: %s, modified: %s", de->file_size, ctime(&de->created),
	     ctime(&de->last_modified));
}

int writeDirToDisk(Map *map) {
    unsigned int size = map_size(map);

    if (size == -1) {
	printf("Failed to get map size\n");
	return (-1);
    }
    if (size == 0) {
	char* names[] = { "" };
	if (write_blocks(FILE_NAME, 1, names) == -1) {
	    printf("Failed to write blocks\n");
	    return (-1);
	}
	return (0);
    }

    char* names[size];
    unsigned int file_size[size];
    time_t created[size];
    time_t modified[size];

    Mapper *mapper;
    mapper = mapper_create(map);
    if (!mapper) {
	map_destroy(&map);
	return -1;
    }

    int i = 0;
    while (mapper_has_next(mapper) == 1) {
	const Mapping *mapping = mapper_next_mapping(mapper);
	DirEntry* de = (DirEntry*)mapping_value(mapping);
	names[i] = (char *)mapping_key(mapping);
	file_size[i] = de->file_size;
	created[i] = de->created;
	modified[i] = de->last_modified;
	i++;
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

    char* names[BLOCKSIZE];
    unsigned int file_size[BLOCKSIZE];
    time_t created[BLOCKSIZE];
    time_t modified[BLOCKSIZE];
 
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
    for (; i < BLOCKSIZE; i++) {
	if (names[i] == "") {
	    break;
	}
	DirEntry* de = malloc(sizeof(DirEntry));
	de->file_size = file_size[i];
	de->created = created[i];
	de->last_modified = modified[i];
	map_add(map, names[i], de);
    }

    return (0);
}

//==================FAT Methods=========================
typedef enum {
    DATA_BLOCK = 5,
    NEXT_ENTRY,
} FatBlockNum;

void FatToStr(char* str, FatEntry *fe) {
    sprintf(str, "data block: %u, next entry: %d", fe->data_block, fe->next_entry);
}

int writeFatToDisk(Map *map) {
    unsigned int size = map_size(map);

    if (size == -1) {
	printf("Failed to get map size\n");
	return (-1);
    }

    unsigned int data[size];
    int next[size];

    Mapper *mapper;
    mapper = mapper_create(map);
    if (!mapper) {
	map_destroy(&map);
	return -1;
    }

    int i = 0;
    while (mapper_has_next(mapper) == 1) {
	const Mapping *mapping = mapper_next_mapping(mapper);
	FatEntry* fe = (FatEntry*)mapping_value(mapping);
	data[i] = fe->data_block;
	next[i] = fe->next_entry;
	i++;
    }

    if (write_blocks(DATA_BLOCK, 1, data) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }
    if (write_blocks(NEXT_ENTRY, 1, next) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }

    mapper_destroy(&mapper);
    return 0;
}

int readFatFromDisk(Map *map) {

    unsigned int data[BLOCKSIZE];
    int next[BLOCKSIZE];
 
    if (read_blocks(DATA_BLOCK, 1, data) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(NEXT_ENTRY, 1, next) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }

    int i = 0;
    for (; i < BLOCKSIZE; i++) {
	if ((data[i] == 0) && (next[i] == 0)) {
	    break;
	}
	FatEntry* fe = malloc(sizeof(FatEntry));
	fe->data_block = data[i];
	fe->next_entry = next[i];
	char key[5];
	sprintf(key, "%d", i);
	map_add(map, key, fe);
    }

    return (0);
}

//==================Free Methods=========================
typedef enum {
    BLOCK = 7,
    NEXT
} FreeBlockNum;

int list_size = 0;

void freeEntryToStr(char* str, FreeEntry fEntry) {
    sprintf(str, "block: %d, next: %d", fEntry.block, fEntry.next);
}

int writeFreeToDisk(List *list) {

    int size = (list_size == 0) ? BLOCKSIZE : list_size;
    int block[size];
    int next[size];

    if (list_size == 0) {
	int i = 0;
	for (; i < BLOCKSIZE; i++) {
	    block[i] = 0;
	    next[i] = 0;
	}
    } else {
	int i = 0;
	while (list_has_next(list) == 1) {
	    FreeEntry* fen = (FreeEntry*)list_next(list);
	    block[i] = fen->block;
	    next[i] = fen->next;
	    i++;
	}
    }

    if (write_blocks(BLOCK, 1, block) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }
    if (write_blocks(NEXT, 1, next) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }

    return 0;
}

int readFreeFromDisk(List *list) {

    int block[BLOCKSIZE];
    int next[BLOCKSIZE];
 
    if (read_blocks(BLOCK, 1, block) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(NEXT, 1, next) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }

    int i = 0;
    for (; i < BLOCKSIZE; i++) {
	if (block[i] == -1 || block[i] == 0) {
	    break;
	}
	FreeEntry *fen = malloc(sizeof(FreeEntry));
	fen->block = block[i];
	fen->next = next[i];
	list_append(free_block_list, fen);
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

    file_allocation_table = map_create(free);
    if (!file_allocation_table) {
	err_msg("map create");
    }

    free_block_list = list_create(free);
    if (!free_block_list) {
	err_msg("map create");
    }

    // init the disk
    if (fresh) {
	int free_blocks = (NUM_BLOCKS - FAT_SIZE - FREE_SIZE - ROOT_SIZE-1);
	if (init_fresh_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
	    printf("Couldn't init fresh disk\n");

	// super block
	int super_block[] = {ROOT_SIZE, FAT_SIZE, free_blocks, 0};
	if (write_blocks(0, 1, super_block) == -1) {
	    err_msg("write blocks");
	}

	if (writeDirToDisk(directory_table) == -1) {
	    err_msg("write to disk");
	}

	int i = 0;
	int block = NEXT+1;
	for (; i < free_blocks; i++) {
	    FreeEntry* fen = malloc(sizeof(FreeEntry));
	    if (i == 0) fen->block = block;
	    fen->next = block++;
	    if (i == free_blocks-1) fen->next = -1;
	    if (!(list_append(free_block_list, fen))) {
		err_msg("map add");
	    }
	    list_size++;
	}

	if (writeFreeToDisk(free_block_list) == -1) {
	    err_msg("write to disk");
	}

    } else {
	if (init_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
	    err_msg("disk init");

	// read the tables from disk
	if (readDirFromDisk(directory_table) == -1) {
	    err_msg("read from disk");
	}

	if (readFatFromDisk(file_allocation_table) == -1) {
	    err_msg("read from disk");
	}

	if (readFreeFromDisk(free_block_list) == -1) {
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

