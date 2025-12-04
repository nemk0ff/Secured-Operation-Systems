#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define SHM_SEGMENT_NAME "/time_transfer_shm"
#define LOCK_SEM_NAME "/single_instance_lock"
#define DATA_SIZE 128

typedef struct {
    time_t current_time;
    pid_t process_id;
    char data_string[DATA_SIZE];
} shm_transfer_t;

sem_t* instance_lock;

void resource_release(void) {
    shm_unlink(SHM_SEGMENT_NAME);
    if (instance_lock != NULL) {
        sem_close(instance_lock);
        sem_unlink(LOCK_SEM_NAME);
    }
}

void signal_handler(int sig_num) {
    (void)sig_num;
    resource_release();
    exit(EXIT_SUCCESS);
}

int setup_shared_memory(shm_transfer_t** data_ptr) {
    int shm_desc = shm_open(SHM_SEGMENT_NAME, O_CREAT | O_RDWR, 0644);
    if (shm_desc < 0) {
        perror("shm_open failed");
        return -1;
    }

    if (ftruncate(shm_desc, sizeof(shm_transfer_t)) < 0) {
        perror("ftruncate failed");
        close(shm_desc);
        return -1;
    }

    *data_ptr = mmap(NULL, sizeof(shm_transfer_t),
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED, shm_desc, 0);

    if (*data_ptr == MAP_FAILED) {
        perror("mmap failed");
        close(shm_desc);
        return -1;
    }

    close(shm_desc);
    return 0;
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    instance_lock = sem_open(LOCK_SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (instance_lock == SEM_FAILED) {
        if (errno == EEXIST) {
            fprintf(stderr, "Transmitter already running\n");
            return EXIT_FAILURE;
        }
        perror("sem_open failed");
        return EXIT_FAILURE;
    }

    shm_transfer_t* transfer_data;
    if (setup_shared_memory(&transfer_data) < 0) {
        sem_close(instance_lock);
        sem_unlink(LOCK_SEM_NAME);
        return EXIT_FAILURE;
    }

    char time_buffer[80];

    while (1) {
        transfer_data->current_time = time(NULL);
        transfer_data->process_id = getpid();

        struct tm* time_info = localtime(&transfer_data->current_time);
        strftime(time_buffer, sizeof(time_buffer),
                 "%Y-%m-%d %H:%M:%S", time_info);

        snprintf(transfer_data->data_string, DATA_SIZE,
                 "Sender time: %s, Sender PID: %d",
                 time_buffer, transfer_data->process_id);

        sleep(5);
    }

    munmap(transfer_data, sizeof(shm_transfer_t));
    resource_release();

    return EXIT_SUCCESS;
}