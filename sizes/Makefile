CFLAGS = -O2 -static -ffunction-sections -fdata-sections -Wl,--gc-sections -flto

BUILDS = main_blank main_libc main_rele main_trc

all: $(BUILDS)

main_blank: main.c
	gcc $(CFLAGS) -o $@ main.c

main_libc:	main.c
	gcc -DENGINE_LIBC=1 $(CFLAGS) -o $@ $^

main_rele:	main.c ../librele/librele.c
	gcc -DENGINE_RELE=1 $(CFLAGS) -o $@ $^

main_trc: main.c ../arm-linux-gnueabihf/tiny-regex-c/tiny-regex-c/re.c
	gcc -DENGINE_TRC=1 $(CFLAGS) -o $@ $^

clean:
	rm -f $(BUILDS)