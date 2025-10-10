#include <stdio.h>
#include <regex.h>
#include <stdlib.h>

regex_t ctx;


size_t total = 0;


extern void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
	total += size;
	return __real_malloc(size);
}

int main(int argc, char *argv[]) {
	int res = 4;
//	int res = regcomp(&ctx, "abc", 0);
	int fred = (int)total;
	fprintf(stderr, "total = %d\n", fred);
	exit(res);
	
}
