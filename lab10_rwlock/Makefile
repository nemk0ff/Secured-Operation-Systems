CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c11
TARGET = rwlock
SOURCE = rwlock.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run