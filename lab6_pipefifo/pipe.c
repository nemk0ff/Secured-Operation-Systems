#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 256

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int pipe_fd[2];
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    if (pipe(pipe_fd) < 0) {
        perror("pipe creation failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    time_t now;
    struct tm *timeinfo;
    char time_str[64];

    switch(pid) {
        case -1:
            perror("fork failed");
            exit(EXIT_FAILURE);
        case 0:
            /* Child process - reads from pipe */
            close(pipe_fd[1]);

            time(&now);
            timeinfo = localtime(&now);
            strftime(time_str, sizeof(time_str),
                     "%a %b %d %H:%M:%S %Y", timeinfo);

            printf("Child process time: %s\n", time_str);

            bytes_read = read(pipe_fd[0], buffer, BUFFER_SIZE - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';  // Add null terminator
                printf("Received from parent: %s", buffer);
            } else if (bytes_read < 0) {
                perror("read failed");
            }

            close(pipe_fd[0]);
            break;
        default:
            /* Parent process - writes to pipe */
            close(pipe_fd[0]);

            sleep(5);

            time(&now);
            timeinfo = localtime(&now);
            strftime(time_str, sizeof(time_str),
                     "%a %b %d %H:%M:%S %Y", timeinfo);

            snprintf(buffer, BUFFER_SIZE,
                     "Parent process time: %s\nParent PID: %d\n",
                     time_str, getpid());

            write(pipe_fd[1], buffer, strlen(buffer));

            close(pipe_fd[1]);

            wait(NULL);
            break;
    }

    return 0;
}