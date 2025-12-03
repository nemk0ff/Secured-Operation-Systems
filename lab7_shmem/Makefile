.PHONY: all send receive clean
all: receive send
send: send.c
	gcc send.c -o send
receive: receive.c
	gcc receive.c -o receive
clean:
	rm -rf *.o send receive