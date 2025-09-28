CC      = gcc
CFLAGS  = -Wall -g -fPIC
LDFLAGS = -ldl -pthread

# Targets
all: test


test: test.o test_cases.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@ 

# Main test program
test.o: test.c 
	$(CC) $(CFLAGS) -c -o $@ $<

# The memory wrapper shared library
memwrap.so: memwrap.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

# Build the test cases object
test_cases.o: test_cases.c
	$(CC) $(CFLAGS) -c -o $@ $<

test_cases.c: test_cases.txt
	./build_cases.py

# Clean up
clean:
	rm -f test memwrap.so test.o 