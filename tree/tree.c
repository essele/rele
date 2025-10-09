#include <stdio.h>
#include <stdlib.h>
#include "../librele/rele.h"

//
// dot -Tpng tree.dot -o tree.png
//


int main(int argc, char *argv[]) {

	struct rectx *ctx;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <dot_file_name> <regex>\n", argv[0]);
		exit(1);
	}
	ctx = rele_compile(argv[2], 0);
	if (!ctx) {
		fprintf(stderr, "Compilation failed.\n");
		exit(0);
	}
	rele_export_tree(ctx, argv[1]);
	printf("done.\n");
	exit(0);
}
