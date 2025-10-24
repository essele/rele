
CC = gcc
CFLAGS = -02 -Wall


tree:	tree.o rele.o
	$(CC) $(FLAGS) -o $@ $^

rele.o: ../rele/rele.c
	$(CC) $(FLAGS) -o $@ -c $<

%.o: %.c
	$(CC) $(FLAGS) -o $@ -c $<


