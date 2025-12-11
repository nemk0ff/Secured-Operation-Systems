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

int shmid;
void *shmaddr;

void free_at_exit(int sig)
{
    (void)sig;
    shmdt(shmaddr);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FNAME);
    exit(EXIT_SUCCESS);
}

void write_string_in_shm()
{
    shm_transfer_t transfer_data;

    transfer_data.current_time = time(NULL);
    transfer_data.process_id = getpid();

    struct tm* time_info = localtime(&transfer_data.current_time);
    char time_buffer[80];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);

    snprintf(transfer_data.data_string, BUFSIZE,
             "Sender time: %s, Sender PID: %d",
             time_buffer, transfer_data.process_id);

    memcpy(shmaddr, &transfer_data, sizeof(shm_transfer_t));
    printf("[sender]: data has been written: %s\n", transfer_data.data_string);
}

int main()
{
    // Создаем файл для ftok
    int fd = open(FNAME, O_CREAT | O_EXCL | O_WRONLY, 0666);
    if (fd == -1)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "[sender]: transmitter already running!\n");
        }
        else
        {
            perror("open");
        }
        exit(EXIT_FAILURE);
    }
    close(fd);

    key_t shmkey = ftok(FNAME, FTOK_ID);
    if (shmkey == -1)
    {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    // Создаем shared memory
    shmid = shmget(shmkey, sizeof(shm_transfer_t), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    shmaddr = shmat(shmid, NULL, 0);
    if (shmaddr == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Настраиваем обработку сигналов
    signal(SIGTERM, free_at_exit);
    signal(SIGINT, free_at_exit);
    memset(shmaddr, 0, sizeof(shm_transfer_t));

    // mainloop
    for (;;)
    {
        write_string_in_shm();
        sleep(5);
    }
}