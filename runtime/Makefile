CC = gcc
DEBUG = -g -DDEBUG -DVALGRIND
CFLAGS = -W -Wall $(DEBUG)
OBJS = 	task.o \
	malloc.o \
	mem_linux.o \
	spinlock.o

LIBRARY = libgogo.a

all: $(LIBRARY)

$(LIBRARY): $(OBJS)
	ar crv libgogo.a $(OBJS)

tst:
	$(CC) test.c $(LIBRARY) -lpthread -o test
	valgrind --log-file=memcheck.log --leak-check=full --show-reachable=yes ./test
clean:
	rm *.o && rm $(LIBRARY) && rm test
