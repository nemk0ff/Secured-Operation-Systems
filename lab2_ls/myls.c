#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

typedef enum {
    FLAG_ALL = 1, // -a
    FLAG_LONG = 2 // -l
} Flags;

typedef enum {
    ENTRY_UNDEFINED = 0, // standart color
    ENTRY_FILE,          // standart color
    ENTRY_DIR,           // blue
    ENTRY_LINK,          // cyan
    ENTRY_CHAR,          // yellow bold
    ENTRY_BLOCK,         // yellow bold
    ENTRY_PIPE,          // yellow
    ENTRY_SOCK,          // magneta bold
} EntryType;

const char EntryCodes[] = "--dlcbps";

#define USER_READ(m) ((m) & S_IRUSR)
#define USER_WRITE(m) ((m) & S_IWUSR)
#define USER_EXECUTE(m) ((m) & S_IXUSR)

#define GROUP_READ(m) ((m) & S_IRGRP)
#define GROUP_WRITE(m) ((m) & S_IWGRP)
#define GROUP_EXECUTE(m) ((m) & S_IXGRP)

#define OTHER_READ(m) ((m) & S_IROTH)
#define OTHER_WRITE(m) ((m) & S_IWOTH)
#define OTHER_EXECUTE(m) ((m) & S_IXOTH)

const char Permissions[] = "rwx";

size_t count_total(const char* dir);
bool list_directory(const char* path, int flags);
bool print_entry(const char* base_path, const char* entry, int flags);

// allocates memory
char* concat_strings(const char* base_path, const char* entry);

void set_color(int colorcode);
void set_color_bold(int colorcode);
void reset_color();

const char flag_string[] = "hal";

int main(int argc, char** argv) {
    int flags = 0;
    bool flag_usage = false;
    int opt;

    while ((opt = getopt(argc, argv, flag_string)) != -1) {
        switch (opt) {
            case 'h':
                flag_usage = true;
                break;
            case 'a':
                flags |= FLAG_ALL;
                break;
            case 'l':
                flags |= FLAG_LONG;
                break;

            default:
                exit(1);
                break;
        }
    }

    if (flag_usage) {
        printf("Usage:\n");
        printf("myls [-h -l -a] [path]\n");
        printf("\t-h\tHelp\n");
        printf("\t-l\tLong print\n");
        printf("\t-a\tList all files\n");

        exit(0);
    }


    char* default_path = "./";

    char* path;

    if (optind == argc) path = default_path;
    else path = argv[optind];

    size_t pathlen = strlen(path);

    char* adjusted_path = path;

    if (path[pathlen-1] != '/') {
        adjusted_path = malloc( sizeof(char)*(pathlen + 2) );
        memcpy(adjusted_path, path, pathlen);
        adjusted_path[pathlen] = '/';
        adjusted_path[pathlen+1] = '\0';
    }

    if (!list_directory(adjusted_path, flags)) {
        exit(1);
    }

    if (adjusted_path != path) {
        free(adjusted_path);
    }

    return 0;
}

void set_color(int colorcode) {
    printf("\e[%dm", colorcode);
}
void set_color_bold(int colorcode) {
    printf("\e[%d;1m", colorcode);
}
void reset_color() {
    printf("\e[0m");
}

char* get_link_target(const char* link_path) {
    char buffer[PATH_MAX];

    ssize_t size = readlink(link_path, buffer, sizeof(buffer));
    if (size == -1) {
        fprintf(stderr, "Cannot read link %s: %s\n", link_path, strerror(errno));
        return NULL;
    }

    char* result = malloc(size + 1);
    if (result == NULL) {
        fprintf(stderr, "Cannot allocate %ld bytes\n", size);
        return NULL;
    }

    memcpy(result, buffer, size);
    result[size] = '\0';

    return result;
}

// allocates memory
char* concat_strings(const char* s1, const char* s2) {
    size_t s1_len = strlen(s1);
    size_t s2_len = strlen(s2);
    char* res = malloc(s1_len + s2_len + 1);
    memcpy(res, s1, s1_len);
    memcpy(res + s1_len, s2, s2_len);
    res[s1_len + s2_len] = '\0';
    return res;
}

bool print_entry(const char* base_path, const char* entry_name, int flags) {
    bool result = false;
    char *full_path = NULL;

    char *link_full_path = NULL;


    full_path = concat_strings(base_path, entry_name);

    struct stat info, link_info;
    if (lstat(full_path, &info) != 0) {
        fprintf(stderr, "Error stat'ing %s : %s\n", full_path, strerror(errno));
        goto DEFER;
    }

    if (stat(full_path, &link_info) != 0) {
        fprintf(stderr, "Error stat'ing %s : %s\n", full_path, strerror(errno));
        goto DEFER;
    }

    struct passwd *pwd_file = NULL;
    pwd_file = getpwuid(info.st_uid);
    // if (!pwd_file) {
    //     fprintf(stderr, "Error getting uid of %s : %s\n", full_path, strerror(errno));
    //     goto DEFER;
    // }

    struct passwd *grp_file = NULL;
    grp_file = getpwuid(info.st_gid);
    // if (!grp_file) {
    //     fprintf(stderr, "Error getting gid of %s : %s\n", full_path, strerror(errno));
    //     goto DEFER;
    // }

    char *time_str = ctime(&info.st_mtim.tv_sec);
    if (!time_str) {
        fprintf(stderr, "Error getting time of %s : %s\n", full_path, strerror(errno));
        goto DEFER;
    }
    char cut_time[13];
    memcpy(cut_time, time_str+4, 12);
    cut_time[12] = '\0';

    bool name_has_spaces = (bool) strchr(entry_name, ' ');

    EntryType type = ENTRY_UNDEFINED;

    int color_code = 39;
    bool bold_text = false;

    int link_color_code = 39;
    bool link_bold_text = false;

    if (S_ISREG(info.st_mode)) {
        type = ENTRY_FILE;
        if ( USER_EXECUTE(info.st_mode) ) {
            bold_text = true;
            color_code = 32;
        }
    } else
    if (S_ISDIR(info.st_mode)) {
        type = ENTRY_DIR;
        color_code = 34;
        bold_text = true;
    } else
    if (S_ISLNK(info.st_mode)) {
        type = ENTRY_LINK;
        color_code = 36;
        bold_text = true;

        link_full_path = get_link_target(full_path);
    } else
    if (S_ISCHR(info.st_mode)) {
        type = ENTRY_CHAR;
        color_code = 33;
        bold_text = true;
    } else
    if (S_ISBLK(info.st_mode)) {
        type = ENTRY_BLOCK;
        color_code = 33;
        bold_text = true;
    } else
    if (S_ISFIFO(info.st_mode)) {
        color_code = 33;
        type = ENTRY_PIPE;
    } else
    if (S_ISSOCK(info.st_mode)) {
        type = ENTRY_SOCK;
        color_code = 35;
        bold_text = true;
    }

    if (link_full_path) {
        if (S_ISREG(link_info.st_mode)) {
            if ( USER_EXECUTE(link_info.st_mode) ) {
                link_bold_text = true;
                link_color_code = 32;
            }
        } else
        if (S_ISDIR(link_info.st_mode)) {
            link_color_code = 34;
            link_bold_text = true;
        } else
        if (S_ISLNK(link_info.st_mode)) {
            link_color_code = 36;
            link_bold_text = true;

            link_full_path = get_link_target(full_path);
        } else
        if (S_ISCHR(link_info.st_mode)) {
            link_color_code = 33;
            link_bold_text = true;
        } else
        if (S_ISBLK(link_info.st_mode)) {
            link_color_code = 33;
            link_bold_text = true;
        } else
        if (S_ISFIFO(link_info.st_mode)) {
            link_color_code = 33;
        } else
        if (S_ISSOCK(link_info.st_mode)) {
            link_color_code = 35;
            link_bold_text = true;
        }
    }


    if (flags & FLAG_LONG) {
        putc(EntryCodes[type], stdout);

        // User permissions
        putc( USER_READ(info.st_mode) ? 'r' : '-' , stdout );
        putc( USER_WRITE(info.st_mode) ? 'w' : '-' , stdout );
        putc( USER_EXECUTE(info.st_mode) ? 'x' : '-' , stdout );

        // Group permissions
        putc( GROUP_READ(info.st_mode) ? 'r' : '-' , stdout );
        putc( GROUP_WRITE(info.st_mode) ? 'w' : '-' , stdout );
        putc( GROUP_EXECUTE(info.st_mode) ? 'x' : '-' , stdout );

        // Others permissions
        putc( OTHER_READ(info.st_mode) ? 'r' : '-' , stdout );
        putc( OTHER_WRITE(info.st_mode) ? 'w' : '-' , stdout );
        putc( OTHER_EXECUTE(info.st_mode) ? 'x' : '-' , stdout );

        putc( ' ' , stdout );

        printf("%3lu ", info.st_nlink);

        if (pwd_file && pwd_file->pw_name) printf("%5s ", pwd_file->pw_name);
        else printf("%5d ", info.st_uid);

        if (grp_file && grp_file->pw_name) printf("%5s ", grp_file->pw_name);
        else printf("%5d ", info.st_uid);

        printf("%6lu", info.st_size);

        putc( ' ' , stdout );

        printf("%s", cut_time);

        putc( ' ' , stdout );

        if (bold_text) set_color_bold(color_code);
        else set_color(color_code);

        if (name_has_spaces) printf("`%s`", entry_name);
        else printf("%s", entry_name);

        reset_color();

        if (link_full_path) {
            printf (" -> ");

            if (link_bold_text) set_color_bold(link_color_code);
            else set_color(link_color_code);

            bool link_target_has_spaces = (bool) strchr(link_full_path, ' ');

            if (link_target_has_spaces) printf("`%s`", link_full_path);
            else printf("%s", link_full_path);

            reset_color();
        }

        putc( '\n' , stdout );

    } else {
        if (bold_text) set_color_bold(color_code);
        else set_color(color_code);

        if (name_has_spaces) printf("`%s`", entry_name);
        else printf("%s", entry_name);

        reset_color();

        putc( ' ' , stdout );
    }


    result = true;
    DEFER:
    if (full_path) free(full_path);
    if (link_full_path) free(link_full_path);

    return result;
}

int compare_strings(const void *p1, const void *p2) {
    return strcmp( *(const char**) p1, *(const char**) p2);
}

size_t count_total(const char* dirpath) {
    DIR* dir = NULL;
    struct dirent* entry;
    size_t total = 0;
    dir = opendir(dirpath);
    if (!dir) {
        return 0;
    }

    for (entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
        const char* filename = entry->d_name;
        char *full_path = concat_strings(dirpath, filename);
        if (!full_path) continue;

        struct stat info;
        int result = lstat(full_path, &info);
        if ( result == 0 ) {
            size_t blocks = info.st_blocks; // 512 B block
            total += blocks/2 + blocks%2; // 1KB block
        }

        if (full_path) free(full_path);
    }

    if (dir) closedir(dir);

    return total;
}

bool list_directory(const char* path, int flags) {
    bool result = false;
    DIR* dir = NULL;
    char **entry_names = NULL;

    size_t entry_count = 0;

    size_t total = count_total(path);

    struct dirent* entry;

    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Cannot open directory %s\n", path);
        goto DEFER;
    }

    for (entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
        if (entry->d_name[0]=='.' && !(flags & FLAG_ALL)) continue;
        entry_count++;
    }

    rewinddir(dir);

    if (entry_count == 0) {
        printf("Directory is empty\n");
        result = true;
        goto DEFER;
    }

    entry_names = malloc(entry_count * sizeof(char*));
    if (!entry_names) {
        fprintf(stderr, "Cannot allocate %ld bytes\n", entry_count * sizeof(char*));
        goto DEFER;
    }

    size_t i=0;
    for (entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
        if (entry->d_name[0]=='.' && !(flags & FLAG_ALL)) continue;
        entry_names[i] = strdup(entry->d_name);
        i++;
    }

    qsort(entry_names, entry_count, sizeof(char*), compare_strings);

    if (flags & FLAG_LONG) printf("Total %lu\n", total);
    for (i=0; i<entry_count; i++) {
        if (entry_names[i][0]=='.' && !(flags & FLAG_ALL)) continue;

        if (!print_entry(path, entry_names[i], flags)) {
            goto DEFER;
        }
    }

    if (!(flags & FLAG_LONG)) putc('\n', stdout);

    result = true;
    DEFER:
    if (dir) closedir(dir);
    if (entry_names) {
        for (size_t i=0; i<entry_count; i++) {
            free(entry_names[i]);
        }
        free(entry_names);
    }
    return result;
}