#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#define BUF_SIZE 256

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Не указан аргумент - имя FIFO.\n");
        return 1;
    }

    const char *fifoname = argv[1];

    struct stat st;
    if (stat(fifoname, &st) == 0) {
        fprintf(stderr, "Файл %s уже существует\n", fifoname);
        return 1;
    }

    if (mkfifo(fifoname, 0600) == -1) {  // Права доступа: чтение/запись только владельцу
        perror("Ошибка создания FIFO");
        return EXIT_FAILURE;
    }

    int fd;
    char buffer[BUF_SIZE];
    time_t t;
    struct tm *tm_info;
    char time_str[64];

    pid_t child_pid = fork();

    switch (child_pid) {
        case -1:
            perror("Ошибка форка");
            unlink(fifoname);
            return EXIT_FAILURE;
        case 0:
            if ((fd = open(fifoname, O_RDONLY)) == -1) {
                perror("Ошибка открытия FIFO для чтения");
                unlink(fifoname);
                return EXIT_FAILURE;
            }

            time(&t);
            tm_info = localtime(&t);
            strftime(time_str, sizeof(time_str), "%a %b %d %T %Y", tm_info);

            printf("Time in CHILD process %s\n", time_str);

            if (read(fd, buffer, BUF_SIZE) == -1) {
                perror("Ошибка чтения из FIFO");
                close(fd);
                unlink(fifoname);
                return EXIT_FAILURE;
            }

            printf("%s", buffer);

            if (close(fd) == -1) {
                perror("Ошибка закрытия FIFO");
                unlink(fifoname);
                return EXIT_FAILURE;
            }

            break;
        default:
            if ((fd = open(fifoname, O_WRONLY)) == -1) {
                perror("Ошибка открытия FIFO для записи");
                unlink(fifoname);
                return EXIT_FAILURE;
            }

            sleep(5);

            time(&t);
            tm_info = localtime(&t);
            strftime(time_str, sizeof(time_str), "%a %b %d %T %Y", tm_info);

            snprintf(buffer, BUF_SIZE, "Time in PARENT process %s\nPARENT pid = %d\n", time_str, getpid());

            if (write(fd, buffer, strlen(buffer)) == -1) {
                perror("Ошибка записи в FIFO");
                close(fd);
                unlink(fifoname);
                return EXIT_FAILURE;
            }

            if (close(fd) == -1) {
                perror("Ошибка закрытия FIFO");
                unlink(fifoname);
                return EXIT_FAILURE;
            }

            wait(NULL);

            if (unlink(fifoname) == -1) {
                perror("Ошибка удаления FIFO");
                return EXIT_FAILURE;
            }

            break;
    }

    return 0;
}