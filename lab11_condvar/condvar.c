#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define ARRAY_SIZE 10
#define NUM_READERS 10
#define BUFFER_SIZE 256

char shared_array[ARRAY_SIZE];      // Shared character array
int write_counter = 0;              // Number of elements written
int all_written = 0;                // Flag indicating all writing is done
int data_available = 0;             // Flag indicating new data is available

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;      // Mutex for synchronization
pthread_cond_t data_ready_cond = PTHREAD_COND_INITIALIZER;  // Condition variable

// Writer thread function - writes to shared array
void* writer_thread(void* arg) {
    (void)arg;  // Unused parameter

    printf("Writer thread started (TID: %lx)\n", pthread_self());

    while (write_counter < ARRAY_SIZE) {
        pthread_mutex_lock(&mutex);      // Lock mutex BEFORE writing

        // Write to shared array
        shared_array[write_counter] = 'A' + write_counter;
        printf("Writer: wrote '%c' at index %d\n",
               shared_array[write_counter], write_counter);
        write_counter++;

        data_available = 1;              // Mark data as available
        pthread_cond_signal(&data_ready_cond);  // Signal ONE reader

        pthread_mutex_unlock(&mutex);    // Unlock mutex AFTER writing

        usleep(100000);  // Small delay between writes
    }

    // Signal that all writing is done
    pthread_mutex_lock(&mutex);
    all_written = 1;
    data_available = 1;
    pthread_cond_broadcast(&data_ready_cond);  // Signal ALL readers
    pthread_mutex_unlock(&mutex);

    printf("Writer finished writing all %d elements\n", ARRAY_SIZE);
    return NULL;
}

// Reader thread function - reads from shared array
void* reader_thread(void* arg) {
    int reader_id = *((int*)arg);

    printf("Reader %d started (TID: %lx)\n", reader_id, pthread_self());

    while (1) {
        pthread_mutex_lock(&mutex);      // Lock mutex BEFORE reading

        // Wait for data to be available using condition variable
        while (!data_available && !all_written) {
            pthread_cond_wait(&data_ready_cond, &mutex);
        }

        // Check if all writing is done and no more data
        if (all_written && write_counter == ARRAY_SIZE) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Read and display current state of array
        printf("Reader %d (TID: %lx): Array = [", reader_id, pthread_self());
        for (int i = 0; i < ARRAY_SIZE; i++) {
            if (i < write_counter) {
                printf("%c", shared_array[i]);
            } else {
                printf("_");
            }
            if (i < ARRAY_SIZE - 1) printf(" ");
        }
        printf("]\n");

        data_available = 0;  // Mark data as processed

        pthread_mutex_unlock(&mutex);    // Unlock mutex AFTER reading

        usleep(50000);  // Small delay between reads
    }

    printf("Reader %d finished\n", reader_id);
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writer;
    int reader_ids[NUM_READERS];

    // Initialize shared array with underscores
    memset(shared_array, '_', ARRAY_SIZE);

    printf("=== Laboratory Work #11: Condition Variables ===\n");
    printf("Creating 1 writer and %d reader threads\n", NUM_READERS);

    // Create writer thread
    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) {
        fprintf(stderr, "Error creating writer thread: %s\n", strerror(errno));
        return 1;
    }

    // Create reader threads
    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i + 1;
        if (pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]) != 0) {
            fprintf(stderr, "Error creating reader thread %d: %s\n", i, strerror(errno));
            return 1;
        }
    }

    // Wait for all threads to complete
    printf("\n=== Waiting for threads to complete ===\n");

    for (int i = 0; i < NUM_READERS; i++) {
        if (pthread_join(readers[i], NULL) != 0) {
            fprintf(stderr, "Error joining reader thread %d: %s\n", i, strerror(errno));
        }
    }

    if (pthread_join(writer, NULL) != 0) {
        fprintf(stderr, "Error joining writer thread: %s\n", strerror(errno));
    }

    // Display final state
    printf("\n=== Final state of shared array ===\n");
    printf("Final array: [");
    for (int i = 0; i < ARRAY_SIZE; i++) {
        printf("%c", shared_array[i]);
        if (i < ARRAY_SIZE - 1) printf(" ");
    }
    printf("]\n");

    printf("Program finished successfully\n");

    return 0;
}