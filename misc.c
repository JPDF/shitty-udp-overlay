#include "misc.h"
#include <stdlib.h>

void fatalerror(char *message) {
	perror(message);
	exit(EXIT_FAILURE);
}
