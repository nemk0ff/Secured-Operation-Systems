CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic
LDFLAGS = -pthread

# Targets for Task 9.2 (Process communication)
TASK92_TARGETS = sender receiver

# Targets for Task 9.1 (Thread communication)
TASK91_TARGETS = thread_example

# Default target
all: $(TASK92_TARGETS) $(TASK91_TARGETS)

# Compilation rules
sender: send.c
	$(CC) $(CFLAGS) -o $@ $<

receiver: receive.c
	$(CC) $(CFLAGS) -o $@ $<

thread_example: thread_example.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Clean build files
clean:
	rm -f $(TASK92_TARGETS) $(TASK91_TARGETS)
	rm -f shared_memory_key

# Run tests
test-task92: sender receiver
	@echo "Starting Task 9.2 test..."
	@echo "Run in separate terminals:"
	@echo "1. ./sender"
	@echo "2. ./receiver"

test-task91: thread_example
	@echo "Starting Task 9.1 test..."
	./thread_example

.PHONY: all clean test-task92 test-task91