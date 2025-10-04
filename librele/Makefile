CC      = gcc
CFLAGS  = -Wall -g -fPIC --stack-usage -fdebug-prefix-map=/=

all: test


test: test.c librele.c
	$(CC) $(CFLAGS) -O2 -o $@ $<
