CC = arm-none-eabi-gcc
AR = arm-none-eabi-ar
CXX = arm-none-eabi-g++

CFLAGS = -Wall -O0 -g -mcpu=cortex-a8 -nostdlib -ffreestanding
WRAPS = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc
LDFLAGS = -mcpu=cortex-a8 -T linker.ld $(WRAPS) -specs=rdimon.specs -lc -lgcc

# Object for building the main function....
OBJS = main.o memwrap.o 

# Shims we need for each of the engines
SHIMS = pcre/shim.o newlib/shim.o tre/shim.o slre/shim.o tiny-regex-c/shim.o subreg/shim.o rele/shim.o

# Engines that need to be built
ENGINES = pcre newlib tre slre tiny-regex-c subreg rele

# Libraries that we need to link with (for all the engines)
EXTRA_LIBS = -Lpcre/pcre2/.libs -lpcre2-8 \
				-Ltre/tre/lib/.libs -ltre \
				-Lnewlib -lnewlib \
				slre/slre/slre.o \
				tiny-regex-c/tiny-regex-c/re.o \
				subreg/subreg/subreg.o \
				rele/rele.o


all: main.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#%.o: %.cpp
#	$(CXX) -c -o $@ $<

main.elf: $(ENGINES) $(OBJS) $(SHIMS)
	$(CXX) $(OBJS) $(SHIMS) $(LDFLAGS) -o $@ $(EXTRA_LIBS)
	
# -L./pcre/pcre2/.libs -lpcre2-8

run: main.elf
	qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -semihosting -kernel main.elf -monitor null
#	qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -semihosting -kernel main.elf -monitor null

debug: main.elf
	qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -semihosting -kernel main.elf -monitor null -S -gdb tcp::1234

clean:
	rm -f *.o *.elf $(SHIMS)


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

##################################################################################################
# TRE -- we need to grab it, do some autogen/configure work and then build
#        the library only.
##################################################################################################

tre:	tre_lib

tre_shim:	tre/shim.o

tre_lib: tre/tre/configure.ac tre/tre/configure tre/tre/Makefile

tre/tre/configure.ac:
	(cd tre && git clone --depth=1 https://github.com/laurikari/tre.git)

tre/tre/configure: tre/tre/configure.ac
	(cd tre/tre && ./utils/autogen.sh)

tre/tre/Makefile: tre/tre/configure
	(cd tre/tre && export CC=arm-none-eabi-gcc && \
		export AR=arm-none-eabi-ar && \
		export RANLIB=arm-none-eabi-ranlib && \
		export CFLAGS="-O1 -g -ffreestanding -mcpu=cortex-a8" && \
		export LDFLAGS="-ffreestanding -nostdlib -lc -lgcc" && \
		./configure --host=arm-none-eabi \
			--disable-approx \
			--enable-static \
			--disable-shared \
			--without-alloca \
            --prefix=/not/relevant)

tre/tre/lib/.libs/libtre.a:	tre/tre/Makefile
	(cd tre/tre && make)

##################################################################################################
# RE2 -- we need to grab it, do some autogen/configure work and then build
#        the library only.
#
# NOTE -- this does not work!
#
##################################################################################################

re2: re2_lib

re2_shim:	re2/shim.o

re2/shim.o: re2/shim.cpp
	$(CXX) -Ire2/abseil-cpp -Ire2/re2 -c -o $@ $<

re2/absl_stub.o: re2/absl_stub.cpp
	$(CXX) -Ire2/abseil-cpp -Ire2/re2 -c -o $@ $<

re2_lib: re2/re2/Makefile re2/re2/re2/RE2_PATCHED re2/abseil-cpp re2/re2/libre2.a 

re2/re2/Makefile:
	(cd re2 && git clone https://github.com/google/re2.git)

re2/re2/re2/RE2_PATCHED:
	(cd re2/re2 && patch -p1 < ../allow_cross_compile.patch)

re2/abseil-cpp:
	(cd re2 && git clone https://github.com/abseil/abseil-cpp.git)

RE2_CXXFLAGS = -std=c++17 -O2 -Wall -ffreestanding -fno-exceptions -fno-rtti \
           -DABSL_OPTION_USE_STD_OPTIONAL=1 \
           -DABSL_USES_STD_STRING_VIEW=1 \
           -DABSL_USES_STD_VARIANT=1 \
           -DABSL_USES_STD_ANY=1

re2/re2/libre2.a:
	(cd re2/re2 && make -f ../minimal.mk)

##################################################################################################
# SLRE -- just grab the source and build the object file
##################################################################################################

slre: slre/slre/slre.o

slre_shim:	slre/shim.o

slre/slre/slre.c:
	(cd slre && git clone https://github.com/cesanta/slre.git)

slre/slre/slre.o: slre/slre/slre.c

##################################################################################################
# TINY-REGEX-C -- just grab the source and build the object file
##################################################################################################

tiny-regex-c:	tiny-regex-c/tiny-regex-c/re.o

tiny-regex-c_shim:	tiny-regex-c/shim.o

tiny-regex-c/tiny-regex-c/re.c:
	(cd tiny-regex-c && git clone https://github.com/kokke/tiny-regex-c.git)

tiny-regex-c/tiny-regex-c/regex.o: tiny-regex-c/tiny-regex-c/re.c

##################################################################################################
# SUBREG -- just grab the source and build the object file
##################################################################################################

subreg: subreg/subreg/subreg.o

subreg_shim:	subreg/shim.o

subreg/subreg/subreg.c:
	(cd subreg && git clone https://github.com/mattbucknall/subreg.git)

subreg/subreg/subreg.o: subreg/subreg/subreg.c

##################################################################################################
# RELE
##################################################################################################

rele:	rele/rele.o

rele_shim:	rele/shim.o
