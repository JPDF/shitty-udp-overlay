#include "error.h"
#include "color.h"
#include <stdlib.h>

void fatal_error(char *message) {
	printf(ANSI_RED);
	perror(message);
	printf(ANSI_RESET);
	exit(EXIT_FAILURE);
}
