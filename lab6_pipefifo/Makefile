# Флаги компилятора
CFLAGS = -Wall -g

all: mypipe myfifo

mypipe: ./pipe.c
	gcc $(CFLAGS) -o mypipe ./pipe.c

myfifo: ./fifo.c
	gcc $(CFLAGS) -o myfifo ./fifo.c

clean:
	rm -f myfifo mypipe

rebuild: clean all