#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>
#include <getopt.h>

enum ListFlags {
    LS_ALL = 1,
    LS_LONG = 2
};

enum FileColor {
    COL_FILE,
    COL_DIR,
    COL_EXEC,
    COL_LN
};

enum ErrorCode {
    ERR_INVALIDOPT,
    ERR_OPENDIR,
    ERR_READDIR,
    ERR_STAT,
    ERR_PWD,
    ERR_GRP
};

static const int FILE_TYPE_COLORS[] = {39, 34, 32, 36};

static const char * const VALID_OPTIONS = "hla";
static const char * const PERMISSION_CHARS = "rwx";
static const char * const ANSI_RESET = "\x1b[0m";
static const char * const ANSI_COLOR_PREFIX = "\x1b[";
static const size_t MAX_PATH_LENGTH =
#ifdef PATH_MAX
        (size_t)PATH_MAX
#else
        (size_t)4096
#endif
;

typedef struct {
    char *pathBuffer;
    const char *basePath;
    DIR *openDir;
    size_t entryCount;
    struct dirent **entries;
    int flags;
} ProgramState;

ProgramState g_state = {0};

void initProgram();
void cleanupProgram();
void failWithError(enum ErrorCode err);
void composePath(const char *fileName);
void collectEntries();
void printEntry(struct dirent *entry);
void listDirectory(const char *dirPath);
int direntCompare(const void *a, const void *b);

int main(int argc, char **argv) {
    initProgram();

    opterr = 0;
    int option;
    while ((option = getopt(argc, argv, VALID_OPTIONS)) != -1) {
        switch (option) {
            case 'h':
                printf("ls - list directory contents\n"
                       "usage: ls [params...] [file]\n"
                       " -a - do not ignore entries starting with '.'\n"
                       " -l - use a long listing format\n"
                       " -h - print this message\n");
                cleanupProgram();
                exit(EXIT_SUCCESS);
            case 'l':
                g_state.flags |= LS_LONG;
                break;
            case 'a':
                g_state.flags |= LS_ALL;
                break;
            case '?':
                failWithError(ERR_INVALIDOPT);
                break;
        }
    }

    if (optind < argc - 1) {
        fprintf(stderr, "[ls]: too many arguments\n");
        cleanupProgram();
        exit(EXIT_FAILURE);
    }

    const char *targetDir = (optind == argc) ? "." : argv[optind];
    listDirectory(targetDir);
    cleanupProgram();
    return EXIT_SUCCESS;
}

void initProgram() {
    g_state.pathBuffer = (char *)malloc(MAX_PATH_LENGTH);
    if (!g_state.pathBuffer) {
        fprintf(stderr, "[ls]: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

void cleanupProgram() {
    if (g_state.pathBuffer) {
        free(g_state.pathBuffer);
        g_state.pathBuffer = NULL;
    }
    if (g_state.openDir) {
        closedir(g_state.openDir);
        g_state.openDir = NULL;
    }
    if (g_state.entries) {
        for (size_t i = 0; i < g_state.entryCount; i++) {
            free(g_state.entries[i]);
        }
        free(g_state.entries);
        g_state.entries = NULL;
    }
}

void failWithError(enum ErrorCode err) {
    const char *message = NULL;

    switch (err) {
        case ERR_INVALIDOPT:
            message = "invalid option, see \"ls -h\"";
            break;
        case ERR_OPENDIR:
            message = strerror(errno);
            break;
        case ERR_READDIR:
            message = strerror(errno);
            break;
        case ERR_STAT:
            message = strerror(errno);
            break;
        case ERR_PWD:
            message = strerror(errno);
            break;
        case ERR_GRP:
            message = strerror(errno);
            break;
    }

    fprintf(stderr, "[ls]: %s\n", message);
    cleanupProgram();
    exit(EXIT_FAILURE);
}

void composePath(const char *fileName) {
    if (!g_state.basePath) {
        g_state.basePath = ".";
    }

    int written = snprintf(g_state.pathBuffer, MAX_PATH_LENGTH, "%s/%s",
                           g_state.basePath, fileName);
    if (written < 0 || (size_t)written >= MAX_PATH_LENGTH) {
        fprintf(stderr, "[ls]: path too long\n");
        cleanupProgram();
        exit(EXIT_FAILURE);
    }
}

void collectEntries() {
    struct dirent *entry;
    size_t capacity = 32;

    g_state.entries = (struct dirent **)malloc(capacity * sizeof(struct dirent *));
    if (!g_state.entries) {
        failWithError(ERR_READDIR);
    }

    g_state.entryCount = 0;
    errno = 0;

    while ((entry = readdir(g_state.openDir)) != NULL) {
        if (g_state.entryCount >= capacity) {
            capacity *= 2;
            struct dirent **newEntries = (struct dirent **)realloc(
                    g_state.entries, capacity * sizeof(struct dirent *));
            if (!newEntries) {
                for (size_t i = 0; i < g_state.entryCount; i++) {
                    free(g_state.entries[i]);
                }
                free(g_state.entries);
                g_state.entries = NULL;
                failWithError(ERR_READDIR);
            }
            g_state.entries = newEntries;
        }

        struct dirent *entryCopy = (struct dirent *)malloc(sizeof(struct dirent));
        if (!entryCopy) {
            for (size_t i = 0; i < g_state.entryCount; i++) {
                free(g_state.entries[i]);
            }
            free(g_state.entries);
            g_state.entries = NULL;
            failWithError(ERR_READDIR);
        }
        memcpy(entryCopy, entry, sizeof(struct dirent));
        g_state.entries[g_state.entryCount++] = entryCopy;
    }

    if (errno) {
        for (size_t i = 0; i < g_state.entryCount; i++) {
            free(g_state.entries[i]);
        }
        free(g_state.entries);
        g_state.entries = NULL;
        failWithError(ERR_READDIR);
    }

    qsort(g_state.entries, g_state.entryCount, sizeof(struct dirent *), direntCompare);
}

void printEntry(struct dirent *entry) {
    if (entry->d_name[0] == '.' && !(g_state.flags & LS_ALL)) {
        free(entry);
        return;
    }

    composePath(entry->d_name);
    enum FileColor filename_color = COL_FILE;
    struct stat file_info;

    if (lstat(g_state.pathBuffer, &file_info) == -1) {
        free(entry);
        failWithError(ERR_STAT);
    }

    if (g_state.flags & LS_LONG) {
        char fileTypeChar = '?';
        if (S_ISREG(file_info.st_mode)) {
            fileTypeChar = '-';
            if (file_info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                filename_color = COL_EXEC;
            }
        } else if (S_ISDIR(file_info.st_mode)) {
            fileTypeChar = 'd';
            filename_color = COL_DIR;
        } else if (S_ISCHR(file_info.st_mode)) {
            fileTypeChar = 'c';
        } else if (S_ISBLK(file_info.st_mode)) {
            fileTypeChar = 'b';
        } else if (S_ISFIFO(file_info.st_mode)) {
            fileTypeChar = 'p';
        } else if (S_ISLNK(file_info.st_mode)) {
            fileTypeChar = 'l';
            filename_color = COL_LN;
        } else if (S_ISSOCK(file_info.st_mode)) {
            fileTypeChar = 's';
        }

        putchar(fileTypeChar);

        mode_t mode = file_info.st_mode;
        const mode_t permissions[] = {S_IRUSR, S_IWUSR, S_IXUSR,
                                      S_IRGRP, S_IWGRP, S_IXGRP,
                                      S_IROTH, S_IWOTH, S_IXOTH};

        for (int i = 0; i < 9; i++) {
            putchar(mode & permissions[i] ? PERMISSION_CHARS[i % 3] : '-');
        }
        putchar(' ');

        printf("%lu ", (unsigned long)file_info.st_nlink);

        struct passwd *pwd_file = getpwuid(file_info.st_uid);
        if (pwd_file) {
            printf("%-8s ", pwd_file->pw_name);
        } else {
            printf("%-8u ", (unsigned)file_info.st_uid);
        }

        struct group *grp_file = getgrgid(file_info.st_gid);
        if (grp_file) {
            printf("%-8s ", grp_file->gr_name);
        } else {
            printf("%-8u ", (unsigned)file_info.st_gid);
        }

        // Для специальных файлов (символьных и блочных устройств) выводим только размер
        if (S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode)) {
            printf("%8lld ", (long long)file_info.st_size);
        } else {
            printf("%8lld ", (long long)file_info.st_size);
        }

        char time_buf[64];
        struct tm *tm_info = localtime(&file_info.st_mtime);
        strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm_info);
        printf("%s ", time_buf);

        printf("%s%dm%s%s", ANSI_COLOR_PREFIX, FILE_TYPE_COLORS[filename_color],
               entry->d_name, ANSI_RESET);

        if (S_ISLNK(file_info.st_mode)) {
            char link_buf[PATH_MAX];
            ssize_t link_len = readlink(g_state.pathBuffer, link_buf, sizeof(link_buf) - 1);
            if (link_len != -1) {
                link_buf[link_len] = '\0';
                printf(" -> %s", link_buf);
            }
        }
        printf("\n");
    } else {
        if (S_ISDIR(file_info.st_mode)) {
            filename_color = COL_DIR;
        } else if (S_ISLNK(file_info.st_mode)) {
            filename_color = COL_LN;
        } else if (file_info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            filename_color = COL_EXEC;
        }

        printf("%s%dm%-20s%s", ANSI_COLOR_PREFIX, FILE_TYPE_COLORS[filename_color],
               entry->d_name, ANSI_RESET);
    }

    free(entry);
}

void listDirectory(const char *dirPath) {
    g_state.openDir = opendir(dirPath);
    if (!g_state.openDir) {
        failWithError(ERR_OPENDIR);
    }

    g_state.basePath = dirPath;
    collectEntries();

    for (size_t i = 0; i < g_state.entryCount; i++) {
        printEntry(g_state.entries[i]);
    }

    g_state.entryCount = 0;
    free(g_state.entries);
    g_state.entries = NULL;

    if (!(g_state.flags & LS_LONG)) {
        putchar('\n');
    }
}

int direntCompare(const void *a, const void *b) {
    const struct dirent *f = *(const struct dirent **)a;
    const struct dirent *s = *(const struct dirent **)b;

    if (f->d_name[0] == '.' && s->d_name[0] != '.') return -1;
    if (f->d_name[0] != '.' && s->d_name[0] == '.') return 1;

    return strcasecmp(f->d_name, s->d_name);
}