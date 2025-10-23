CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGETS = myls

.PHONY: all clean

all: $(TARGETS)

myls: myls.c
	$(CC) $(CFLAGS) -o myls myls.c

clean:
	rm -f $(TARGETS)