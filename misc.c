// MADE BY: Patrik, Jakob, Simon
#include "misc.h"
#include <stdlib.h>

void fatalerror(char *message) {
	perror(message);
	exit(EXIT_FAILURE);
}


void printBits(unsigned int bits) {
	size_t size = sizeof(bits);
	unsigned int firstBit = 1 << (size * 8 - 1);
	int i, j;
	for (i = 0; i < size * 2; i++) {
		for (j = 0; j < 4; j++) {
			printf("%u", bits & firstBit ? 1 : 0);
			bits = bits << 1;
		}
		printf(" ");
	}
	printf("\n");
}

void printBytes(const void *object, size_t size) {
	const unsigned char *bytes = (const unsigned char *)object;
	for (int i = 0; i < size; i++) {
		printf("%02x ", bytes[i]);
	}
	printf("\n");
}
