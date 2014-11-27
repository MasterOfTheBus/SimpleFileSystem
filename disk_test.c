#include <stdio.h>
#include <stdlib.h>

#include "disk_emu.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
	printf("arg 1 or 0\n");
	return (-1);
    }

    if (atoi(argv[1]) == 1) {

	if (init_fresh_disk("DiskTest", 512, 1) == -1) {
	    printf("Couldn't init fresh");
	    return(-1);
	}

	char* names[] = {"Stanley", "Fred", "Jane", "Alice", "bloop"};
	if (write_blocks(0, 1, names) == -1) {
	    printf("couldn't write\n");
	    return (-1);
	}
    } else if (atoi(argv[1]) == 0) {
	if (init_disk("DiskTest", 512, 1) == -1) {
	    printf("Coudln't init old\n");
	    return (-1);
	}
    } else if (atoi(argv[1]) == 2) {
	printf("shh secret debug\n");
	#if 0
	char* read[512];
	if (read_blocks(
	#endif
    } else {
	printf("must be 0 or 1\n");
	return (-1);
    }

    char* read[512];
    if (read_blocks(0, 1, read) == -1) {
	printf("Couldn't read\n");
	return (-1);
    }

    int i = 0;
    for (; i < 5; i++) {
	printf("read[%d]: %s\n", i, read[i]);
    }

    close_disk();
}
