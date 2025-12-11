#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "thread_ops.h"

// Global shared resources
pthread_mutex_t buffer_lock;
int shared_data[BUFFER_SIZE];
int write_position = 0;

// Initialize synchronization resources
bool init_sync_resources(void) {
    int mutex_status = pthread_mutex_init(&buffer_lock, NULL);
    if (mutex_status != 0) {
        fprintf(stderr, "Mutex initialization failed: %s\n",
                strerror(mutex_status));
        return false;
    }

    // Initialize buffer with zeros
    for (int i = 0; i < BUFFER_SIZE; i++) {
        shared_data[i] = 0;
    }

    return true;
}

// Clean up resources
void cleanup_sync_resources(void) {
    pthread_mutex_destroy(&buffer_lock);
}

// Writer thread function
void* writer_routine(void* arg) {
    (void)arg; // Unused parameter

    while (write_position < BUFFER_SIZE) {
        pthread_mutex_lock(&buffer_lock);

        // Simulate data processing
        usleep(2000);

        if (write_position < BUFFER_SIZE) {
            // Write next value
            shared_data[write_position] = write_position + 1;
            printf("WRITER: Added value %d at position %d\n",
                   shared_data[write_position], write_position);
            write_position++;
        }

        pthread_mutex_unlock(&buffer_lock);

        // Delay between writes
        usleep(150000);
    }

    printf("WRITER: Finished writing %d elements\n", write_position);
    return NULL;
}

// Reader thread function
void* reader_routine(void* arg) {
    int reader_id = *((int*)arg);
    unsigned long thread_id = pthread_self();

    while (1) {
        pthread_mutex_lock(&buffer_lock);

        // Simulate reading delay
        usleep(5000);

        printf("READER %d: Buffer contents: [", reader_id);
        for (int i = 0; i < write_position; i++) {
            printf("%d", shared_data[i]);
            if (i < write_position - 1) {
                printf(", ");
            }
        }
        printf("], Thread ID: 0x%lx\n", thread_id);

        // Check if writing is complete
        if (write_position >= BUFFER_SIZE) {
            pthread_mutex_unlock(&buffer_lock);
            break;
        }

        pthread_mutex_unlock(&buffer_lock);

        // Delay between reads
        usleep(100000 + (reader_id * 3000));
    }

    printf("READER %d: Finished reading\n", reader_id);
    return NULL;
}

int main(void) {
    pthread_t readers[READER_THREADS];
    pthread_t writer;
    int reader_ids[READER_THREADS];

    printf("=== Thread Synchronization Demo ===\n");
    printf("Reader threads: %d\n", READER_THREADS);
    printf("Buffer size: %d\n", BUFFER_SIZE);

    // Initialize synchronization resources
    if (!init_sync_resources()) {
        return EXIT_FAILURE;
    }

    // Create writer thread
    int create_status = pthread_create(&writer, NULL, writer_routine, NULL);
    if (create_status != 0) {
        fprintf(stderr, "Failed to create writer thread: %s\n",
                strerror(create_status));
        cleanup_sync_resources();
        return EXIT_FAILURE;
    }

    // Create reader threads
    for (int i = 0; i < READER_THREADS; i++) {
        reader_ids[i] = i + 1; // Number readers starting from 1
        create_status = pthread_create(&readers[i], NULL,
                                       reader_routine, &reader_ids[i]);
        if (create_status != 0) {
            fprintf(stderr, "Failed to create reader %d: %s\n",
                    i + 1, strerror(create_status));

            // Cancel writer and already created readers
            pthread_cancel(writer);
            for (int j = 0; j < i; j++) {
                pthread_cancel(readers[j]);
            }

            cleanup_sync_resources();
            return EXIT_FAILURE;
        }
    }

    // Wait for reader threads to complete
    for (int i = 0; i < READER_THREADS; i++) {
        void* thread_result = NULL;
        int join_status = pthread_join(readers[i], &thread_result);

        if (join_status != 0) {
            fprintf(stderr, "Error joining reader %d: %s\n",
                    i + 1, strerror(join_status));
        }
    }

    // Wait for writer thread to complete
    void* writer_result = NULL;
    int join_status = pthread_join(writer, &writer_result);
    if (join_status != 0) {
        fprintf(stderr, "Error joining writer: %s\n",
                strerror(join_status));
    }

    // Display final state
    printf("\n=== Final Buffer State ===\n");
    printf("Total elements written: %d\n", write_position);
    printf("Buffer contents: [");
    for (int i = 0; i < write_position; i++) {
        printf("%d", shared_data[i]);
        if (i < write_position - 1) {
            printf(", ");
        }
    }
    printf("]\n");

    // Clean up resources
    cleanup_sync_resources();

    printf("Program completed successfully\n");
    return EXIT_SUCCESS;
}