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

enum DisplayModes {
    SHOW_ALL = 1,
    LONG_FORMAT = 2
};

enum ColorTypes {
    CLR_REGULAR,
    CLR_DIRECTORY,
    CLR_EXECUTABLE,
    CLR_SYMLINK
};

enum ErrorTypes {
    ERR_BAD_OPTION,
    ERR_CANT_OPEN_DIR,
    ERR_CANT_READ_DIR,
    ERR_CANT_STAT,
    ERR_CANT_GET_PWD,
    ERR_CANT_GET_GRP
};

static const int COLOR_VALUES[] = {39, 34, 32, 36};
static const char * const OPTION_FLAGS = "hla";
static const char * const PERMISSION_LETTERS = "rwx";
static const char * const ANSI_RESET_CODE = "\x1b[0m";
static const char * const ANSI_COLOR_START = "\x1b[";
static const size_t MAX_PATH_SIZE =
#ifdef PATH_MAX
        (size_t)PATH_MAX
#else
        (size_t)4096
#endif
;

typedef struct {
    char *pathStorage;
    const char *rootPath;
    DIR *dirHandle;
    size_t itemTotal;
    struct dirent **itemArray;
    int settings;
} RuntimeContext;

RuntimeContext ctx = {0};

void setupContext();
void cleanupContext();
void handleError(enum ErrorTypes errCode);
void makeFilePath(const char *fileEntry);
void readDirItems();
void showItem(struct dirent *dirEntry);
void scanDirectory(const char *pathName);
int compareEntries(const void *entryA, const void *entryB);

int main(int argc, char **argv) {
    setupContext();

    opterr = 0;
    int selectedOption;
    while ((selectedOption = getopt(argc, argv, OPTION_FLAGS)) != -1) {
        switch (selectedOption) {
            case 'h':
                printf("ls - list directory contents\n"
                       "usage: ls [params...] [file]\n"
                       " -a - do not ignore entries starting with '.'\n"
                       " -l - use a long listing format\n"
                       " -h - print this message\n");
                cleanupContext();
                exit(EXIT_SUCCESS);
            case 'l':
                ctx.settings |= LONG_FORMAT;
                break;
            case 'a':
                ctx.settings |= SHOW_ALL;
                break;
            case '?':
                handleError(ERR_BAD_OPTION);
                break;
        }
    }

    if (optind < argc - 1) {
        fprintf(stderr, "[ls]: too many arguments\n");
        cleanupContext();
        exit(EXIT_FAILURE);
    }

    const char *targetPath = (optind == argc) ? "." : argv[optind];
    scanDirectory(targetPath);
    cleanupContext();
    return EXIT_SUCCESS;
}

void setupContext() {
    ctx.pathStorage = (char *)malloc(MAX_PATH_SIZE);
    if (!ctx.pathStorage) {
        fprintf(stderr, "[ls]: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

void cleanupContext() {
    if (ctx.pathStorage) {
        free(ctx.pathStorage);
        ctx.pathStorage = NULL;
    }
    if (ctx.dirHandle) {
        closedir(ctx.dirHandle);
        ctx.dirHandle = NULL;
    }
    if (ctx.itemArray) {
        for (size_t i = 0; i < ctx.itemTotal; i++) {
            free(ctx.itemArray[i]);
        }
        free(ctx.itemArray);
        ctx.itemArray = NULL;
    }
}

void handleError(enum ErrorTypes errCode) {
    const char *errorMsg = NULL;

    switch (errCode) {
        case ERR_BAD_OPTION:
            errorMsg = "invalid option, see \"ls -h\"";
            break;
        case ERR_CANT_OPEN_DIR:
            errorMsg = strerror(errno);
            break;
        case ERR_CANT_READ_DIR:
            errorMsg = strerror(errno);
            break;
        case ERR_CANT_STAT:
            errorMsg = strerror(errno);
            break;
        case ERR_CANT_GET_PWD:
            errorMsg = strerror(errno);
            break;
        case ERR_CANT_GET_GRP:
            errorMsg = strerror(errno);
            break;
    }

    fprintf(stderr, "[ls]: %s\n", errorMsg);
    cleanupContext();
    exit(EXIT_FAILURE);
}

void makeFilePath(const char *fileEntry) {
    if (!ctx.rootPath) {
        ctx.rootPath = ".";
    }

    int charsWritten = snprintf(ctx.pathStorage, MAX_PATH_SIZE, "%s/%s",
                                ctx.rootPath, fileEntry);
    if (charsWritten < 0 || (size_t)charsWritten >= MAX_PATH_SIZE) {
        fprintf(stderr, "[ls]: path too long\n");
        cleanupContext();
        exit(EXIT_FAILURE);
    }
}

void readDirItems() {
    struct dirent *currentEntry;
    size_t arrayCapacity = 32;

    ctx.itemArray = (struct dirent **)malloc(arrayCapacity * sizeof(struct dirent *));
    if (!ctx.itemArray) {
        handleError(ERR_CANT_READ_DIR);
    }

    ctx.itemTotal = 0;
    errno = 0;

    while ((currentEntry = readdir(ctx.dirHandle)) != NULL) {
        if (ctx.itemTotal >= arrayCapacity) {
            arrayCapacity *= 2;
            struct dirent **newArray = (struct dirent **)realloc(
                    ctx.itemArray, arrayCapacity * sizeof(struct dirent *));
            if (!newArray) {
                for (size_t i = 0; i < ctx.itemTotal; i++) {
                    free(ctx.itemArray[i]);
                }
                free(ctx.itemArray);
                ctx.itemArray = NULL;
                handleError(ERR_CANT_READ_DIR);
            }
            ctx.itemArray = newArray;
        }

        struct dirent *entryCopy = (struct dirent *)malloc(sizeof(struct dirent));
        if (!entryCopy) {
            for (size_t i = 0; i < ctx.itemTotal; i++) {
                free(ctx.itemArray[i]);
            }
            free(ctx.itemArray);
            ctx.itemArray = NULL;
            handleError(ERR_CANT_READ_DIR);
        }
        memcpy(entryCopy, currentEntry, sizeof(struct dirent));
        ctx.itemArray[ctx.itemTotal++] = entryCopy;
    }

    if (errno) {
        for (size_t i = 0; i < ctx.itemTotal; i++) {
            free(ctx.itemArray[i]);
        }
        free(ctx.itemArray);
        ctx.itemArray = NULL;
        handleError(ERR_CANT_READ_DIR);
    }

    qsort(ctx.itemArray, ctx.itemTotal, sizeof(struct dirent *), compareEntries);
}

void showItem(struct dirent *dirEntry) {
    if (dirEntry->d_name[0] == '.' && !(ctx.settings & SHOW_ALL)) {
        free(dirEntry);
        return;
    }

    makeFilePath(dirEntry->d_name);
    enum ColorTypes itemColor = CLR_REGULAR;
    struct stat fileAttributes;

    if (lstat(ctx.pathStorage, &fileAttributes) == -1) {
        free(dirEntry);
        handleError(ERR_CANT_STAT);
    }

    if (ctx.settings & LONG_FORMAT) {
        char typeChar = '?';
        if (S_ISREG(fileAttributes.st_mode)) {
            typeChar = '-';
            if (fileAttributes.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                itemColor = CLR_EXECUTABLE;
            }
        } else if (S_ISDIR(fileAttributes.st_mode)) {
            typeChar = 'd';
            itemColor = CLR_DIRECTORY;
        } else if (S_ISCHR(fileAttributes.st_mode)) {
            typeChar = 'c';
        } else if (S_ISBLK(fileAttributes.st_mode)) {
            typeChar = 'b';
        } else if (S_ISFIFO(fileAttributes.st_mode)) {
            typeChar = 'p';
        } else if (S_ISLNK(fileAttributes.st_mode)) {
            typeChar = 'l';
            itemColor = CLR_SYMLINK;
        } else if (S_ISSOCK(fileAttributes.st_mode)) {
            typeChar = 's';
        }

        putchar(typeChar);

        mode_t permBits = fileAttributes.st_mode;
        const mode_t permMasks[] = {S_IRUSR, S_IWUSR, S_IXUSR,
                                    S_IRGRP, S_IWGRP, S_IXGRP,
                                    S_IROTH, S_IWOTH, S_IXOTH};

        for (int i = 0; i < 9; i++) {
            putchar(permBits & permMasks[i] ? PERMISSION_LETTERS[i % 3] : '-');
        }
        putchar(' ');

        printf("%lu ", (unsigned long)fileAttributes.st_nlink);

        struct passwd *userInfo = getpwuid(fileAttributes.st_uid);
        if (userInfo) {
            printf("%-8s ", userInfo->pw_name);
        } else {
            printf("%-8u ", (unsigned)fileAttributes.st_uid);
        }

        struct group *groupInfo = getgrgid(fileAttributes.st_gid);
        if (groupInfo) {
            printf("%-8s ", groupInfo->gr_name);
        } else {
            printf("%-8u ", (unsigned)fileAttributes.st_gid);
        }

        if (S_ISCHR(fileAttributes.st_mode) || S_ISBLK(fileAttributes.st_mode)) {
            printf("%8lld ", (long long)fileAttributes.st_size);
        } else {
            printf("%8lld ", (long long)fileAttributes.st_size);
        }

        char timeString[64];
        struct tm *timeData = localtime(&fileAttributes.st_mtime);
        strftime(timeString, sizeof(timeString), "%b %d %H:%M", timeData);
        printf("%s ", timeString);

        printf("%s%dm%s%s", ANSI_COLOR_START, COLOR_VALUES[itemColor],
               dirEntry->d_name, ANSI_RESET_CODE);

        if (S_ISLNK(fileAttributes.st_mode)) {
            char linkPath[PATH_MAX];
            ssize_t linkSize = readlink(ctx.pathStorage, linkPath, sizeof(linkPath) - 1);
            if (linkSize != -1) {
                linkPath[linkSize] = '\0';
                printf(" -> %s", linkPath);
            }
        }
        printf("\n");
    } else {
        if (S_ISDIR(fileAttributes.st_mode)) {
            itemColor = CLR_DIRECTORY;
        } else if (S_ISLNK(fileAttributes.st_mode)) {
            itemColor = CLR_SYMLINK;
        } else if (fileAttributes.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            itemColor = CLR_EXECUTABLE;
        }

        printf("%s%dm%-20s%s", ANSI_COLOR_START, COLOR_VALUES[itemColor],
               dirEntry->d_name, ANSI_RESET_CODE);
    }

    free(dirEntry);
}

void scanDirectory(const char *pathName) {
    ctx.dirHandle = opendir(pathName);
    if (!ctx.dirHandle) {
        handleError(ERR_CANT_OPEN_DIR);
    }

    ctx.rootPath = pathName;
    readDirItems();

    for (size_t i = 0; i < ctx.itemTotal; i++) {
        showItem(ctx.itemArray[i]);
    }

    ctx.itemTotal = 0;
    free(ctx.itemArray);
    ctx.itemArray = NULL;

    if (!(ctx.settings & LONG_FORMAT)) {
        putchar('\n');
    }
}

int compareEntries(const void *entryA, const void *entryB) {
    const struct dirent *first = *(const struct dirent **)entryA;
    const struct dirent *second = *(const struct dirent **)entryB;

    if (first->d_name[0] == '.' && second->d_name[0] != '.') return -1;
    if (first->d_name[0] != '.' && second->d_name[0] == '.') return 1;

    return strcasecmp(first->d_name, second->d_name);
}