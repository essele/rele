CXX = arm-none-eabi-g++
AR = arm-none-eabi-ar

CXXFLAGS = -std=c++17 -O2 -fno-exceptions -fno-rtti \
           -Wall \
           
           -DABSL_OPTION_USE_STD_OPTIONAL=1 \
           -DABSL_USES_STD_STRING_VIEW=1 \
           -DABSL_USES_STD_VARIANT=1 \
           -DABSL_USES_STD_ANY=1


CXXFLAGS += -I../abseil-cpp -I.

OBJS = re2/regexp.o re2/dfa.o re2/re2.o re2/onepass.o \
       re2/parse.o re2/prog.o re2/simplify.o \
       re2/tostring.o re2/unicode_casefold.o re2/unicode_groups.o

libre2.a: $(OBJS)
	$(AR) rcs $@ $^

clean:
	rm -f $(OBJS) libre2.a
