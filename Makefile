CC = gcc -g
CFLAGS = -Werror -Wextra -Wall

OBJS = main.o queue.o

all: main

main: $(OBJS)
	$(CC) $(CFLAGS) -o usage $(OBJS)

main.o: main.c queue.h

clean:
	rm -f *~ *.o usage
