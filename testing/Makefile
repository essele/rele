CC      = gcc
CFLAGS  = -Wall -g -fPIC
LDFLAGS = -ldl -pthread

ENGINES	= libc.o rele.o pcre.o
EXTRA_LIBS = -lpcre2-8

# Targets
all: test memwrap.so

engines: $(ENGINES:%=engines/%)


#
# Individual engines...
#
engines/%.o: engines/%.c
	$(CC) $(CFLAGS) -c -o $@ $<


test: test.o stack.o test_cases.o $(ENGINES:%=engines/%) ../librele/rele.o
	$(CC) $(CFLAGS) -fno-stack-protector -pthread $^ $(LDFLAGS) $(EXTRA_LIBS) -o $@ 

# Main test program
%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $<

# The memory wrapper shared library
memwrap.so: memwrap.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

Xstack.o: stack.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Build the test cases object
Xtest_cases.o: test_cases.c
	$(CC) $(CFLAGS) -c -o $@ $<

test_cases.c: test_cases.txt
	./build_cases.py

# Clean up
clean:
	rm -f test memwrap.so test.o stack.o test_cases.o test_cases.c $(ENGINES:%=engines/%)