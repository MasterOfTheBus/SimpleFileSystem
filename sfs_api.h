#include <slack/std.h>
#include <slack/list.h>
#include <slack/map.h>

typedef struct directory_entry DirEntry;
typedef struct file_descriptor_entry FdEntry;
typedef struct file_allocation_entry FatEntry;

Map *directory_table;
Map *file_descriptor_table;
Map *file_allocation_table;
List *free_block_list;

int mksfs(int fresh);
void sfs_ls(void);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fwrite(int fileID, char *buf, int length);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fseek(int fileID, int offset);
int sfs_remove(char *file);



