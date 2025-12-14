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
        if (shmdt(shared_address) < 0) {
            fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
        }
    }

    if (shmctl(shared_memory_id, IPC_RMID, NULL) < 0) {
        fprintf(stderr, "Failed to remove shared memory: %s\n", strerror(errno));
    }

    if (semctl(semaphore_id, 0, IPC_RMID) == -1) {
        fprintf(stderr, "Failed to remove semaphore: %s\n", strerror(errno));
    }

    remove(SHARED_MEM_KEY_FILE);
    exit(0);
}

int main() {
    int file_descriptor = open(SHARED_MEM_KEY_FILE, O_CREAT | O_EXCL, S_IWUSR | S_IRUSR);
    if (file_descriptor == -1) {
        fprintf(stderr, "Another instance is already running\n");
        return 1;
    }
    close(file_descriptor);

    if (signal(SIGINT, cleanup_handler) == SIG_ERR ||
        signal(SIGTERM, cleanup_handler) == SIG_ERR) {
        report_error("signal", strerror(errno));
    }

    key_t ipc_key = ftok(SHARED_MEM_KEY_FILE, 'A');
    if (ipc_key == -1) {
        report_error("ftok", strerror(errno));
    }

    shared_memory_id = shmget(ipc_key, BUFFER_SIZE, 0666 | IPC_CREAT | IPC_EXCL);
    if (shared_memory_id == -1) {
        report_error("shmget", strerror(errno));
    }

    shared_address = (char*)shmat(shared_memory_id, NULL, 0);
    if (shared_address == (char*)-1) {
        report_error("shmat", strerror(errno));
    }

    semaphore_id = semget(ipc_key, 1, 0666 | IPC_CREAT | IPC_EXCL);
    if (semaphore_id == -1) {
        report_error("semget", strerror(errno));
    }

    if (semctl(semaphore_id, 0, SETVAL, 1) == -1) {
        report_error("semctl", strerror(errno));
    }

    printf("Sender process started (PID: %d)\n", getpid());

    int message_counter = 0;
    while (1) {
        semop(semaphore_id, &lock_operation, 1);

        time_t current_time = time(NULL);
        struct tm* time_info = localtime(&current_time);
        char message_buffer[100];

        snprintf(message_buffer, sizeof(message_buffer),
                 "[MESSAGE #%d] Time: %02d:%02d:%02d, PID: %d",
                 ++message_counter,
                 time_info->tm_hour,
                 time_info->tm_min,
                 time_info->tm_sec,
                 getpid());

        strncpy(shared_address, message_buffer, BUFFER_SIZE - 1);
        shared_address[BUFFER_SIZE - 1] = '\0';

        printf("Sender: Message sent - %s\n", message_buffer);

        semop(semaphore_id, &unlock_operation, 1);
        sleep(3);
    }

    return 0;
}