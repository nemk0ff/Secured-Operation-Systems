# Build configuration for shared memory demonstration
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lrt -lpthread

.PHONY: all transmitter receiver clean

all: transmitter receiver

transmitter: sender.c
	$(CC) $(CFLAGS) sender.c -o transmitter $(LDFLAGS)

receiver: receiver.c
	$(CC) $(CFLAGS) receiver.c -o receiver $(LDFLAGS)

clean:
	rm -f transmitter receiver

run_transmitter: transmitter
	./transmitter

run_receiver: receiver
	./receiver