#include <stdio.h>
#include <stdlib.h>
#include <string.h>



char *rele_strchr(char *str, int c) {
	while (*str) {
		if (*str == c) return str;
		str++;
	}
	return NULL;
}


char *find_string(char *p, char *str, int *len, char *ch) {
	char c;			// single return char
	int l = 0;		// len tracking
	int quoted = 0;	// are we in a quoted section

	while (*p) {
		// First deal with the quoted situation...
		if (quoted) {
			if (!*p) goto error;
			if (*p == '\\') {
				if (!p[1]) goto error;			// end after a backslash
				if (p[1] == 'E') { quoted = 0; p += 2; continue; }
			}
			goto normal_char;
		}

		// Do we need to end the current run...
		// Basic chars first...
		if (rele_strchr(".+?*|()[]{}^$", p[0])) break;

		// Then backslash variants...
		if (*p == '\\') {
			if (rele_strchr("dDwWsSbBRg{1234567890", p[1])) break;
			if (!p[1]) goto error;
		}

		// Ok, at this point we are fairly sure the character is valid, lets
		// figure out what it is...
		if (*p == '\\') {
			switch (p[1]) {
				case 'Q':		p += 2;			// point at first char of quote
								quoted = 1;
								continue;

				case 'x':		c = 'A'; break;		// TODO handle hex
				case 'n':		c = '\n'; break;
				case 't':		c = '\t'; break;
				case ',':		c = ','; break;
				default:	fprintf(stderr, "unknown backslash [%c]\n", p[1]); goto error;
			}
			p += 2;
		} else {
normal_char:
			c = *p++;
		}

		// Ok, so we have a candidate char, we need to make sure it's not followed
		// by something that would cause a problem (+?*), if so we need to roll it back.
		// The exception is if we are a single char, then we can find just that...
		if (rele_strchr("+?*", *p)) {
			if (l) { p--; break; }
			if (ch) *ch = c; 
			l = 1; break; 
		}

		// So here we think it's valid...
		if (ch) *ch = c;
		if (str) *str++ = c;
		l++;
	}
	if (len) *len = l;
	return p;

error:
	return NULL;
}

int main(int argc, char *argv[]) {
	char *input = argv[1];
	char *str = malloc(100);
	char ch;
	int len;

	char *p = find_string(input, str, &len, &ch);

	fprintf(stderr, "input=%p p=%p str=[%.*s] len=%d ch=[%c]\n", input, p, len, str, len, ch);



	exit(0);
}
