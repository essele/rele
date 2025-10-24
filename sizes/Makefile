CC = arm-linux-gnueabihf-gcc
CXX = arm-linux-gnueabihf-g++
CFLAGS = -O2 -static -ffunction-sections -fdata-sections -Wl,--gc-sections -flto

BUILDS = main_blank main_libc main_rele main_trc main_pcre2 main_slre main_subreg main_newlib main_re2 main_tre

all: $(BUILDS)

main_blank: main.c
	$(CC) $(CFLAGS) -o $@ main.c

main_libc:	main.c
	$(CC) -DENGINE_LIBC=1 $(CFLAGS) -o $@ $^

main_rele:	main.c ../rele/rele.c
	$(CC) -DENGINE_RELE=1 $(CFLAGS) -o $@ $^

main_trc: main.c ../arm-linux-gnueabihf/tiny-regex-c/tiny-regex-c/re.c
	$(CC) -DENGINE_TRC=1 $(CFLAGS) -o $@ $^

main_pcre2: main.c
	$(CC) -DENGINE_PCRE2 $(CFLAGS) -o $@ $^ -lpcre2-8

main_slre: main.c ../arm-linux-gnueabihf/slre/slre/slre.c
	$(CC) -DENGINE_SLRE=1 $(CFLAGS) -o $@ $^

main_subreg: main.c ../arm-linux-gnueabihf/subreg/subreg/subreg.c
	$(CC) -DENGINE_SUBREG=1 $(CFLAGS) -o $@ $^

main_newlib: main.c
	$(CC) -DENGINE_NEWLIB=1 $(CFLAGS) -I../arm-linux-gnueabihf/newlib -o $@ $^ -L../arm-linux-gnueabihf/newlib -lnewlib

main_re2: main.c
	$(CXX) -DENGINE_RE2=1 $(CFLAGS) -o $@ $^ \
				../arm-linux-gnueabihf/re2/shim.o \
				-L../arm-linux-gnueabihf/re2/re2 -lre2 \
				-L../arm-linux-gnueabihf/re2/abseil-cpp/build/absl -labsl

main_tre: main.c
	$(CC) -DENGINE_TRE=1 $(CFLAGS) -o $@ $^ -L../arm-linux-gnueabihf/tre/tre/lib/.libs -ltre

clean:
	rm -f $(BUILDS)