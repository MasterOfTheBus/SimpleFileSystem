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
#define MAX_FILES 16 // temp values
#define MAX_FILE_SIZE 1024 // temp values
#define MAX_NAME_LENGTH 32 

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

int calcBlockNum(int ptr) {
    printf("%d / %d = %d\n", ptr, BLOCKSIZE, ptr / BLOCKSIZE);
    return (ptr / BLOCKSIZE);
}

int getFatEntryAt(int offset, FatEntry* fat_entry) {
    if (!fat_entry) {
	return (-1);
    }
    int count = 0;
    int index = fat_entry->next_entry;
    printf("offset: %d\n", offset);
    while((index != -1) && (count < offset)) {
	printf("index: %d, count: %d\n", index, count);
	count++;
	fat_entry = map_get(file_allocation_table, (void*)index);
	index = fat_entry->next_entry;
    }
    if (count < offset) {
	// case where the file pointer is beyond range of the file
	return (count);
    }
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

    int names_size = (size >= MAX_FILES) ? MAX_FILES : size+1;

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
    
    if (size < MAX_FILES) {
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

    char names[MAX_FILES][MAX_NAME_LENGTH];
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
    unsigned int size = (map_size(map) == BLOCKSIZE) ? BLOCKSIZE : map_size(map) + 1;

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
    if (map_size(map) != BLOCKSIZE) {
	index[i] = -1;
	data[i] = 0;
	next[i] = 0;
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
	if ((data[i] == 0) || (next[i] == 0) || (index[i] == -1)) {
	    break;
	}
	FatEntry* fe = malloc(sizeof(FatEntry));
	fe->data_block = data[i];
	fe->next_entry = next[i];
	map_add(map, (void*)index[i], fe);
	printf("reading fat index: %d\n", index[i]);
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
	if (init_fresh_disk(DISK_FILE, BLOCKSIZE, NUM_BLOCKS) == -1) {
	    printf("Couldn't init fresh disk\n");
	}

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
	    fen->block = block;
	    fen->next = block+1;
	    if (i == free_blocks-1) {
		fen->next = -1;
	    }
	    if (!(list_append(free_block_list, fen))) {
		err_msg("map add");
	    }
	    list_size++;
	    block++;
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
printf("fat_index: %d\n", fat_index);
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
    int written_bytes = 0;
    FdEntry* fd = map_get(file_descriptor_table, (void*)fileID);
    if (!fd) {
	printf("Invalid file descriptor\n");
	return (-1);
    }
    printf("fileID: %d, file fat root: %d\n", fileID, fd->file_fat_root);
    FatEntry* fat_entry = map_get(file_allocation_table, (void*)fd->file_fat_root);
    if (!fat_entry) {
	printf("Couldn't read from disk\n");
	return (-1);
    }
    printf("fat block: %d, fat next: %d\n", fat_entry->data_block, fat_entry->next_entry);
    if (getFatEntryAt(calcBlockNum(fd->write_ptr), fat_entry) == -1) {
	printf("pointer outside of file range\n");
	return (-1);
    }

    List *buf_queue = list_create(NULL);
    if (!buf_queue) {
	printf("error\n");
	return (-1);
    }
    list_append(buf_queue, buf);
    List *block_queue = list_create(NULL);
    if (!block_queue) {
	printf("error\n");
	return (-1);
    }
    list_append(block_queue, fat_entry);

    int write_ptr = fd->write_ptr;

    // save the leftoever data in the block first
    char after_data[BLOCKSIZE];
    int after_data_next_block = fat_entry->next_entry;
    // read the block data
    char readfirst[BLOCKSIZE];
    if (read_blocks(fat_entry->data_block, 1, readfirst) == -1) {
	printf("error reading blocks starting at %d\n", fat_entry->data_block);
	return (-1);
    }
    strcpy(after_data, readfirst + (write_ptr % BLOCKSIZE));

    // write the buf data
    do {
	if(list_empty(block_queue)) {
	    printf("error, list empty\n");
	    return (-1);
	}
	FatEntry* f_entry = list_shift(block_queue);
	int block = f_entry->data_block;

	char* buf_to_write = list_shift(buf_queue);
	int ptr_relative_pos = write_ptr % BLOCKSIZE;
	int buf_length = strcspn(buf_to_write, "\0");

	// read the block data
	char read[BLOCKSIZE];
	if (read_blocks(block, 1, read) == -1) {
	    printf("error reading block\n");
	    return (-1);
	}
	int data_length = strcspn(read, "\0");

	// compare the data to write to the available space
	if (buf_length <= BLOCKSIZE - data_length) {
	    strcpy(read + ptr_relative_pos, buf_to_write);
	    write_ptr += buf_length;
	} else if (buf_length > BLOCKSIZE - data_length) {
	    // write what can be written
	    int partial_write = BLOCKSIZE - ptr_relative_pos;
	    strncpy(read + ptr_relative_pos, buf_to_write, partial_write);
	    ptr_relative_pos += partial_write;

	    // split up the buffer and save what still needs to be written
	    int remaining_length = buf_length - partial_write;
	    strcpy(buf_to_write, buf_to_write + remaining_length);

	    // allocate a block for the remaining data in the buffer
	    if (list_empty(free_block_list)) {
		printf("Disk Full\n");
		return (-1);
	    }
	    FreeEntry* free_entry = list_shift(free_block_list);
	    if (!free_entry) {
		printf("error block list\n");
		return (-1);
	    }
	    FatEntry* new_fat = malloc(sizeof(FatEntry));
	    new_fat->data_block = free_entry->block;
	    new_fat->next_entry = -1;
	    list_append(block_queue, new_fat);
	    write_ptr += data_length;
	}
	if (write_blocks(block, 1, read) == -1) {
	    printf("failed to write\n");
	    return (-1);
	}
    } while (!list_empty(buf_queue));

    if (strcspn(after_data, "\0") > 0) {
	list_append(buf_queue, after_data);
    }
    if (after_data_next_block == -1) {
	return (written_bytes);
    }
    fat_entry = map_get(file_allocation_table, (void*)after_data_next_block);
    if (!fat_entry) {
	printf("failed allocation\n");
	return (-1);
    }
    list_append(block_queue, fat_entry);
    int write_bytes = 0;
    while (!list_empty(buf_queue)) {
	char* write = list_shift(buf_queue);
	int buf_length = strcspn(write, "\0");
	FatEntry* f = list_shift(block_queue);
	if (!f) {
	    printf("failed allocation\n");
	    return (-1);
	}
	char read[BLOCKSIZE];
	char* leftover;
	if (read_blocks(f->data_block, 1, read) == -1) {
	    printf("failed read\n");
	    return (-1);
	}
	int read_length = strcspn(read, "\0");
	strcpy(leftover, read + (BLOCKSIZE - buf_length));
	if (strcspn(leftover, "\0") > 0) {
	    list_append(buf_queue, leftover);
	}
	int rel_ptr = write_bytes % BLOCKSIZE;
	if (buf_length > BLOCKSIZE) {
	    strncpy(read + rel_ptr, write, BLOCKSIZE - (rel_ptr));
	} else {
	    strcpy(read + rel_ptr, write);
	}
	if (write_blocks(f->data_block, 1, write) == -1) {
	    printf("failed to write\n");
	    return (-1);
	}
    }

    // write the "after" data
    return (written_bytes);
}

/*
 * return the number of bytes read
 *
 * return can be less than length in the event that length from read pointer
 * exceed file length
 */
int sfs_fread(int fileID, char *buf, int length) {
    char for_reading[length];
    int read_bytes = 0;
    FdEntry* fd = map_get(file_descriptor_table, (void*)fileID);
    if (!fd) {
	printf("Invalid file descriptor\n");
	return (-1);
    }
    FatEntry* fat_entry = map_get(file_allocation_table, (void*)fd->file_fat_root);
    if (!fat_entry) {
	printf("Couldn't read from disk\n");
	return (-1);
    }
    int read_ptr = fd->read_ptr;
    int block_offset = calcBlockNum(read_ptr);
    if (getFatEntryAt(block_offset, fat_entry) == -1) {
	printf("pointer outside of file range\n");
	return (-1);
    }

    int next = fat_entry->next_entry;
    do {
	int block = fat_entry->data_block;

	// first read what's in the current block
	char read[BLOCKSIZE];
	if (read_blocks(block, 1, read) == -1) {
	    printf("Error reading from disk\n");
	    return (-1);
	}

	// get what's remaining in the first block
	int data_end = read_ptr % BLOCKSIZE;
	//int data = BLOCKSIZE - data_end;
	int data = strcspn(read, "\0");
	strncat(for_reading, read + data_end, data);
	read_bytes += data;
	read_ptr += data;

	if (next == -1) {
	    continue;
	}
	fat_entry = map_get(file_allocation_table, (void*)next);
	if (!fat_entry) {
	    printf("Error reading\n");
	    return (-1);
	}
	next = fat_entry->next_entry;
    } while (next != -1 && read_bytes < length);
    fd->read_ptr = read_ptr;
    buf = for_reading;
    return (read_bytes);
}

int sfs_fseek(int fileID, int offset) {
    FdEntry* fd = map_get(file_descriptor_table, (void*)fileID);
    if (!fd) {
	printf("Invalid file descriptor\n");
	return (-1);
    }

    if (fd->write_ptr + offset < 0 || fd->read_ptr + offset < 0) {
	printf("seeking to before beginning of file\n");
	return (-1);
    }
    
    FatEntry* fat_entry = map_get(file_allocation_table, (void*)fd->file_fat_root);
    if (!fat_entry) {
	printf("Couldn't read from disk\n");
	return (-1);
    }
    int next = fat_entry->next_entry; 
    int length = 0; 
    do {
	int block = fat_entry->data_block;

	// first read what's in the current block
	char read[BLOCKSIZE];
	if (read_blocks(block, 1, read) == -1) {
	    printf("Error reading from disk\n");
	    return (-1);
	}
	length += strcspn(read, "\0");

	fat_entry = map_get(file_allocation_table, (void*)next);
	if (!fat_entry) {
	    printf("Error reading\n");
	    return (-1);
	}
	next = fat_entry->next_entry;
    } while (next != -1);
   
    if (fd->write_ptr + offset > length || fd->read_ptr + offset > length) {
	printf("Seeking to after end of file\n");
	return (-1);
    }
    fd->write_ptr = fd->write_ptr + offset;
    fd->read_ptr = fd->read_ptr + offset;

    return (0);
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

