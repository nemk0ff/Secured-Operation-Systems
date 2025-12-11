#pragma once

#define FNAME "/tmp/.time-transfer-info"
#define BUFSIZE 256
#define FTOK_ID 1

typedef struct {
    time_t current_time;
    pid_t process_id;
    char data_string[BUFSIZE];
} shm_transfer_t;