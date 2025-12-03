#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

// Имя сегмента разделяемой памяти
const char *SHM_NAME = "/shm_time";
const char *SEMAPHORE_NAME = "/shm_sem"; // Имя семафора

// Структура данных для передачи
typedef struct {
    time_t timestamp;
    pid_t pid;
    char message[128];
} shared_data_t;

sem_t *sem;

void cleanup(void) {
    shm_unlink(SHM_NAME);
    sem_close(sem);
    sem_unlink(SEMAPHORE_NAME);
}

void handle_signal(int sig) {
    cleanup();
    exit(0);
}

int main() {
    // Обработка сигналов
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Инициализация семафора для проверки уникального запуска
    sem = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) {
        if (errno == EEXIST) {
            fprintf(stderr, "Процесс уже запущен!\n");
            return EXIT_FAILURE;
        } else {
            perror("Ошибка создания семафора");
            return EXIT_FAILURE;
        }
    }

    // Создаем сегмент разделяемой памяти
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (shm_fd == -1) {
        perror("Ошибка создания разделяемой памяти");
        sem_close(sem);
        sem_unlink(SEMAPHORE_NAME);
        return EXIT_FAILURE;
    }

    // Устанавливаем размер сегмента разделяемой памяти
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("Ошибка установки размера разделяемой памяти");
        shm_unlink(SHM_NAME);
        sem_close(sem);
        sem_unlink(SEMAPHORE_NAME);
        return EXIT_FAILURE;
    }

    // Отображаем сегмент разделяемой памяти в адресное пространство процесса
    shared_data_t *data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("Ошибка отображения разделяемой памяти");
        shm_unlink(SHM_NAME);
        sem_close(sem);
        sem_unlink(SEMAPHORE_NAME);
        return EXIT_FAILURE;
    }

    close(shm_fd);

    while (1) {
        data->timestamp = time(NULL);
        data->pid = getpid();

        char formatted_time[80];
        strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d %H:%M:%S", localtime(&data->timestamp));
        snprintf(data->message, sizeof(data->message), "Time: %s, PID: %d", formatted_time, data->pid);

        sleep(5);
    }

    munmap(data, sizeof(shared_data_t));
    cleanup(); // Освобождаем ресурсы

    return 0;
}