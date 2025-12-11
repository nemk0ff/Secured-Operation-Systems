CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =

.PHONY: all clean sender receiver run_sender run_receiver

all: sender receiver

sender: send.c header.h
	$(CC) $(CFLAGS) send.c -o sender $(LDFLAGS)

receiver: receive.c header.h
	$(CC) $(CFLAGS) receive.c -o receiver $(LDFLAGS)

clean:
	rm -f sender receiver
	rm -f /tmp/.time-transfer-info

run_sender: sender
	./sender

run_receiver: receiver
	./receiver