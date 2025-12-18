#define _GNU_SOURCE  // for getline()
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>  // for getopt_long()
#include <unistd.h>
#include <regex.h>
#include <ctype.h>

#define LINE_NUM_FLAG 0x01
#define NONBLANK_NUM_FLAG 0x02
#define END_MARKER_FLAG 0x04

typedef struct {
    int options;
    int exit_code;
} AppState;

// ==================== TEXT PROCESSOR (CAT-LIKE) ====================
void display_text_line(char *text_buffer, size_t length, int options_set, int *counter) {
    int line_is_empty = (text_buffer[0] == '\n' || length == 0);

    // Handle line numbering
    if ((options_set & NONBLANK_NUM_FLAG) && !line_is_empty) {
        printf("%6d\t", (*counter)++);
    } else if ((options_set & LINE_NUM_FLAG) && !(options_set & NONBLANK_NUM_FLAG)) {
        printf("%6d\t", (*counter)++);
    }

    // Handle end-of-line markers
    if (options_set & END_MARKER_FLAG) {
        if (length > 0 && text_buffer[length - 1] == '\n') {
            text_buffer[length - 1] = '\0';
            printf("%s$\n", text_buffer);
        } else {
            printf("%s$\n", text_buffer);
        }
    } else {
        printf("%s", text_buffer);
    }
}

void stream_text_display(FILE *input_stream, int options_set) {
    char *buffer_ptr = NULL;
    size_t buffer_size = 0;
    ssize_t bytes_read;
    int current_line = 1;

    while ((bytes_read = getline(&buffer_ptr, &buffer_size, input_stream)) != -1) {
        display_text_line(buffer_ptr, bytes_read, options_set, &current_line);
    }

    if (buffer_ptr != NULL) free(buffer_ptr);
}

int process_text_file(const char *filename, int options_set) {
    FILE *input_file = fopen(filename, "r");
    if (input_file == NULL) {
        fprintf(stderr, "Error opening '%s': ", filename);
        perror("");
        return 1;
    }

    stream_text_display(input_file, options_set);
    fclose(input_file);
    return 0;
}

void process_text_input(int options_set) {
    stream_text_display(stdin, options_set);
}

int text_processor_main(int arg_count, char *arg_values[]) {
    int current_option;
    int selected_options = 0;
    AppState app_state = {0, 0};

    struct option long_opts[] = {
            {"number", 0, NULL, 'n'},
            {"number-nonblank", 0, NULL, 'b'},
            {"show-ends", 0, NULL, 'E'},
            {NULL, 0, NULL, 0}
    };

    // Reset option index for reusability
    optind = 1;

    while ((current_option = getopt_long(arg_count, arg_values, "nbE", long_opts, NULL)) != -1) {
        switch (current_option) {
            case 'n':
                selected_options |= LINE_NUM_FLAG;
                break;
            case 'b':
                selected_options |= NONBLANK_NUM_FLAG;
                break;
            case 'E':
                selected_options |= END_MARKER_FLAG;
                break;
            default:
                fprintf(stderr, "Usage: %s [-n] [-b] [-E] [file...]\n", arg_values[0]);
                return 1;
        }
    }

    if (optind == arg_count) {
        process_text_input(selected_options);
    } else {
        for (int idx = optind; idx < arg_count; idx++) {
            app_state.exit_code |= process_text_file(arg_values[idx], selected_options);
        }
    }

    return app_state.exit_code;
}

// ==================== PATTERN SEARCHER (GREP-LIKE) ====================
int search_stream_pattern(regex_t *compiled_regex, FILE *input_stream,
                          const char *source_name, int multi_source) {
    char *line_buffer = NULL;
    size_t buffer_capacity = 0;
    int found_status = 0;

    while (getline(&line_buffer, &buffer_capacity, input_stream) != -1) {
        if (regexec(compiled_regex, line_buffer, 0, NULL, 0) == 0) {
            if (multi_source && source_name != NULL) {
                printf("%s:%s", source_name, line_buffer);
            } else {
                printf("%s", line_buffer);
            }
            found_status = 1;
        }
    }

    if (line_buffer != NULL) free(line_buffer);
    return found_status ? 0 : 1;
}

int search_file_pattern(const char *search_pattern, const char *filename, int multi_source) {
    FILE *input_file = fopen(filename, "r");
    if (input_file == NULL) {
        fprintf(stderr, "Cannot open '%s': ", filename);
        perror("");
        return 1;
    }

    regex_t pattern_regex;
    int compile_result = regcomp(&pattern_regex, search_pattern, REG_EXTENDED);

    if (compile_result != 0) {
        fprintf(stderr, "Pattern compilation failed\n");
        fclose(input_file);
        return 1;
    }

    int search_result = search_stream_pattern(&pattern_regex, input_file, filename, multi_source);

    regfree(&pattern_regex);
    fclose(input_file);

    return search_result;
}

void search_input_pattern(const char *search_pattern) {
    regex_t pattern_regex;

    if (regcomp(&pattern_regex, search_pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Pattern compilation failed\n");
        return;
    }

    search_stream_pattern(&pattern_regex, stdin, NULL, 0);
    regfree(&pattern_regex);
}

int pattern_searcher_main(int arg_count, char *arg_values[]) {
    if (arg_count < 2) {
        fprintf(stderr, "Usage: %s search_pattern [file...]\n", arg_values[0]);
        return 1;
    }

    const char *target_pattern = arg_values[1];
    int overall_status = 0;
    int multiple_sources = (arg_count > 3);

    if (arg_count == 2) {
        search_input_pattern(target_pattern);
    } else {
        for (int idx = 2; idx < arg_count; idx++) {
            overall_status |= search_file_pattern(target_pattern, arg_values[idx], multiple_sources);
        }
    }

    return overall_status;
}

// ==================== MAIN DISPATCHER ====================
int main(int arg_count, char *arg_values[]) {
    char *exec_name = arg_values[0];

    // Case-insensitive check for program name
    char *lower_name = strdup(exec_name);
    for (int i = 0; lower_name[i]; i++) {
        lower_name[i] = tolower(lower_name[i]);
    }

    int result = 1;

    if (strstr(lower_name, "mycat") != NULL) {
        result = text_processor_main(arg_count, arg_values);
    } else if (strstr(lower_name, "mygrep") != NULL) {
        result = pattern_searcher_main(arg_count, arg_values);
    } else {
        fprintf(stderr, "Program must be executed as 'mycat' or 'mygrep'\n");
    }

    free(lower_name);
    return result;
}