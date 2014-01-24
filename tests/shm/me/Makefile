CC=gcc
NAME=me
CFLAGS=-Wall -g -c
OBJS=me.o lock.o barrier.o
all: $(NAME)

$(NAME): ${OBJS}
	$(CC) -o $@ ${OBJS}

me.o: me.c me.h
	$(CC) ${CFLAGS} $< -o $@
	
lock.o: lock.c lock.h
	$(CC) ${CFLAGS} $< -o $@

barrier.o: barrier.c barrier.h
	$(CC) ${CFLAGS} $< -o $@
	
clean:
	rm -f $(NAME) *.o
