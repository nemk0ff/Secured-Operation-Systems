#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sem.h>

#define SHARED_MEM_KEY_FILE "shared_memory_key"
#define BUFFER_SIZE 1024

int shared_memory_id = 0;
int semaphore_id = 0;
char* shared_address = NULL;

struct sembuf lock_operation = {0, -1, 0};
struct sembuf unlock_operation = {0, 1, 0};

void report_error(const char* function_name, const char* error_message) {
    fprintf(stderr, "ERROR in %s: %s\n", function_name, error_message);
    exit(EXIT_FAILURE);
}

void cleanup_handler(int signal_number) {
    printf("[SIGNAL HANDLER] Received signal: %d\n", signal_number);

    if (shared_address != NULL) {
        if (shmdt(shared_address) == -1) {
            fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
        }
    }
    exit(0);
}

int main() {
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    key_t ipc_key = ftok(SHARED_MEM_KEY_FILE, 'A');
    if (ipc_key == (key_t)-1) {
        report_error("ftok", strerror(errno));
    }

    shared_memory_id = shmget(ipc_key, BUFFER_SIZE, 0666);
    if (shared_memory_id < 0) {
        report_error("shmget", strerror(errno));
    }

    shared_address = shmat(shared_memory_id, NULL, 0);
    if (shared_address == (void*)-1) {
        report_error("shmat", strerror(errno));
    }

    semaphore_id = semget(ipc_key, 1, 0666);
    if (semaphore_id == -1) {
        report_error("semget", strerror(errno));
    }

    printf("Receiver process started (PID: %d)\n", getpid());

    while (1) {
        semop(semaphore_id, &lock_operation, 1);

        char received_message[100];
        strncpy(received_message, shared_address, sizeof(received_message) - 1);
        received_message[sizeof(received_message) - 1] = '\0';

        time_t current_time = time(NULL);
        struct tm* time_info = localtime(&current_time);
        char time_string[30];
        strftime(time_string, sizeof(time_string), "%H:%M:%S", time_info);

        printf("[RECEIVER] Local time: %s, PID: %d, Received: %s\n",
               time_string, getpid(), received_message);

        semop(semaphore_id, &unlock_operation, 1);
        sleep(1);
    }

    return 0;
}