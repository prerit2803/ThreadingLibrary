all: mythread.c
	gcc -c mythread.c
	ar cr libmyth.a mythread.o