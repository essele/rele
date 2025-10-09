
CC = gcc
CFLAGS = -02 -Wall


tree:	tree.o librele.o
	$(CC) $(FLAGS) -o $@ $^

librele.o: ../librele/librele.c
	$(CC) $(FLAGS) -o $@ -c $<

%.o: %.c
	$(CC) $(FLAGS) -o $@ -c $<


