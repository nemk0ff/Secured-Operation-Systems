#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

int parse_octal_mode(const char *mode_str) {
    int mode = 0;
    for (int i = 0; mode_str[i]; i++) {
        if (!isdigit(mode_str[i]) || mode_str[i] > '7') {
            return -1;
        }
        mode = mode * 8 + (mode_str[i] - '0');
    }
    return mode;
}

int parse_symbolic_mode(const char *mode_str, mode_t current_mode, mode_t *new_mode) {
    const char *p = mode_str;
    int who_mask = 0;
    char op = 0;
    int perm_bits = 0;

    while (*p && strchr("ugoa", *p)) {
        if (*p == 'u') who_mask |= 04700;
        else if (*p == 'g') who_mask |= 02070;
        else if (*p == 'o') who_mask |= 01007;
        else if (*p == 'a') who_mask = 07777;
        p++;
    }

    if (who_mask == 0) who_mask = 0777;

    if (*p != '+' && *p != '-' && *p != '=') {
        return -1;
    }
    op = *p++;

    while (*p && strchr("rwx", *p)) {
        if (*p == 'r') {
            if (who_mask & 04700) perm_bits |= S_IRUSR;
            if (who_mask & 02070) perm_bits |= S_IRGRP;
            if (who_mask & 01007) perm_bits |= S_IROTH;
        } else if (*p == 'w') {
            if (who_mask & 04700) perm_bits |= S_IWUSR;
            if (who_mask & 02070) perm_bits |= S_IWGRP;
            if (who_mask & 01007) perm_bits |= S_IWOTH;
        } else if (*p == 'x') {
            if (who_mask & 04700) perm_bits |= S_IXUSR;
            if (who_mask & 02070) perm_bits |= S_IXGRP;
            if (who_mask & 01007) perm_bits |= S_IXOTH;
        }
        p++;
    }

    if (*p != '\0') {
        return -1;
    }

    if (op == '+') {
        *new_mode = current_mode | perm_bits;
    } else if (op == '-') {
        *new_mode = current_mode & ~perm_bits;
    } else if (op == '=') {
        int clear_mask = 0;
        if (who_mask & 04700) clear_mask |= 0700;
        if (who_mask & 02070) clear_mask |= 0070;
        if (who_mask & 01007) clear_mask |= 0007;
        *new_mode = (current_mode & ~clear_mask) | perm_bits;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s MODE FILE\n", argv[0]);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s +x file.txt\n", argv[0]);
        fprintf(stderr, "  %s u-r file.txt\n", argv[0]);
        fprintf(stderr, "  %s ug+rw file.txt\n", argv[0]);
        fprintf(stderr, "  %s 755 file.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *mode_str = argv[1];
    const char *filepath = argv[2];

    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat");
        return EXIT_FAILURE;
    }

    mode_t new_mode;

    if (isdigit(mode_str[0])) {
        int octal = parse_octal_mode(mode_str);
        if (octal < 0 || octal > 07777) {
            fprintf(stderr, "Invalid octal mode: %s\n", mode_str);
            return EXIT_FAILURE;
        }
        new_mode = octal;
    } else {
        if (parse_symbolic_mode(mode_str, st.st_mode, &new_mode) != 0) {
            fprintf(stderr, "Invalid symbolic mode: %s\n", mode_str);
            return EXIT_FAILURE;
        }
    }

    if (chmod(filepath, new_mode) == -1) {
        perror("chmod");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}