#ifndef MISC_H
#define MISC_H

#include <stdio.h>

struct client {
	int id;
	int windowsize;
};

void fatalerror(char *message);

#endif
