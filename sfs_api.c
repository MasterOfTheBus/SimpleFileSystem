#include <time.h>
#include <string.h>

#include "sfs_api.h"
#include "disk_emu.h"

#define DISK_FILE "CCDisk"
#define BLOCKSIZE 512
#define NUM_BLOCKS 1024 // so .5 GB?
#define SUPER_BLOCK 0
#define FAT_SIZE 3 // temp values
#define ROOT_SIZE 5 // temp values
#define FREE_SIZE 2
#define MAX_FILES 50 // temp values
#define MAX_FILE_SIZE 1024 // temp values
#define MAX_NAME_LENGTH 50

#define err_msg(msg) \
  perror(msg); return (EXIT_FAILURE);

struct directory_entry {
  unsigned int file_size;
  time_t created;
  time_t last_modified;
  int file_fat_root;
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

int fat_index;
int fd_index;
int free_blocks;
int data_blocks;

//===================Helper Methods======================

//===============Misc Helper=============================
void *int_copy(const void *key)
{
    return (void *)key;
}

int int_cmp(const void *a, const void *b)
{
    return (int)a - (int)b;
}

size_t int_hash(size_t size, const void *key)
{
    return (int)key % size;
}

int updateSuperBlock() {
    int super_block[] = {ROOT_SIZE, FAT_SIZE, free_blocks, data_blocks, -1};
    if (write_blocks(SUPER_BLOCK, 1, super_block) == -1) {
	return (-1);
    }
    return (0);
}

int readSuperBlock() {
    int super[BLOCKSIZE];
    if (read_blocks(SUPER_BLOCK, 1, super) == -1) {
	return (-1);
    }
    free_blocks = super[2];
    data_blocks = super[3];
    return (0);
}

int writeToDiskStructures() {
    if (writeFatToDisk(file_allocation_table) == -1) {
	return (-1);
    }
    if (writeDirToDisk(directory_table) == -1) {
	return (-1);
    }
    if (writeFreeToDisk(free_block_list) == -1) {
	return (-1);
    }
    return (0);
}

//==================Directory Methods================
// enum for the block number of the directory table entries
typedef enum {
    FILE_NAME = 1,
    FILE_SIZE,
    CREATED,
    LAST_MODIFIED,
    FILE_FAT_ROOT
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

    int names_size = (size >= BLOCKSIZE) ? BLOCKSIZE : size+1;

    char names[names_size][MAX_NAME_LENGTH];
    unsigned int file_size[size];
    time_t created[size];
    time_t modified[size];
    int fat_root[size];

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
	strcpy(names[i], (char *)mapping_key(mapping));
	file_size[i] = de->file_size;
	created[i] = de->created;
	modified[i] = de->last_modified;
	fat_root[i] = de->file_fat_root;
	i++;
    }
    
    if (size < BLOCKSIZE) {
        strcpy(names[i], "\0");
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
    if (write_blocks(FILE_FAT_ROOT, 1, fat_root) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }

    mapper_destroy(&mapper);
    return 0;
}

int readDirFromDisk(Map *map) {

    char names[BLOCKSIZE][MAX_NAME_LENGTH];
    unsigned int file_size[BLOCKSIZE];
    time_t created[BLOCKSIZE];
    time_t modified[BLOCKSIZE];
    int fat_root[BLOCKSIZE];

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
    if (read_blocks(FILE_FAT_ROOT, 1, fat_root) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }

    int i = 0;
    for (; i < BLOCKSIZE; i++) {
	if ((names[i] == "") || !names[i] || (strlen(names[i]) == 0)) {
	    break;
	}
	DirEntry* de = malloc(sizeof(DirEntry));
	de->file_size = file_size[i];
	de->created = created[i];
	de->last_modified = modified[i];
	de->file_fat_root = fat_root[i];
	map_add(map, names[i], de);
    }

    return (0);
}

//==================FAT Methods=========================

typedef enum {
    FAT_INDEX = 6,
    DATA_BLOCK,
    NEXT_ENTRY
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

    int index[size];
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
	index[i] = (int)mapping_key(mapping);
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
    if (write_blocks(FAT_INDEX, 1, index) == -1) {
	printf("Failed to write blocks\n");
	return (-1);
    }

    mapper_destroy(&mapper);
    return 0;
}

int readFatFromDisk(Map *map) {

    unsigned int data[BLOCKSIZE];
    int next[BLOCKSIZE];
    int index[BLOCKSIZE];

    if (read_blocks(DATA_BLOCK, 1, data) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(NEXT_ENTRY, 1, next) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }
    if (read_blocks(FAT_INDEX, 1, index) == -1) {
	printf("Failed to read blocks\n");
	return (-1);
    }

    int i = 0;
    int max = -1;
    for (; i < BLOCKSIZE; i++) {
	if ((data[i] == 0) && (next[i] == 0)) {
	    break;
	}
	FatEntry* fe = malloc(sizeof(FatEntry));
	fe->data_block = data[i];
	fe->next_entry = next[i];
	map_add(map, (void*)index[i], fe);
	if (index[i] > max) {
	    max = index[i];
	}
    }
    fat_index = max;

    return (0);
}

//==================Free Methods=========================
typedef enum {
    BLOCK = 9,
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
    fd_index = 0;

    // create the in memory tables
    directory_table = map_create(free);
    if (!directory_table) {
	err_msg("map create");
    }

    file_descriptor_table = map_create_generic(int_copy, int_cmp, int_hash,
					       NULL, free);
   if (!file_descriptor_table) {
	err_msg("map create");
    }

    file_allocation_table = map_create_generic(int_copy, int_cmp, int_hash,
					       NULL, free);
    if (!file_allocation_table) {
	err_msg("map create");
    }

    free_block_list = list_create(free);
    if (!free_block_list) {
	err_msg("map create");
    }

    // init the disk
    if (fresh) {
	free_blocks =(NUM_BLOCKS - FAT_SIZE - FREE_SIZE - ROOT_SIZE-1);
	fat_index = 0;
	data_blocks = 0;
	if (init_fresh_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1)
	    printf("Couldn't init fresh disk\n");

	// super block
	if (updateSuperBlock() == -1) {
	    err_msg("write to disk");
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
	if (init_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1) {
	    printf("disk init failed\n");
	    return (-1);
	}

	if (readSuperBlock() == -1) {
	    printf("Failed to read from disk");
	    return (-1);
	}

	// read the tables from disk
	if (readDirFromDisk(directory_table) == -1) {
	    err_msg("read from disk");
	}

	if (readFatFromDisk(file_allocation_table) == -1) {
	    err_msg("read from disk");
	}

	fat_index = map_size(file_allocation_table) + 1;

	if (readFreeFromDisk(free_block_list) == -1) {
	    err_msg("read from disk");
	}
    }

    return 0;
}

void sfs_ls(void) {
    Mapper *mapper;
    mapper = mapper_create(directory_table);
    if (!mapper) {
	printf("Could not list directory entries");
	return;	
    }

    while (mapper_has_next(mapper) == 1) {
	const Mapping *mapping = mapper_next_mapping(mapper);
	DirEntry* de = (DirEntry*)mapping_value(mapping);
	char * created = strtok(ctime(&de->created), "\n");
	char * modified = strtok(ctime(&de->last_modified), "\n");
	printf("%s %s %u %s\n", created, modified, de->file_size,
		(char*)mapping_key(mapping));
    }

    mapper_destroy(&mapper);
}

int sfs_fopen(char *name) {

    if (strlen(name) > MAX_NAME_LENGTH) {
	printf("Name length exceeds maximum allowed length of %d characters\n",
		MAX_NAME_LENGTH);
	return (-1);
    }

    // check if it exists in the directory
    DirEntry* dEntry = map_get(directory_table, name);
    if (!dEntry) {
	// create entry in fat
	// find free block
	FreeEntry* free_entry = list_shift(free_block_list);
	if (!free_entry) {
	    err_msg("Could not remove item from list");
	}
	FatEntry* fat_entry = malloc(sizeof(FatEntry));
	fat_entry->data_block = free_entry->block;
	fat_entry->next_entry = -1;
	free(free_entry);
	fat_index++;
	if (map_add(file_allocation_table, (void*)fat_index, fat_entry) == -1) {
	    fat_index--;
	    err_msg("Could not add new Fat entry");
	}

	// create new entry in file descriptor table, set pointers and return fd
	FdEntry* fd = malloc(sizeof(FdEntry));
	fd->read_ptr = 0;
	fd->write_ptr = 0;
	fd->file_fat_root = fat_index;
	printf("fat_index: %d, file_fat_root: %d\n", fat_index, fd->file_fat_root);
	fd_index++;
	if (map_add(file_descriptor_table, (void*)fd_index, fd) == -1) {
	    fd_index--;
	    err_msg("Could not add new fd entry");
	}

	// does not exist then create new directory entry
	dEntry = malloc(sizeof(DirEntry));
	dEntry->file_size = 0;
	time_t timer = time(NULL);
	dEntry->created = timer;
	dEntry->last_modified = timer;
	dEntry->file_fat_root = fat_index;
	if (map_add(directory_table, name, dEntry) == -1) {
	    err_msg("Could not add new directory entry");
	}

	// modify the content on disk
	if (writeToDiskStructures() == -1) {
	    err_msg("Could not write to disk");
	}
	data_blocks++;
	free_blocks--;
	if (updateSuperBlock() == -1) {
	    err_msg("Could not write to disk");
	}
    } else {
        // exists then create a file descriptor table entry for it, set wirte pointer to eof and return fd
	FdEntry* fd = malloc(sizeof(FdEntry));
	fd->read_ptr = 0;
	fd->write_ptr = dEntry->file_size;
	fd->file_fat_root = dEntry->file_fat_root;

	fd_index++;
	if (map_add(file_descriptor_table, (void*)fd_index, fd) == -1) {
	    fd_index--;
	    return (-1);
	}
    }
    return (fd_index);
}

int sfs_fclose(int fileID) {
    FdEntry* fd = map_get(file_descriptor_table, (void*)fileID);
    if (!fd) {
	printf("Fd does not exist\n");
	return (-1);
    }
    if (map_remove(file_descriptor_table, (void*)fileID) == -1) {
	printf("Could not close file descriptor: %d\n", fileID);
	return (-1);
    }
    return (0);
}

int sfs_fwrite(int fileID, char *buf, int length) {

}

int sfs_fread(int fileID, char *buf, int length) {

}

int sfs_fseek(int fileID, int offset) {

}

int sfs_remove(char *file) {
    DirEntry* dir_entry = map_get(directory_table, file);
    if (!dir_entry) {
	printf("File (%s) does not exist\n", file);
	return (-1);
    }
    int fat_index = dir_entry->file_fat_root;

    while (fat_index != -1) {
	FatEntry* fat_entry = map_get(file_allocation_table, (void*)fat_index);
	if (!fat_entry) {
	    printf("Error getting fat entry\n");
	    return (-1);
	}
	if (map_remove(file_allocation_table, (void*)fat_index) == -1) {
	    printf("Error deleting file allocation block\n");
	    return (-1);
	}

	FreeEntry* free_entry = malloc(sizeof(FreeEntry));
	free_entry->block = fat_entry->data_block;
	free_entry->next = -1;
	ssize_t index = list_last(free_block_list);
	if (index != -1) {
	    FreeEntry* last_entry = list_item(free_block_list, index);
	    if (!last_entry) {
		printf("Error putting blocks back in free list\n");
		return (-1);
	    }
	    last_entry->next = fat_entry->data_block;
	}
	list_append(free_block_list, free_entry); 

	fat_index = fat_entry->next_entry;
    }

    if (writeToDiskStructures() == -1) {
	err_msg("Failed to delete file\n");
    }

    if (map_remove(directory_table, file) == -1) {
	printf("Error deleting file\n");
	return (-1);
    }

    return (0);
}

