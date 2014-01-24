CC=gcc
NAME=rap
CFLAGS=-Wall -g -c
OBJS=rap.o lock.o barrier.o
all: $(NAME)

$(NAME): ${OBJS}
	$(CC) -o $@ ${OBJS}

rap.o: rap.c rap.h
	$(CC) ${CFLAGS} $< -o $@
	
lock.o: lock.c lock.h
	$(CC) ${CFLAGS} $< -o $@

barrier.o: barrier.c barrier.h
	$(CC) ${CFLAGS} $< -o $@
	
clean:
	rm -f $(NAME) *.o
