# Makefile for the SimpleFileSystem
# Compiler: GCC

all: sfsf sfsh

sfsf : sfs_api.c sfs_api.h disk_emu.h disk_emu.c
	gcc -o sfsf sfs_ftest.c sfs_api.c disk_emu.c -DHAVE_PTHREAD_RWLOCK=1 -lslack

sfsh : sfs_api.c sfs_api.h disk_emu.h disk_emu.c
	gcc -o sfsh sfs_htest.c sfs_api.c disk_emu.c -DHAVE_PTHREAD_RWLOCK=1 -lslack
