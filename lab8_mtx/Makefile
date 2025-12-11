# Практическая работа №8: Читающие и пишущие потоки с мьютексом
CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread
TARGET = threads_demo
SOURCES = main.c
HEADERS = thread_ops.h

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET)

debug: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -g $(SOURCES) -o $(TARGET)_debug $(LDFLAGS)
	gdb ./$(TARGET)_debug

valgrind: $(TARGET)
	valgrind --leak-check=full ./$(TARGET)