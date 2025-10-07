CXX = arm-linux-gnueabihf-g++
AR = arm-linux-gnueabihf-ar

CXXFLAGS = -std=c++17 -O2 -Wall \
           -DABSL_OPTION_USE_STD_OPTIONAL=1 \
           -DABSL_USES_STD_STRING_VIEW=1 \
           -DABSL_USES_STD_VARIANT=1 \
           -DABSL_USES_STD_ANY=1


CXXFLAGS += -I../abseil-cpp -I.

OBJS = re2/regexp.o re2/dfa.o re2/re2.o re2/onepass.o \
       re2/parse.o re2/prog.o re2/simplify.o \
       re2/tostring.o re2/unicode_casefold.o re2/unicode_groups.o \
       util/rune.o re2/bitstate.o re2/bitmap256.o re2/compile.o \
       re2/filtered_re2.o re2/nfa.o re2/prefilter.o \
       re2/prefilter_tree.o re2/set.o re2/perl_groups.o \
       util/strutil.o


libre2.a: $(OBJS)
	$(AR) rcs $@ $^

clean:
	rm -f $(OBJS) libre2.a
