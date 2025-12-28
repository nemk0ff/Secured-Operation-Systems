#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define ARRAY_SIZE 10
#define NUM_READERS 10
#define BUFFER_SIZE 256

char shared_array[ARRAY_SIZE];
int write_counter = 0;
int read_finished = 0;

pthread_rwlock_t rwlock;

void* writer_thread(void* arg) {
    (void)arg;

    while (write_counter < ARRAY_SIZE) {
        pthread_rwlock_wrlock(&rwlock);

        if (write_counter < ARRAY_SIZE) {
            shared_array[write_counter] = 'A' + write_counter;
            write_counter++;
            printf("Writer: wrote '%c' at index %d\n",
                   shared_array[write_counter-1], write_counter-1);
        }

        pthread_rwlock_unlock(&rwlock);
        usleep(50000);
    }

    pthread_rwlock_wrlock(&rwlock);
    read_finished = 1;
    pthread_rwlock_unlock(&rwlock);

    return NULL;
}

void* reader_thread(void* arg) {
    int tid = *((int*)arg);

    while (1) {
        pthread_rwlock_rdlock(&rwlock);

        if (read_finished) {
            pthread_rwlock_unlock(&rwlock);
            break;
        }

        char buffer[BUFFER_SIZE];
        int pos = 0;

        pos += snprintf(buffer + pos, BUFFER_SIZE - pos,
                        "Reader %d (TID: %lx): Array = [", tid, pthread_self());

        for (int i = 0; i < ARRAY_SIZE; i++) {
            if (i < write_counter) {
                pos += snprintf(buffer + pos, BUFFER_SIZE - pos,
                                "%c", shared_array[i]);
            } else {
                pos += snprintf(buffer + pos, BUFFER_SIZE - pos, "_");
            }

            if (i < ARRAY_SIZE - 1) {
                pos += snprintf(buffer + pos, BUFFER_SIZE - pos, " ");
            }
        }

        pos += snprintf(buffer + pos, BUFFER_SIZE - pos, "]\n");
        printf("%s", buffer);

        pthread_rwlock_unlock(&rwlock);
        usleep(100000);
    }

    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writer;
    int reader_ids[NUM_READERS];

    memset(shared_array, '_', ARRAY_SIZE);

    if (pthread_rwlock_init(&rwlock, NULL) != 0) {
        fprintf(stderr, "Error initializing rwlock: %s\n", strerror(errno));
        return 1;
    }

    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) {
        fprintf(stderr, "Error creating writer thread: %s\n", strerror(errno));
        pthread_rwlock_destroy(&rwlock);
        return 1;
    }

    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i + 1;
        if (pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]) != 0) {
            fprintf(stderr, "Error creating reader thread %d: %s\n", i, strerror(errno));
            pthread_rwlock_destroy(&rwlock);
            return 1;
        }
    }

    for (int i = 0; i < NUM_READERS; i++) {
        if (pthread_join(readers[i], NULL) != 0) {
            fprintf(stderr, "Error joining reader thread %d: %s\n", i, strerror(errno));
        }
    }

    if (pthread_join(writer, NULL) != 0) {
        fprintf(stderr, "Error joining writer thread: %s\n", strerror(errno));
    }

    pthread_rwlock_destroy(&rwlock);
    printf("Program finished successfully\n");

    return 0;
}