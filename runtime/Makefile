CC = gcc
OS = _OS_linux
ARCH = _ARCH_amd64
DEBUG = -g -DDEBUG -DVALGRIND
CFLAGS = -W -Wall $(DEBUG)
OBJS = 	task.o \
	malloc.o \
	mem_linux.o \
	spinlock.o \
	syscall_linux.o

LIBRARY = libgogo.a

all: $(LIBRARY)

$(LIBRARY): $(OBJS)
	ar crv libgogo.a $(OBJS)

tst:
	$(CC) test.c $(LIBRARY) -lpthread -o test.out
	valgrind --log-file=memcheck.log --leak-check=full --show-reachable=yes ./test.out
clean:
	rm *.o && rm $(LIBRARY) && rm test.out
