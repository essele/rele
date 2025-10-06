CC = arm-none-eabi-gcc
AR = arm-none-eabi-ar

CFLAGS = -Wall -O0 -g -mcpu=cortex-a8 -nostdlib -ffreestanding
WRAPS = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc
LDFLAGS = -mcpu=cortex-a8 -T linker.ld $(WRAPS) -specs=rdimon.specs -lc -lgcc

# Object for building the main function....
OBJS = main.o memwrap.o 

# Shims we need for each of the engines
SHIMS = pcre/shim.o newlib/shim.o

# Engines that need to be built
ENGINES = pcre newlib

# Libraries that we need to link with (for all the engines)
EXTRA_LIBS = -Lpcre/pcre2/.libs -lpcre2-8 -Lnewlib -lnewlib


all: main.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main.elf: $(OBJS) $(SHIMS) $(ENGINES)
	$(CC) $(OBJS) $(SHIMS) $(LDFLAGS) -o $@ $(EXTRA_LIBS)
	
# -L./pcre/pcre2/.libs -lpcre2-8

run: main.elf
	qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -semihosting -kernel main.elf -monitor null

debug: main.elf
	qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -semihosting -kernel main.elf -monitor null -S -gdb tcp::1234

clean:
	rm -f *.o *.elf


# All the modules we need....

##################################################################################################
# NEWLIB -- for some reason the built newlib doesn't contain the regex, so we can do this
#.          separately just by grabing the right bits of code.
##################################################################################################

NEWLIB_OBJS = regcomp.o regexec.o collate.o collcmp.o regfree.o

newlib: $(NEWLIB_OBJS:%=newlib/%)
	$(AR) rcs newlib/libnewlib.a $^


##################################################################################################
# PCRE -- we need to grab it, patch it, do some autogen/configure work and then build
#         the library only.
#
#.        Then we can worry about our shim...
#
##################################################################################################

pcre: pcre_lib

pcre_shim:	pcre/shim.o

pcre_lib: pcre/pcre2/autogen.sh  pcre/pcre2/src/RELE_PATCHED pcre/pcre2/configure \
				pcre/pcre2/Makefile pcre/pcre2/.libs/libpcre2-8.a

#				pcre/pcre2/src/RELE_PATCHED

pcre/pcre2/autogen.sh:
	(cd pcre && git clone --depth=1 https://github.com/PCRE2Project/pcre2.git)

pcre/pcre2/src/RELE_PATCHED:
	(cd pcre/pcre2 && patch -p1 < ../allow_cross_compile.patch)

pcre/pcre2/configure:
	(cd pcre/pcre2 && ./autogen.sh)

pcre/pcre2/Makefile:
	(cd pcre/pcre2 && \
		export CC=arm-none-eabi-gcc && \
		export AR=arm-none-eabi-ar && \
		export RANLIB=arm-none-eabi-ranlib && \
		export CFLAGS="-O1 -g -ffreestanding -mcpu=cortex-a8" && \
		export LDFLAGS="-ffreestanding -nostdlib -lc -lgcc" && \
		\
	    ./configure --host=arm-none-eabi \
            --prefix=/not/relevant \
            --disable-unicode \
            --disable-pcre2-16 \
            --disable-pcre2-32 \
            --disable-shared \
            --enable-static \
            --disable-jit)

pcre/pcre2/.libs/libpcre2-8.a:
	(cd pcre/pcre2 && make libpcre2-8.la)