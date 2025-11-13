#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <getopt.h>
#include <errno.h>

struct archive_record {
    char path[1024];
    struct stat file_stats;
    unsigned char marked_deleted;
};

#define MAX_ALLOWED_SIZE (1024LL * 1024LL * 1024LL)
#define TRANSFER_BUFFER_SIZE 8192

static int complete_write(int fd, const void *buf, size_t count) {
    const char *ptr = (const char *)buf;
    size_t left = count;

    while (left > 0) {
        ssize_t written = write(fd, ptr, left);
        if (written < 0) return -1;
        if (written == 0) {
            errno = EIO;
            return -1;
        }
        ptr += written;
        left -= (size_t)written;
    }
    return 0;
}

static int duplicate_content(int src, int dst, off_t total_bytes) {
    char local_buf[TRANSFER_BUFFER_SIZE];
    off_t bytes_remaining = total_bytes;

    while (bytes_remaining > 0) {
        ssize_t chunk = (bytes_remaining > (off_t)sizeof(local_buf)) ?
        (ssize_t)sizeof(local_buf) : (ssize_t)bytes_remaining;
        ssize_t bytes_read = read(src, local_buf, chunk);

        if (bytes_read <= 0) return -1;
        if (complete_write(dst, local_buf, (size_t)bytes_read) == -1) return -1;

        bytes_remaining -= bytes_read;
    }
    return 0;
}

static int skip_bytes(int fd, off_t length) {
    if (length <= 0) return 0;
    if (lseek(fd, length, SEEK_CUR) != -1) return 0;

    char discard_buf[TRANSFER_BUFFER_SIZE];
    off_t remaining = length;

    while (remaining > 0) {
        ssize_t to_read = (remaining > (off_t)sizeof(discard_buf)) ?
        (ssize_t)sizeof(discard_buf) : (ssize_t)remaining;
        ssize_t bytes_read = read(fd, discard_buf, to_read);
        if (bytes_read <= 0) return -1;
        remaining -= bytes_read;
    }
    return 0;
}

static void apply_original_attributes(const char *filepath, const struct stat *original_stat) {
    if (!filepath || !original_stat) return;

    if (chmod(filepath, original_stat->st_mode) == -1) {
        perror("Внимание: не удалось установить права доступа");
    }

    chown(filepath, original_stat->st_uid, original_stat->st_gid);

    struct utimbuf time_settings = { original_stat->st_atime, original_stat->st_mtime };
    if (utime(filepath, &time_settings) == -1) {
        perror("Внимание: не удалось восстановить временные метки");
    }
}

void show_help_info() {
    printf("Использование: ./archiver <архив> [опции] [файлы]\n");
    printf("Опции:\n");
    printf("  -i, --input <файл>    Добавить файл в архив\n");
    printf("  -e, --extract <файл>  Извлечь файл из архива (с удалением записи)\n");
    printf("  -s, --stat            Показать содержимое архива\n");
    printf("  -h, --help            Показать эту справку\n");
}

int compress_archive_file(const char *archive_path) {
    int input_fd = open(archive_path, O_RDONLY);
    if (input_fd == -1) {
        perror("compress: ошибка открытия архива");
        return -1;
    }

    // Создаем временный файл в текущей директории
    char temp_archive_name[] = "archiver_temp_XXXXXX";

    int temp_fd = mkstemp(temp_archive_name);
    if (temp_fd == -1) {
        perror("compress: ошибка создания временного файла");
        close(input_fd);
        return -1;
    }

    struct stat archive_attributes;
    if (fstat(input_fd, &archive_attributes) == 0) {
        fchmod(temp_fd, archive_attributes.st_mode);
    }

    struct archive_record current_record;
    ssize_t read_bytes;

    while ((read_bytes = read(input_fd, &current_record, sizeof(current_record))) == sizeof(current_record)) {
        off_t data_size = current_record.file_stats.st_size;

        if (!current_record.marked_deleted) {
            if (complete_write(temp_fd, &current_record, sizeof(current_record)) == -1) {
                perror("compress: ошибка записи заголовка");
                goto error_cleanup;
            }

            if (duplicate_content(input_fd, temp_fd, data_size) == -1) {
                perror("compress: ошибка копирования данных");
                goto error_cleanup;
            }
        } else {
            if (skip_bytes(input_fd, data_size) == -1) {
                perror("compress: ошибка пропуска данных");
                goto error_cleanup;
            }
        }
    }

    if (read_bytes == -1) {
        perror("compress: ошибка чтения архива");
        goto error_cleanup;
    }

    fsync(temp_fd);
    close(temp_fd);
    close(input_fd);

    // Заменяем оригинальный файл
    if (rename(temp_archive_name, archive_path) == -1) {
        perror("compress: ошибка замены архива");
        unlink(temp_archive_name);
        return -1;
    }

    return 0;

    error_cleanup:
    close(input_fd);
    close(temp_fd);
    unlink(temp_archive_name);
    return -1;
}

void add_file_to_archive(const char *archive_name, const char *target_file) {
    int archive_fd = open(archive_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (archive_fd == -1) {
        perror("Ошибка: не удалось открыть архив");
        return;
    }

    int source_fd = open(target_file, O_RDONLY);
    if (source_fd == -1) {
        perror("Ошибка: не удалось открыть исходный файл");
        close(archive_fd);
        return;
    }

    struct archive_record new_record;

    if (strlen(target_file) >= sizeof(new_record.path)) {
        printf("Ошибка: имя файла '%s' слишком длинное\n", target_file);
        close(source_fd);
        close(archive_fd);
        return;
    }

    memset(&new_record, 0, sizeof(new_record));
    strncpy(new_record.path, target_file, sizeof(new_record.path) - 1);

    if (fstat(source_fd, &new_record.file_stats) == -1) {
        perror("Ошибка: не удалось получить метаданные файла");
        close(source_fd);
        close(archive_fd);
        return;
    }
    new_record.marked_deleted = 0;

    if (complete_write(archive_fd, &new_record, sizeof(new_record)) == -1) {
        perror("Ошибка: запись заголовка в архив не удалась");
        close(source_fd);
        close(archive_fd);
        return;
    }

    if (duplicate_content(source_fd, archive_fd, new_record.file_stats.st_size) == -1) {
        perror("Ошибка: добавление данных файла в архив не удалось");
        close(source_fd);
        close(archive_fd);
        return;
    }

    printf("Успешно: файл '%s' добавлен в архив '%s'.\n", target_file, archive_name);

    close(source_fd);
    close(archive_fd);
}

void extract_and_remove_file(const char *archive_name, const char *file_to_extract) {
    int archive_fd = open(archive_name, O_RDWR);
    if (archive_fd == -1) {
        perror("Ошибка: не удалось открыть архив");
        return;
    }

    struct archive_record record;
    ssize_t bytes_read;
    int file_found = 0;

    while (1) {
        off_t record_start = lseek(archive_fd, 0, SEEK_CUR);
        if (record_start == -1) {
            perror("Ошибка определения позиции в архиве");
            break;
        }

        bytes_read = read(archive_fd, &record, sizeof(record));
        if (bytes_read == 0) break;
        if (bytes_read != sizeof(record)) {
            perror("Ошибка: чтение заголовка из архива не удалось");
            break;
        }

        if (strcmp(record.path, file_to_extract) == 0 && !record.marked_deleted) {
            file_found = 1;

            if (record.file_stats.st_size > MAX_ALLOWED_SIZE) {
                printf("Предупреждение: файл '%s' слишком большой (размер: %lld байт)\n",
                       file_to_extract, (long long)record.file_stats.st_size);
                if (lseek(archive_fd, record.file_stats.st_size, SEEK_CUR) == -1) {
                    perror("Ошибка: пропуск данных большого файла не удался");
                }
                break;
            }

            int output_fd = open(record.path, O_WRONLY | O_CREAT | O_TRUNC, record.file_stats.st_mode);
            if (output_fd == -1) {
                perror("Ошибка: не удалось создать файл для извлечения");
                if (lseek(archive_fd, record.file_stats.st_size, SEEK_CUR) == -1) {
                    perror("Ошибка: не удалось пропустить данные при неудачном создании файла");
                }
                break;
            }

            if (duplicate_content(archive_fd, output_fd, record.file_stats.st_size) == -1) {
                perror("Ошибка: извлечение данных файла не удалось");
                close(output_fd);
                break;
            }

            close(output_fd);
            apply_original_attributes(record.path, &record.file_stats);

            record.marked_deleted = 1;
            if (lseek(archive_fd, record_start, SEEK_SET) == -1) {
                perror("Ошибка: позиционирование в архиве для пометки удаления не удалось");
            } else if (write(archive_fd, &record, sizeof(record)) != sizeof(record)) {
                perror("Ошибка: запись пометки удаления в архив не удалась");
            }

            close(archive_fd);
            if (compress_archive_file(archive_name) == -1) {
                fprintf(stderr, "Предупреждение: сжатие архива после удаления не выполнено.\n");
            }

            printf("Успешно: файл '%s' извлечён и удалён из архива.\n", file_to_extract);
            return;
        } else {
            if (lseek(archive_fd, record.file_stats.st_size, SEEK_CUR) == -1) {
                perror("Ошибка: позиционирование в архиве при пропуске записи не удалось");
                break;
            }
        }
    }

    if (!file_found) {
        printf("Информация: файл '%s' не найден в архиве.\n", file_to_extract);
    }

    close(archive_fd);
}

void display_archive_contents(const char *archive_name) {
    int archive_fd = open(archive_name, O_RDONLY);
    if (archive_fd == -1) {
        perror("Ошибка: не удалось открыть архив");
        return;
    }

    struct archive_record record;
    ssize_t bytes_read;
    printf("Содержимое архива '%s' (удалённые файлы скрыты):\n", archive_name);
    printf("--------------------------------------------------\n");
    printf("%-30s %-12s %-20s\n", "Имя файла", "Размер (байт)", "Дата изменения");
    printf("--------------------------------------------------\n");

    while ((bytes_read = read(archive_fd, &record, sizeof(record))) > 0) {
        if (bytes_read != sizeof(record)) {
            perror("Ошибка: чтение заголовка при просмотре архива не удалось");
            break;
        }
        if (!record.marked_deleted) {
            char time_buffer[80];
            struct tm *time_info = localtime(&record.file_stats.st_mtime);
            if (time_info) strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);
            else strncpy(time_buffer, "неизвестно", sizeof(time_buffer));
            printf("%-30s %-10lld %-20s\n", record.path,
                   (long long)record.file_stats.st_size, time_buffer);
        }
        if (lseek(archive_fd, record.file_stats.st_size, SEEK_CUR) == -1) {
            perror("Ошибка: позиционирование в архиве при просмотре не удалось");
            break;
        }
    }

    close(archive_fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        show_help_info();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        show_help_info();
        return 0;
    }

    if (argc < 3) {
        show_help_info();
        return 1;
    }

    const char *archive_name = argv[1];

    static struct option long_options[] = {
            {"input",   required_argument, 0, 'i'},
            {"extract", required_argument, 0, 'e'},
            {"stat",    no_argument,       0, 's'},
            {"help",    no_argument,       0, 'h'},
            {0, 0, 0, 0}
    };

    optind = 2;
    int option;
    int option_index = 0;
    option = getopt_long(argc, argv, "i:e:sh", long_options, &option_index);

    switch (option) {
        case 'i':
            if (optarg) add_file_to_archive(archive_name, optarg);
            break;
        case 'e':
            if (optarg) extract_and_remove_file(archive_name, optarg);
            break;
        case 's':
            display_archive_contents(archive_name);
            break;
        case 'h':
            show_help_info();
            break;
        default:
            show_help_info();
            return 1;
    }

    return 0;
}