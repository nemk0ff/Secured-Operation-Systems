#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>

// Имя сегмента разделяемой памяти
const char *SHM_NAME = "/shm_time";
const char *SEM_NAME = "/sem_time"; // Имя семафора

// Структура данных для передачи
typedef struct {
    time_t timestamp;
    pid_t pid;
    char message[128];
} shared_data_t;

void cleanup(int sig) {
    shm_unlink(SHM_NAME); // Освобождаем разделяемую память при получении сигнала
    sem_unlink(SEM_NAME); // Освобождаем семафор при получении сигнала
    exit(EXIT_SUCCESS);
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Создаем семафор для синхронизации
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Ошибка создания семафора");
        return EXIT_FAILURE;
    }

    // Количество попыток открыть сегмент
    int attempts = 5;
    for (int i = 0; i < attempts; i++) {
        // Открываем существующий сегмент разделяемой памяти
        int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0644);
        if (shm_fd != -1) {
            break;
        } else if (i == attempts - 1) {
            perror("Ошибка открытия разделяемой памяти");
            sem_close(sem);
            return EXIT_FAILURE;
        } else {
            sleep(1);
        }
    }

    // Открываем существующий сегмент разделяемой памяти
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0644);
    if (shm_fd == -1) {
        perror("Ошибка открытия разделяемой памяти");
        sem_close(sem);
        return EXIT_FAILURE;
    }

    // Отображаем сегмент разделяемой памяти в адресное пространство процесса
    shared_data_t *data = mmap(NULL, sizeof(shared_data_t), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("Ошибка отображения разделяемой памяти");
        close(shm_fd);
        sem_close(sem);
        return EXIT_FAILURE;
    }

    while (1) {
        // Блокируем семафор перед чтением данных
        sem_wait(sem);

        // Проверяем, если сообщение не пустое
        if (strlen(data->message) > 0) {
            printf("Received: %s\n", data->message);

            time_t current_time = time(NULL);
            char buffer[26];  //хранение строки времени
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

            printf("Current Time: %s, Current PID: %d\n", buffer, getpid());
        } else {
            printf("No message received.\n");
        }

        // Освобождаем семафор после чтения
        sem_post(sem);
        sleep(5);
    }

    munmap(data, sizeof(shared_data_t));
    close(shm_fd);
    sem_close(sem);

    return 0;
}