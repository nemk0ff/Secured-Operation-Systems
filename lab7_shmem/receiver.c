#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>

#define SHM_SEGMENT_NAME "/time_transfer_shm"
#define SYNC_SEM_NAME "/shm_access_sem"
#define MAX_ATTEMPTS 10
#define DATA_SIZE 128

typedef struct {
    time_t current_time;
    pid_t process_id;
    char data_string[DATA_SIZE];
} shm_transfer_t;

void cleanup_resources(int sig_num) {
    (void)sig_num;
    shm_unlink(SHM_SEGMENT_NAME);
    sem_unlink(SYNC_SEM_NAME);
    exit(EXIT_SUCCESS);
}

int open_shared_memory_segment(void) {
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        int shm_desc = shm_open(SHM_SEGMENT_NAME, O_RDONLY, 0644);
        if (shm_desc >= 0) {
            return shm_desc;
        }
        if (attempt == MAX_ATTEMPTS - 1) {
            perror("shm_open retry exhausted");
            return -1;
        }
        sleep(1);
    }
    return -1;
}

int main(void) {
    signal(SIGINT, cleanup_resources);
    signal(SIGTERM, cleanup_resources);

    sem_t* access_sem = sem_open(SYNC_SEM_NAME, O_CREAT, 0644, 1);
    if (access_sem == SEM_FAILED) {
        perror("sem_open failed");
        return EXIT_FAILURE;
    }

    int shm_desc = open_shared_memory_segment();
    if (shm_desc < 0) {
        sem_close(access_sem);
        return EXIT_FAILURE;
    }

    shm_transfer_t* received_data = mmap(NULL, sizeof(shm_transfer_t),
                                         PROT_READ, MAP_SHARED, shm_desc, 0);

    if (received_data == MAP_FAILED) {
        perror("mmap failed");
        close(shm_desc);
        sem_close(access_sem);
        return EXIT_FAILURE;
    }

    char local_time_buffer[26];

    while (1) {
        sem_wait(access_sem);

        if (strlen(received_data->data_string) > 0) {
            printf("Data received: %s\n", received_data->data_string);

            time_t local_time = time(NULL);
            struct tm* local_time_info = localtime(&local_time);
            strftime(local_time_buffer, sizeof(local_time_buffer),
                     "%Y-%m-%d %H:%M:%S", local_time_info);

            printf("Receiver time: %s, Receiver PID: %d\n",
                   local_time_buffer, getpid());
        } else {
            printf("No data available\n");
        }

        sem_post(access_sem);
        sleep(5);
    }

    munmap(received_data, sizeof(shm_transfer_t));
    close(shm_desc);
    sem_close(access_sem);

    return EXIT_SUCCESS;
}