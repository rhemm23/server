#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

void die(char *msg);

void *smalloc(size_t size);
void *scalloc(size_t num, size_t size);

#endif
