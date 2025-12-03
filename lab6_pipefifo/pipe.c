#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 256

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    int fd[2];
    char buffer[BUF_SIZE];

    if (pipe(fd) == -1) {
        perror("Ошибка создания pipe");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();

    time_t t;
    struct tm *tm_info;
    char time_str[64];

    switch(child_pid) {
        case -1:
            perror("Ошибка форка");
            exit(EXIT_FAILURE);
            break;
        case 0:
            close(fd[1]);

            time(&t);
            tm_info = localtime(&t);
            strftime(time_str, sizeof(time_str), "%a %b %d %T %Y", tm_info);

            printf("Time in CHILD process %s\n", time_str);

            read(fd[0], buffer, BUF_SIZE);
            printf("%s", buffer);

            close(fd[0]);
            break;
        default:
            close(fd[0]);

            sleep(5);

            time(&t);
            tm_info = localtime(&t);
            strftime(time_str, sizeof(time_str), "%a %b %d %T %Y", tm_info);

            snprintf(buffer, BUF_SIZE, "Time in PARENT process %s\nPARENT pid = %d\n", time_str, getpid());

            write(fd[1], buffer, strlen(buffer));

            close(fd[1]);

            wait(NULL);
            break;
    }

    return 0;
}