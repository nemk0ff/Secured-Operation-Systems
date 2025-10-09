#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

void display_error_and_exit(const char *error_message) {
    perror(error_message);
    exit(EXIT_FAILURE);
}

int process_octal_permissions(const char *octal_mode_string, const char *target_filename) {
    char *validation_pointer;
    long calculated_mode = strtol(octal_mode_string, &validation_pointer, 8);

    if (*validation_pointer != '\0' || calculated_mode < 0 || calculated_mode > 07777) {
        fprintf(stderr, "mychmod: invalid octal mode specification '%s'\n", octal_mode_string);
        return 1;
    }

    if (chmod(target_filename, (mode_t)calculated_mode) == -1) {
        display_error_and_exit(target_filename);
    }
    return 0;
}

int process_symbolic_permissions(const char *symbolic_mode_string, const char *target_filename) {
    struct stat file_statistics;
    if (stat(target_filename, &file_statistics) == -1) {
        display_error_and_exit(target_filename);
    }

    mode_t existing_permissions = file_statistics.st_mode;
    mode_t updated_permissions = existing_permissions;

    const char *current_position = symbolic_mode_string;

    int permission_categories = 0;
    while (*current_position && strchr("ugoa", *current_position)) {
        switch (*current_position) {
            case 'u': permission_categories |= 1 << 2; break;
            case 'g': permission_categories |= 1 << 1; break;
            case 'o': permission_categories |= 1 << 0; break;
            case 'a': permission_categories |= 0b111; break;
        }
        current_position++;
    }

    if (permission_categories == 0) {
        permission_categories = 0b111;
    }

    if (*current_position != '+' && *current_position != '-' && *current_position != '=') {
        fprintf(stderr, "mychmod: invalid permission operator in '%s'\n", symbolic_mode_string);
        return 1;
    }

    char permission_operator = *current_position++;
    mode_t permission_bits = 0;
    while (*current_position) {
        switch (*current_position) {
            case 'r': permission_bits |= S_IRUSR | S_IRGRP | S_IROTH; break;
            case 'w': permission_bits |= S_IWUSR | S_IWGRP | S_IWOTH; break;
            case 'x': permission_bits |= S_IXUSR | S_IXGRP | S_IXOTH; break;
            default:
                fprintf(stderr, "mychmod: invalid permission character '%c' in '%s'\n", *current_position, symbolic_mode_string);
                return 1;
        }
        current_position++;
    }

    mode_t permission_mask = 0;
    if (permission_categories & (1 << 2)) permission_mask |= S_IRWXU;
    if (permission_categories & (1 << 1)) permission_mask |= S_IRWXG;
    if (permission_categories & (1 << 0)) permission_mask |= S_IRWXO;

    mode_t applicable_bits = 0;
    if (permission_categories & (1 << 2)) applicable_bits |= (permission_bits & S_IRWXU);
    if (permission_categories & (1 << 1)) applicable_bits |= (permission_bits & S_IRWXG);
    if (permission_categories & (1 << 0)) applicable_bits |= (permission_bits & S_IRWXO);

    switch (permission_operator) {
        case '+':
            updated_permissions |= applicable_bits;
            break;
        case '-':
            updated_permissions &= ~applicable_bits;
            break;
        case '=':
            updated_permissions &= ~permission_mask;
            updated_permissions |= applicable_bits;
            break;
    }

    if (chmod(target_filename, updated_permissions) == -1) {
        display_error_and_exit(target_filename);
    }
    return 0;
}

int main(int argument_count, char *argument_values[]) {
    if (argument_count != 3) {
        fprintf(stderr, "Usage: %s <permission_mode> <target_file>\n", argument_values[0]);
        exit(EXIT_FAILURE);
    }

    const char *mode_specification = argument_values[1];
    const char *target_file = argument_values[2];

    int is_octal_format = 1;
    for (int index = 0; mode_specification[index]; index++) {
        if (mode_specification[index] < '0' || mode_specification[index] > '7') {
            is_octal_format = 0;
            break;
        }
    }

    if (is_octal_format) {
        return process_octal_permissions(mode_specification, target_file);
    } else {
        return process_symbolic_permissions(mode_specification, target_file);
    }
}