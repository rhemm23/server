#include <stdio.h>

#include "util.h"

void die(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

void *smalloc(size_t size) {
	void *res;
	if((res = malloc(size)) == NULL) {
		die("malloc");
	}
	return res;
}

void *scalloc(size_t num, size_t size) {
	void *res;
	if((res = calloc(num, size)) == NULL) {
		die("calloc");
	}
	return res;
}
