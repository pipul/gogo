all:
	gcc -g task.c test.c malloc.c mem_linux.c -lpthread -o test
tst:
	valgrind --log-file=memcheck.log --leak-check=full --show-reachable=yes ./test
clean:
	rm test
