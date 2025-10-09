CC = arm-linux-gnueabihf-gcc
AR = arm-linux-gnueabihf-ar
CXX = arm-linux-gnueabihf-g++
RANLIB = arm-linux-gnueabihf-ranlib

CFLAGS = -Wall -O0
WRAPS = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc
LDFLAGS =

# Object for building the main function....
OBJS = main.o memwrap.o test_cases.o

# Shims we need for each of the engines
SHIMS = pcre/shim.o libc/shim.o newlib/shim.o tre/shim.o slre/shim.o \
					re2/shim.o \
					tiny-regex-c/shim.o subreg/shim.o rele/shim.o

# Engines that need to be built
ENGINES = pcre newlib tre slre tiny-regex-c subreg rele re2

# Libraries that we need to link with (for all the engines)
EXTRA_LIBS = -Lpcre/pcre2/.libs -lpcre2-8 \
				-Lnewlib -lnewlib \
				-Ltre/tre/lib/.libs -ltre \
				slre/slre/slre.o \
				tiny-regex-c/tiny-regex-c/re.o \
				subreg/subreg/subreg.o \
				-Lre2/re2 -lre2 -Lre2/abseil-cpp/build/absl -labsl \
				rele/rele.o



all: main.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#%.o: %.cpp
#	$(CXX) -c -o $@ $<

#
# Turn the textual test cases into a testcases.c file so that we can then build it
# as part of the main binary.
#
test_cases.c: cases/*
	./build_cases.py -o test_cases.c cases/*

main.elf: $(ENGINES) $(OBJS) $(SHIMS)
	$(CXX) $(OBJS) $(SHIMS) $(LDFLAGS) $(WRAPS) -o $@ $(EXTRA_LIBS)
	
# -L./pcre/pcre2/.libs -lpcre2-8

run: main.elf
	qemu-arm main.elf
#	qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -semihosting -kernel main.elf -monitor null
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

NEWLIB_OBJS = regcomp.o regexec.o collate.o collcmp.o regfree.o my_reallocf.o

newlib: $(NEWLIB_OBJS:%=newlib/%)
	$(AR) rcs newlib/libnewlib.a $^

newlib/%.o: newlib/%.c
	$(CC) -Inewlib $(CFLAGS) -c $< -o $@



##################################################################################################
# PCRE -- we need to grab it, patch it, do some autogen/configure work and then build
#         the library only.
#
#.        Then we can worry about our shim...
#
##################################################################################################

pcre: pcre_lib

pcre_shim:	pcre/shim.o

pcre_lib: pcre/pcre2/autogen.sh pcre/pcre2/configure \
				pcre/pcre2/Makefile pcre/pcre2/.libs/libpcre2-8.a

pcre/pcre2/autogen.sh:
	(cd pcre && git clone --depth=1 https://github.com/PCRE2Project/pcre2.git)

pcre/pcre2/configure:
	(cd pcre/pcre2 && ./autogen.sh)

pcre/pcre2/Makefile:
	(cd pcre/pcre2 && \
		export CC=$(CC) && \
		export AR=$(AR) && \
		export RANLIB=$(RANLIB) && \
		export CFLAGS="-O2" && \
		export LDFLAGS="" && \
		\
	    ./configure --host=arm-lionux-gnueabihf \
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

tre_lib: tre/tre/configure.ac tre/tre/configure tre/tre/Makefile \
							tre/tre/lib/.libs/libtre.a

tre/tre/configure.ac:
	(cd tre && git clone --depth=1 https://github.com/laurikari/tre.git)

tre/tre/configure: tre/tre/configure.ac
	(cd tre/tre && ./utils/autogen.sh)

tre/tre/Makefile: tre/tre/configure
	(cd tre/tre && export CC=$(CC) && \
		export AR=$(AR) && \
		export RANLIB=$(RANLIB) && \
		export CFLAGS="-O2" && \
		export LDFLAGS="" && \
		./configure --host=arm-linux-gnueabihf \
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

re2: re2_abseil re2_lib

re2_shim:	re2/shim.o

re2/shim.o: re2/shim.cpp
	$(CXX) -Ire2/abseil-cpp -Ire2/re2 -c -o $@ $<

re2/absl_stub.o: re2/absl_stub.cpp
	$(CXX) -Ire2/abseil-cpp -Ire2/re2 -c -o $@ $<

re2_lib: re2/re2/Makefile re2/re2/re2/RE2_PATCHED re2/re2/libre2.a 

re2/re2/Makefile:
	(cd re2 && git clone https://github.com/google/re2.git)

re2/re2/re2/RE2_PATCHED:
	(cd re2/re2 && patch -p1 < ../allow_cross_compile.patch)

re2_abseil:  re2/abseil-cpp re2/abseil-cpp/build/absl/libabsl.a

re2/abseil-cpp:
	(cd re2 && git clone https://github.com/abseil/abseil-cpp.git)

re2/abseil-cpp/build/absl/libabsl.a:
	(cd re2/abseil-cpp && mkdir -p build && cd build && \
		cmake -DCMAKE_TOOLCHAIN_FILE=../../abseil-toolchain.cmake .. && \
		make && \
		cd absl && \
		$(AR) rcs libabsl.a `find . -name "*.o"`)

RE2_CXXFLAGS = -std=c++17 -O2 -Wall \
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
