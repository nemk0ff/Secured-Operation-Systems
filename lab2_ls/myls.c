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

#define FLAG_ALL 0x01
#define FLAG_LONG 0x02
#define FLAG_HELP 0x04

#define COLOR_NORMAL 0
#define COLOR_FOLDER 1
#define COLOR_EXE 2
#define COLOR_LINK 3

#define PATH_BUFFER_SIZE 4096

static const int color_codes[] = {39, 34, 32, 36};
static const char *option_str = "hla";
static const char *perm_chars = "rwx";
static const char *reset_seq = "\x1b[0m";
static const char *color_seq = "\x1b[";
static const mode_t perm_flags[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
};

typedef struct {
    char *buffer;
    const char *location;
    DIR *stream;
    size_t count;
    struct dirent **list;
    unsigned char flags;
} AppData;

AppData app = {0};

void init_app();
void cleanup_app();
void report_error(int code);
void build_full_path(const char *name);
void load_directory_entries();
void display_entry(struct dirent *entry);
void process_directory(const char *target);
int entry_sorter(const void *a, const void *b);

int main(int argc, char **argv) {
    init_app();

    opterr = 0;
    int opt_val;
    while ((opt_val = getopt(argc, argv, option_str)) != -1) {
        switch (opt_val) {
            case 'h':
                printf("Directory listing utility\n"
                       "Usage: %s [options] [directory]\n"
                       "Options:\n"
                       "  -a  Include hidden entries\n"
                       "  -l  Detailed view\n"
                       "  -h  Display help\n", argv[0]);
                cleanup_app();
                return 0;
            case 'l':
                app.flags |= FLAG_LONG;
                break;
            case 'a':
                app.flags |= FLAG_ALL;
                break;
            case '?':
                report_error(1);
                break;
        }
    }

    if (optind < argc - 1) {
        fprintf(stderr, "Error: Too many arguments provided\n");
        cleanup_app();
        return 1;
    }

    const char *target = (optind == argc) ? "." : argv[optind];
    process_directory(target);
    cleanup_app();
    return 0;
}

void init_app() {
    app.buffer = (char *)malloc(PATH_BUFFER_SIZE);
    if (!app.buffer) {
        fprintf(stderr, "Memory allocation failure\n");
        exit(1);
    }
}

void cleanup_app() {
    if (app.buffer) {
        free(app.buffer);
        app.buffer = NULL;
    }
    if (app.stream) {
        closedir(app.stream);
        app.stream = NULL;
    }
    if (app.list) {
        for (size_t i = 0; i < app.count; i++) {
            free(app.list[i]);
        }
        free(app.list);
        app.list = NULL;
    }
}

void report_error(int code) {
    const char *msg = NULL;

    switch (code) {
        case 1:
            msg = "Invalid option. Use -h for help";
            break;
        case 2:
            msg = strerror(errno);
            break;
        case 3:
            msg = strerror(errno);
            break;
        case 4:
            msg = strerror(errno);
            break;
        case 5:
            msg = strerror(errno);
            break;
        case 6:
            msg = strerror(errno);
            break;
    }

    fprintf(stderr, "Operation failed: %s\n", msg);
    cleanup_app();
    exit(1);
}

void build_full_path(const char *name) {
    if (!app.location) {
        app.location = ".";
    }

    int len = snprintf(app.buffer, PATH_BUFFER_SIZE, "%s/%s",
                       app.location, name);
    if (len < 0 || (size_t)len >= PATH_BUFFER_SIZE) {
        fprintf(stderr, "Path exceeds maximum length\n");
        cleanup_app();
        exit(1);
    }
}

void load_directory_entries() {
    struct dirent *current;
    size_t capacity = 32;

    app.list = (struct dirent **)malloc(capacity * sizeof(struct dirent *));
    if (!app.list) {
        report_error(3);
    }

    app.count = 0;
    errno = 0;

    while ((current = readdir(app.stream)) != NULL) {
        if (app.count >= capacity) {
            capacity *= 2;
            struct dirent **new_list = (struct dirent **)realloc(
                    app.list, capacity * sizeof(struct dirent *));
            if (!new_list) {
                for (size_t i = 0; i < app.count; i++) {
                    free(app.list[i]);
                }
                free(app.list);
                app.list = NULL;
                report_error(3);
            }
            app.list = new_list;
        }

        struct dirent *copy = (struct dirent *)malloc(sizeof(struct dirent));
        if (!copy) {
            for (size_t i = 0; i < app.count; i++) {
                free(app.list[i]);
            }
            free(app.list);
            app.list = NULL;
            report_error(3);
        }
        memcpy(copy, current, sizeof(struct dirent));
        app.list[app.count++] = copy;
    }

    if (errno) {
        for (size_t i = 0; i < app.count; i++) {
            free(app.list[i]);
        }
        free(app.list);
        app.list = NULL;
        report_error(3);
    }

    qsort(app.list, app.count, sizeof(struct dirent *), entry_sorter);
}

void display_entry(struct dirent *entry) {
    if (entry->d_name[0] == '.' && !(app.flags & FLAG_ALL)) {
        free(entry);
        return;
    }

    build_full_path(entry->d_name);
    int color_idx = COLOR_NORMAL;
    struct stat info;

    if (lstat(app.buffer, &info) == -1) {
        free(entry);
        report_error(4);
    }

    if (app.flags & FLAG_LONG) {
        char type_sym = '?';
        if (S_ISREG(info.st_mode)) {
            type_sym = '-';
            if (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                color_idx = COLOR_EXE;
            }
        } else if (S_ISDIR(info.st_mode)) {
            type_sym = 'd';
            color_idx = COLOR_FOLDER;
        } else if (S_ISCHR(info.st_mode)) {
            type_sym = 'c';
        } else if (S_ISBLK(info.st_mode)) {
            type_sym = 'b';
        } else if (S_ISFIFO(info.st_mode)) {
            type_sym = 'p';
        } else if (S_ISLNK(info.st_mode)) {
            type_sym = 'l';
            color_idx = COLOR_LINK;
        } else if (S_ISSOCK(info.st_mode)) {
            type_sym = 's';
        }

        putchar(type_sym);

        mode_t perms = info.st_mode;
        for (int i = 0; i < 9; i++) {
            putchar(perms & perm_flags[i] ? perm_chars[i % 3] : '-');
        }
        putchar(' ');

        printf("%lu ", (unsigned long)info.st_nlink);

        struct passwd *user = getpwuid(info.st_uid);
        if (user) {
            printf("%-8s ", user->pw_name);
        } else {
            printf("%-8u ", (unsigned)info.st_uid);
        }

        struct group *grp = getgrgid(info.st_gid);
        if (grp) {
            printf("%-8s ", grp->gr_name);
        } else {
            printf("%-8u ", (unsigned)info.st_gid);
        }

        if (S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode)) {
            printf("%8lld ", (long long)info.st_size);
        } else {
            printf("%8lld ", (long long)info.st_size);
        }

        char time_buf[64];
        struct tm *tm_ptr = localtime(&info.st_mtime);
        strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm_ptr);
        printf("%s ", time_buf);

        printf("%s%dm%s%s", color_seq, color_codes[color_idx],
               entry->d_name, reset_seq);

        if (S_ISLNK(info.st_mode)) {
            char link_path[PATH_MAX];
            ssize_t link_len = readlink(app.buffer, link_path, sizeof(link_path) - 1);
            if (link_len != -1) {
                link_path[link_len] = '\0';
                printf(" -> %s", link_path);
            }
        }
        printf("\n");
    } else {
        if (S_ISDIR(info.st_mode)) {
            color_idx = COLOR_FOLDER;
        } else if (S_ISLNK(info.st_mode)) {
            color_idx = COLOR_LINK;
        } else if (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            color_idx = COLOR_EXE;
        }

        printf("%s%dm%-20s%s", color_seq, color_codes[color_idx],
               entry->d_name, reset_seq);
    }

    free(entry);
}

void process_directory(const char *target) {
    app.stream = opendir(target);
    if (!app.stream) {
        report_error(2);
    }

    app.location = target;
    load_directory_entries();

    for (size_t i = 0; i < app.count; i++) {
        display_entry(app.list[i]);
    }

    app.count = 0;
    free(app.list);
    app.list = NULL;

    if (!(app.flags & FLAG_LONG)) {
        putchar('\n');
    }
}

int entry_sorter(const void *a, const void *b) {
    const struct dirent *first = *(const struct dirent **)a;
    const struct dirent *second = *(const struct dirent **)b;

    if (first->d_name[0] == '.' && second->d_name[0] != '.') return -1;
    if (first->d_name[0] != '.' && second->d_name[0] == '.') return 1;

    return strcasecmp(first->d_name, second->d_name);
}