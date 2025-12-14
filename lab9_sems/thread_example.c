#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define BUFFER_SIZE 256

char shared_buffer[BUFFER_SIZE];
int message_counter = 0;
sem_t buffer_semaphore;

void* writer_thread_function() {
    printf("Writer thread started (TID: %lu)\n", pthread_self());

    while (1) {
        sem_wait(&buffer_semaphore);

        message_counter++;
        time_t current_time = time(NULL);
        struct tm* time_info = localtime(&current_time);

        snprintf(shared_buffer, BUFFER_SIZE,
                 "Message #%d at %02d:%02d:%02d",
                 message_counter,
                 time_info->tm_hour,
                 time_info->tm_min,
                 time_info->tm_sec);

        printf("Writer (TID: %lu): Wrote - %s\n",
               pthread_self(), shared_buffer);

        sem_post(&buffer_semaphore);
        sleep(1);
    }

    return NULL;
}

void* reader_thread_function() {
    printf("Reader thread started (TID: %lu)\n", pthread_self());

    while (1) {
        sem_wait(&buffer_semaphore);

        time_t current_time = time(NULL);
        struct tm* time_info = localtime(&current_time);
        char time_string[30];
        strftime(time_string, sizeof(time_string), "%H:%M:%S", time_info);

        printf("Reader (TID: %lu) | Time: %s | Buffer content: %s\n",
               pthread_self(), time_string, shared_buffer);

        sem_post(&buffer_semaphore);
        sleep(2);
    }

    return NULL;
}

int main() {
    pthread_t writer_thread, reader_thread;

    strcpy(shared_buffer, "Initial empty buffer");

    if (sem_init(&buffer_semaphore, 0, 1) != 0) {
        perror("Failed to initialize semaphore");
        return 1;
    }

    if (pthread_create(&writer_thread, NULL, writer_thread_function, NULL) != 0) {
        perror("Failed to create writer thread");
        return 1;
    }

    if (pthread_create(&reader_thread, NULL, reader_thread_function, NULL) != 0) {
        perror("Failed to create reader thread");
        return 1;
    }

    printf("Main program started. Press Ctrl+C to exit.\n");

    pthread_join(writer_thread, NULL);
    pthread_join(reader_thread, NULL);

    sem_destroy(&buffer_semaphore);

    return 0;
}