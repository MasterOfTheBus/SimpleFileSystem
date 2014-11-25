# Makefile for the SimpleFileSystem
# Compiler: GCC

all: sfsf sfsh sfst

sfst : sfs_test.c sfs_api.h disk_emu.h sfs_api.c disk_emu.c
	gcc -g -o sfst sfs_test.c sfs_api.c disk_emu.c -DHAVE_PTHREAD_RWLOCK=1 -lslack

sfsf : sfs_api.c sfs_api.h disk_emu.h disk_emu.c
	gcc -g -o sfsf sfs_ftest.c sfs_api.c disk_emu.c -DHAVE_PTHREAD_RWLOCK=1 -lslack

sfsh : sfs_api.c sfs_api.h disk_emu.h disk_emu.c
	gcc -g -o sfsh sfs_htest.c sfs_api.c disk_emu.c -DHAVE_PTHREAD_RWLOCK=1 -lslack
