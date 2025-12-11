#ifndef THREAD_OPS_H
#define THREAD_OPS_H

#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 10
#define READER_THREADS 10

extern pthread_mutex_t buffer_lock;
extern int shared_data[BUFFER_SIZE];
extern int write_position;

// Writer thread function
void* writer_routine(void* arg);

// Reader thread function
void* reader_routine(void* arg);

// Initialize synchronization resources
bool init_sync_resources(void);

// Clean up resources
void cleanup_sync_resources(void);

#endif