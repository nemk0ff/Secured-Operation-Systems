#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#include <stdbool.h>
#include <stdint.h>

void show_help(FILE* output);

typedef enum {
    OP_NONE,
    OP_ASSIGN,
    OP_GRANT,
    OP_REVOKE
} OperationKind;

typedef struct {
    int8_t rights;
    OperationKind op;
} PermissionOp;

typedef struct {
    PermissionOp owner;
    PermissionOp group;
    PermissionOp everyone;
} PermissionSet;

bool interpret_permission_string(const char* perm_str, PermissionSet* result);
bool modify_file_permissions(const char* filename, PermissionSet* new_perms);
bool retrieve_current_permissions(const char* filename, PermissionSet* current);

uint8_t combine_rights(uint8_t existing, uint8_t additional);
uint8_t remove_rights(uint8_t existing, uint8_t to_remove);
mode_t build_mode_bits(uint8_t owner_bits, uint8_t group_bits, uint8_t other_bits);

int main(int arg_count, char** arg_values) {
    if (arg_count < 3) {
        fprintf(stderr, "Insufficient arguments provided\n");
        show_help(stderr);
        exit(1);
    }

    const char* permission_spec = arg_values[1];
    const char* target_file = arg_values[2];

    PermissionSet permissions = {0};

    if (!interpret_permission_string(permission_spec, &permissions)) {
        exit(1);
    }

    if (!modify_file_permissions(target_file, &permissions)) {
        exit(1);
    }

    return 0;
}

void show_help(FILE* output) {
    fprintf(output, "Command syntax:\n");
    fprintf(output, "\tchmod PERMISSIONS FILE\n");
}

enum PermissionScope {
    SCOPE_NONE = 0,
    SCOPE_OWNER = 1,
    SCOPE_GROUP = 2,
    SCOPE_ALL = 4
};

bool interpret_permission_string(const char* input_str, PermissionSet* output) {
    bool octal_format = true;
    size_t input_len = strlen(input_str);

    if (input_len <= 3) {
        for (size_t pos = 0; pos < input_len; pos++) {
            if (input_str[pos] < '0' || input_str[pos] > '7') octal_format = false;
        }
    } else octal_format = false;

    if (octal_format) {
        output->everyone.rights = input_str[0] - '0';
        output->everyone.op = OP_ASSIGN;

        if (input_len > 1) {
            output->group.rights = input_str[1] - '0';
            output->group.op = OP_ASSIGN;
        }

        if (input_len > 2) {
            output->owner.rights = input_str[2] - '0';
            output->owner.op = OP_ASSIGN;
        }

        return true;
    }

    enum PermissionScope scope = SCOPE_NONE;
    bool reading_scope = true;
    OperationKind current_op = OP_NONE;

    uint8_t permission_bits = 0;

#define RIGHT_READ 0b00000100
#define RIGHT_WRITE 0b00000010
#define RIGHT_EXECUTE 0b00000001

    for (size_t idx = 0; idx < input_len; idx++) {
        char current_char = input_str[idx];

        if (reading_scope) {
            if (current_char == 'u') {
                scope = scope | SCOPE_OWNER;
                continue;
            }
            if (current_char == 'g') {
                scope = scope | SCOPE_GROUP;
                continue;
            }
            if (current_char == 'o') {
                scope = scope | SCOPE_ALL;
                continue;
            }

            if (current_char == '+' || current_char == '-') {
                current_op = current_char == '+' ? OP_GRANT : OP_REVOKE;
                reading_scope = false;
                if (scope == SCOPE_NONE) scope = SCOPE_OWNER | SCOPE_GROUP | SCOPE_ALL;
                continue;
            }

            fprintf(stderr, "Unrecognized character '%c' in permission specification\n", current_char);
            return false;
        }

        if (current_char == 'r') permission_bits = permission_bits | RIGHT_READ;
        else if (current_char == 'w') permission_bits = permission_bits | RIGHT_WRITE;
        else if (current_char == 'x') permission_bits = permission_bits | RIGHT_EXECUTE;
        else {
            fprintf(stderr, "Unrecognized permission character '%c'\n", current_char);
            return false;
        }
    }

    if (scope & SCOPE_OWNER) {
        output->owner.rights = permission_bits;
        output->owner.op = current_op;
    } else output->owner.op = OP_NONE;

    if (scope & SCOPE_GROUP) {
        output->group.rights = permission_bits;
        output->group.op = current_op;
    } else output->group.op = OP_NONE;

    if (scope & SCOPE_ALL) {
        output->everyone.rights = permission_bits;
        output->everyone.op = current_op;
    } else output->everyone.op = OP_NONE;

    return true;
}

bool modify_file_permissions(const char* file_path, PermissionSet* new_perms) {
    PermissionSet existing_perms = {0};
    if (!retrieve_current_permissions(file_path, &existing_perms)) return false;

    switch (new_perms->owner.op) {
        case OP_NONE:
            new_perms->owner.rights = existing_perms.owner.rights;
            break;
        case OP_GRANT:
            new_perms->owner.rights = combine_rights(existing_perms.owner.rights, new_perms->owner.rights);
            break;
        case OP_REVOKE:
            new_perms->owner.rights = remove_rights(existing_perms.owner.rights, new_perms->owner.rights);
            break;
        default:
            break;
    }

    switch (new_perms->group.op) {
        case OP_NONE:
            new_perms->group.rights = existing_perms.group.rights;
            break;
        case OP_GRANT:
            new_perms->group.rights = combine_rights(existing_perms.group.rights, new_perms->group.rights);
            break;
        case OP_REVOKE:
            new_perms->group.rights = remove_rights(existing_perms.group.rights, new_perms->group.rights);
            break;
        default:
            break;
    }

    switch (new_perms->everyone.op) {
        case OP_NONE:
            new_perms->everyone.rights = existing_perms.everyone.rights;
            break;
        case OP_GRANT:
            new_perms->everyone.rights = combine_rights(existing_perms.everyone.rights, new_perms->everyone.rights);
            break;
        case OP_REVOKE:
            new_perms->everyone.rights = remove_rights(existing_perms.everyone.rights, new_perms->everyone.rights);
            break;
        default:
            break;
    }

    mode_t final_permissions = build_mode_bits(new_perms->owner.rights, new_perms->group.rights, new_perms->everyone.rights);
    if (chmod(file_path, final_permissions) == -1) {
        fprintf(stderr, "Failed to change permissions: %s\n", strerror(errno));
        return false;
    }

    return true;
}

bool retrieve_current_permissions(const char* file_path, PermissionSet* current) {
    struct stat file_info;
    uint8_t permission_mask = 0b111;

    if (lstat(file_path, &file_info) != 0) {
        fprintf(stderr, "Cannot access file %s: %s\n", file_path, strerror(errno));
        return false;
    }
    mode_t file_perms = file_info.st_mode;

    current->everyone.rights = file_perms & permission_mask;
    file_perms = file_perms >> 3;
    current->group.rights = file_perms & permission_mask;
    file_perms = file_perms >> 3;
    current->owner.rights = file_perms & permission_mask;

    return true;
}

uint8_t combine_rights(uint8_t current, uint8_t additional) {
    return current | additional;
}

uint8_t remove_rights(uint8_t current, uint8_t to_remove) {
    return current & (~to_remove);
}

mode_t build_mode_bits(uint8_t owner, uint8_t group, uint8_t others) {
    mode_t result = ((owner << 3) << 3) | (group << 3) | others;
    return result;
}