#include "slidingwindow.h"

int isInsideWindow(int seq, int first, int last) {
	return (first <= last && seq >= first && seq <= last) ||
						(first > last && seq <= first && seq >= last);
}
