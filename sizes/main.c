#include <stdio.h>
#include <stdlib.h>

#ifdef ENGINE_LIBC
#include <regex.h>
#endif
#ifdef ENGINE_RELE
#include "../librele/rele.h"
#endif
#ifdef ENGINE_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include "../arm-linux-gnueabihf/pcre/pcre2/src/pcre2.h"
#endif

#ifdef ENGINE_TRC
#include "../arm-linux-gnueabihf/tiny-regex-c/tiny-regex-c/re.h"
#endif

#ifdef ENGINE_SLRE
#include "../arm-linux-gnueabihf/slre/slre/slre.h"
#endif

#ifdef ENGINE_SUBREG
#include "../arm-linux-gnueabihf/subreg/subreg/subreg.h"
#endif

#ifdef ENGINE_NEWLIB
#include "../arm-linux-gnueabihf/newlib/regex.h"
#endif

#ifdef ENGINE_RE2
#include "../arm-linux-gnueabihf/shim.h"
#endif

#ifdef ENGINE_TRE
#define USE_LOCAL_TRE_H
#include "../arm-linux-gnueabihf/tre/tre/local_includes/regex.h"
#endif

int main(int argc, char *argv[]) {
#ifdef ENGINE_LIBC
	regex_t		ctx;
	if (!regcomp(&ctx, "abc", 0))exit(1);
	if (!regexec(&ctx, "helloabc", 0, NULL, 0)) exit(1);
#endif

#ifdef ENGINE_RELE
	struct rectx	*ctx;
	ctx = rele_compile("abc", 0, NULL);
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

#ifdef ENGINE_PCRE2
	static pcre2_code *pcre_code;
	static pcre2_match_data *match_data;
	static int errornumber;
	static PCRE2_SIZE erroroffset;

    pcre_code = pcre2_compile((PCRE2_SPTR8)"abc", PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL); 
    if (!pcre_code) exit(1);

    match_data = pcre2_match_data_create_from_pattern(pcre_code, NULL);
	if (!match_data) exit(1);
	pcre2_match(pcre_code, (PCRE2_SPTR8)"helloabc", 8, 0, 0, match_data, NULL);

#endif

#ifdef ENGINE_SLRE
	#define SLRE_MAX_CAPTURES	10
	struct slre_cap	captures[SLRE_MAX_CAPTURES];
	int offset;
	offset = slre_match("abc", "helloabc", 8, captures, SLRE_MAX_CAPTURES, 0);
	if (offset < 0) exit(1);	
#endif

#ifdef ENGINE_SUBREG
	#define SUBR_MAX_GROUPS     10
	static subreg_capture_t      captures[SUBR_MAX_GROUPS];
	static int 	capture_count;

	capture_count = subreg_match("abc", "helloabc", captures, SUBR_MAX_GROUPS, 128);
	if (capture_count < 0) exit (1);
#endif

#ifdef ENGINE_NEWLIB
	#define LIBC_MAX_GROUPS     10
	static regex_t         newlib_regex;
	static regmatch_t      pmatch[LIBC_MAX_GROUPS];

    if (regcomp_nl(&newlib_regex, "abc", 0)) exit(1);
    int res = regexec_nl(&newlib_regex, "helloabc", LIBC_MAX_GROUPS, pmatch, 0);
    if (res) exit(1);
#endif

#ifdef ENGINE_RE2
	extern struct engine re2_engine;

	re2_engine.compile((char *)"abc", 0);
	re2_engine.match((char *)"helloabc", 0);
#endif

#ifdef ENGINE_TRE
	#define LIBC_MAX_GROUPS     10
	static regex_t         tre_regex;
	static regmatch_t      pmatch[LIBC_MAX_GROUPS];

    if (tre_regcomp(&tre_regex, "abc", 0)) exit (1);
    int res = tre_regexec(&tre_regex, "helloabc", LIBC_MAX_GROUPS, pmatch, 0);
    if (res) exit(1);
#endif

	exit(0);

}
