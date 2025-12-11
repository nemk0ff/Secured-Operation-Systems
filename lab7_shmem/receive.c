#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "header.h"

void *shmaddr;
int shmid;

void free_at_exit(int sig)
{
    (void)sig;
    shmdt(shmaddr);
    exit(EXIT_SUCCESS);
}

int main()
{
    key_t shmkey = ftok(FNAME, FTOK_ID);
    if (shmkey == -1)
    {
        if (errno == ENOENT)
        {
            fprintf(stderr, "[receiver]: you should execute \"sender\" program before!\n");
        }
        exit(EXIT_FAILURE);
    }

    shmid = shmget(shmkey, sizeof(shm_transfer_t), 0);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    shmaddr = shmat(shmid, NULL, SHM_RDONLY);
    if (shmaddr == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, free_at_exit);
    signal(SIGTERM, free_at_exit);

    for (;;)
    {
        shm_transfer_t received_data;
        memcpy(&received_data, shmaddr, sizeof(shm_transfer_t));

        if (strlen(received_data.data_string) > 0) {
            printf("[receiver]: got data: %s\n", received_data.data_string);

            time_t local_time = time(NULL);
            char local_time_buffer[80];
            struct tm* local_time_info = localtime(&local_time);
            strftime(local_time_buffer, sizeof(local_time_buffer),
                     "%Y-%m-%d %H:%M:%S", local_time_info);

            printf("[receiver]: receiver time: %s, receiver PID: %d\n",
                   local_time_buffer, getpid());
        } else {
            printf("[receiver]: no data available\n");
        }

        sleep(5);
    }
}