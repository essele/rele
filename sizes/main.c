#include <stdio.h>
#include <stdlib.h>

#ifdef ENGINE_LIBC
#include <regex.h>
#endif
#ifdef ENGINE_RELE
#include "../librele/rele.h"
#endif

#ifdef ENGINE_TRC
#include "../arm-linux-gnueabihf/tiny-regex-c/tiny-regex-c/re.h"
#endif

int main(int argc, char *argv[]) {
#ifdef ENGINE_LIBC
	regex_t		ctx;
	if (!regcomp(&ctx, "abc", 0))exit(1);
	if (!regexec(&ctx, "helloabc", 0, NULL, 0)) exit(1);
#endif

#ifdef ENGINE_RELE
	struct rectx	*ctx;
	ctx = rele_compile("abc", 0);
	if (!ctx) exit(1);
	if (!rele_match(ctx, "helloabc", 0, 0)) exit(1);
#endif

#ifdef ENGINE_TRC
	static re_t ctx;
	int			mlen;

	ctx = re_compile("abc");
	if (!ctx) exit(1);
	if (!re_matchp(ctx, "helloabc", &mlen)) exit(1);
#endif

	exit(0);

}
