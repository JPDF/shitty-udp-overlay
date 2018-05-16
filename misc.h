// MADE BY: Patrik, Jakob, Simon
#ifndef MISC_H
#define MISC_H

#include <stdint.h>
#include <stdio.h>

//colors
#define ANSI_WHITE   "\x1b[1;37m"
#define ANSI_RED     "\x1b[1;31m"
#define ANSI_GREEN   "\x1b[1;32m"
#define ANSI_YELLOW  "\x1b[1;33m"
#define ANSI_BLUE    "\x1b[1;34m"
#define ANSI_MAGENTA "\x1b[1;35m"
#define ANSI_CYAN    "\x1b[1;36m"
#define ANSI_RESET   "\x1b[0m"
#define clear() printf("\033[H\033[J")

struct client {
	int id;
	int windowsize;
};

void fatalerror(char *message);

void printBits(unsigned int bits);
void printBytes(const void *object, size_t size);

#endif
