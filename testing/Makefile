CC      = gcc
CFLAGS  = -Wall -g -fPIC
LDFLAGS = -ldl -pthread

# Targets
all: test memwrap.so

# Your program
test: test.c
	$(CC) $(CFLAGS) -o $@ $<

# The memory wrapper shared library
memwrap.so: memwrap.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

# Run your program with memory tracking enabled
run: test memwrap.so
	LD_PRELOAD=./memwrap.so ./test

# Clean up
clean:
	rm -f test memwrap.so