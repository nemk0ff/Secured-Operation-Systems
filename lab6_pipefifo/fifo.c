#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <fifo_name>\n", argv[0]);
        return 1;
    }

    const char *fifo_name = argv[1];
    struct stat st;

    if (stat(fifo_name, &st) == 0) {
        fprintf(stderr, "File '%s' already exists\n", fifo_name);
        return 1;
    }

    if (mkfifo(fifo_name, 0600) < 0) {
        perror("mkfifo failed");
        return EXIT_FAILURE;
    }

    int fd;
    char buffer[BUFFER_SIZE];
    time_t now;
    struct tm *timeinfo;
    char time_str[64];
    ssize_t bytes_read;

    pid_t pid = fork();

    switch (pid) {
        case -1:
            perror("fork failed");
            unlink(fifo_name);
            return EXIT_FAILURE;
        case 0:
            /* Child process - reads from FIFO */
            fd = open(fifo_name, O_RDONLY);
            if (fd < 0) {
                perror("open for reading failed");
                unlink(fifo_name);
                return EXIT_FAILURE;
            }

            time(&now);
            timeinfo = localtime(&now);
            strftime(time_str, sizeof(time_str),
                     "%a %b %d %H:%M:%S %Y", timeinfo);

            printf("Child process time: %s\n", time_str);

            bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
            if (bytes_read < 0) {
                perror("read from fifo failed");
                close(fd);
                unlink(fifo_name);
                return EXIT_FAILURE;
            } else if (bytes_read > 0) {
                buffer[bytes_read] = '\0';  // Add null terminator
                printf("Received from parent: %s", buffer);
            }

            if (close(fd) < 0) {
                perror("close failed");
                unlink(fifo_name);
                return EXIT_FAILURE;
            }
            break;
        default:
            /* Parent process - writes to FIFO */
            fd = open(fifo_name, O_WRONLY);
            if (fd < 0) {
                perror("open for writing failed");
                unlink(fifo_name);
                return EXIT_FAILURE;
            }

            sleep(5);

            time(&now);
            timeinfo = localtime(&now);
            strftime(time_str, sizeof(time_str),
                     "%a %b %d %H:%M:%S %Y", timeinfo);

            snprintf(buffer, BUFFER_SIZE,
                     "Parent process time: %s\nParent PID: %d\n",
                     time_str, getpid());

            if (write(fd, buffer, strlen(buffer)) < 0) {
                perror("write to fifo failed");
                close(fd);
                unlink(fifo_name);
                return EXIT_FAILURE;
            }

            if (close(fd) < 0) {
                perror("close failed");
                unlink(fifo_name);
                return EXIT_FAILURE;
            }

            wait(NULL);

            if (unlink(fifo_name) < 0) {
                perror("unlink failed");
                return EXIT_FAILURE;
            }
            break;
    }

    return 0;
}