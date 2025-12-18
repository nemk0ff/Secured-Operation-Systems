#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// Helper for error reporting
void report_system_error(const char *message_context) {
    perror(message_context);
    exit(EXIT_FAILURE);
}

// Handle numeric (octal) permission changes
int apply_numeric_mode(const char *mode_input, const char *target_path) {
    char *end_ptr;
    // Convert string to octal number
    long mode_value = strtol(mode_input, &end_ptr, 8);

    // Validate the octal string
    if (*end_ptr != '\0' || mode_value < 0 || mode_value > 07777) {
        fprintf(stderr, "mode: invalid numeric permission '%s'\n", mode_input);
        return EXIT_FAILURE;
    }

    // Apply the mode change
    if (chmod(target_path, (mode_t)mode_value) == -1) {
        report_system_error(target_path);
    }
    return EXIT_SUCCESS;
}

// Handle symbolic permission changes
int apply_symbolic_mode(const char *mode_spec, const char *target_path) {
    struct stat file_info;
    if (stat(target_path, &file_info) == -1) {
        report_system_error(target_path);
    }

    mode_t current_mode = file_info.st_mode;
    mode_t new_mode = current_mode;

    const char *parse_pos = mode_spec;

    // Parse user/group/other specifiers
    int categories = 0;
    while (*parse_pos && strchr("ugoa", *parse_pos)) {
        switch (*parse_pos) {
            case 'u': categories |= 4; break;  // 100 binary
            case 'g': categories |= 2; break;  // 010 binary
            case 'o': categories |= 1; break;  // 001 binary
            case 'a': categories |= 7; break;  // 111 binary
        }
        parse_pos++;
    }

    // Default to all categories if none specified
    if (categories == 0) {
        categories = 7;
    }

    // Validate operator
    if (*parse_pos != '+' && *parse_pos != '-' && *parse_pos != '=') {
        fprintf(stderr, "mode: invalid operator in '%s'\n", mode_spec);
        return EXIT_FAILURE;
    }

    char operation = *parse_pos++;
    mode_t permission_flags = 0;

    // Parse permission characters
    while (*parse_pos) {
        switch (*parse_pos) {
            case 'r': permission_flags |= S_IRUSR | S_IRGRP | S_IROTH; break;
            case 'w': permission_flags |= S_IWUSR | S_IWGRP | S_IWOTH; break;
            case 'x': permission_flags |= S_IXUSR | S_IXGRP | S_IXOTH; break;
            default:
                fprintf(stderr, "mode: invalid permission '%c' in '%s'\n", *parse_pos, mode_spec);
                return EXIT_FAILURE;
        }
        parse_pos++;
    }

    // Calculate mask for selected categories
    mode_t category_mask = 0;
    if (categories & 4) category_mask |= S_IRWXU;  // User
    if (categories & 2) category_mask |= S_IRWXG;  // Group
    if (categories & 1) category_mask |= S_IRWXO;  // Others

    // Apply permissions to selected categories only
    mode_t effective_bits = 0;
    if (categories & 4) effective_bits |= (permission_flags & S_IRWXU);
    if (categories & 2) effective_bits |= (permission_flags & S_IRWXG);
    if (categories & 1) effective_bits |= (permission_flags & S_IRWXO);

    // Apply the operation
    switch (operation) {
        case '+':
            new_mode |= effective_bits;
            break;
        case '-':
            new_mode &= ~effective_bits;
            break;
        case '=':
            new_mode &= ~category_mask;
            new_mode |= effective_bits;
            break;
    }

    // Apply the new mode
    if (chmod(target_path, new_mode) == -1) {
        report_system_error(target_path);
    }
    return EXIT_SUCCESS;
}

// Main program entry point
int main(int argc, char *argv[]) {
    // Check argument count
    if (argc != 3) {
        fprintf(stderr, "Permission Manager\n");
        fprintf(stderr, "Usage: %s <mode> <file>\n", argv[0]);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s 755 script.sh\n", argv[0]);
        fprintf(stderr, "  %s u+x document.txt\n", argv[0]);
        fprintf(stderr, "  %s go-rw config.cfg\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *mode_string = argv[1];
    const char *target_file = argv[2];

    // Determine if input is numeric or symbolic
    int is_numeric = 1;
    for (int i = 0; mode_string[i]; i++) {
        if (mode_string[i] < '0' || mode_string[i] > '7') {
            is_numeric = 0;
            break;
        }
    }

    if (is_numeric) {
        return apply_numeric_mode(mode_string, target_file);
    } else {
        return apply_symbolic_mode(mode_string, target_file);
    }
}