CC      = gcc
CFLAGS  = -Wall -g
LDFLAGS = -ldl -pthread

ENGINES	= libc.o rele.o pcre.o re2.o tiny-rc.o subr.o
EXTRAS = engines/ex-tinyrc.o engines/ex-subr.o
EXTRA_LIBS = -lpcre2-8 -lre2 -labsl_string_view

# Targets
all: test memwrap.so

engines: $(ENGINES:%=engines/%)


#
# Librele special case
#
librele.o: ../librele/librele_c.c
	$(CC) $(CFLAGS) -O2 -c -o $@ $<

#
# Extra objects...
#
engines/ex-tinyrc.o: engines/tiny-regex-c/re.c
	$(CC) $(CFLAGS) -c -o $@ $<
engines/ex-subr.o: engines/subreg/subreg.c
	$(CC) $(CFLAGS) -c -o $@ $<

#
# Individual engines...
#
engines/%.o: engines/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

engines/%.o: engines/%.cpp
	g++ -c -o $@ $<

test: test.o stack.o test_cases.o $(ENGINES:%=engines/%) librele.o ${EXTRAS}
	g++ $(CFLAGS) -pthread $^ $(LDFLAGS) $(EXTRA_LIBS) -o $@ 

# Main test program
%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $<

# The memory wrapper shared library
memwrap.so: memwrap.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS)

#
# Turn the textual test cases into a testcases.c file so that we can then build it
# as part of the main binary.
#
test_cases.c: test_cases.txt
	./build_cases.py

# Clean up
clean:
	rm -f test memwrap.so test.o stack.o test_cases.o test_cases.c librele.o $(ENGINES:%=engines/%)