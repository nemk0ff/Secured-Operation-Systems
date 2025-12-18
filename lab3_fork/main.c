#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

// Termination status indicators
#define SUCCESS_TERMINATION 0
#define FAILURE_TERMINATION 1

// Signal identifiers
#define SIGNAL_INTERRUPT 2
#define SIGNAL_TERMINATE 15

// Cleanup function declaration
void cleanup_routine(void);

// Signal handler prototypes
void handle_interrupt_signal(int signal_num);
void handle_termination_signal(int signal_num);

// Process monitoring function
void monitor_child_process();

// Main execution routine
int execute_process_hierarchy(void);

int main(void) {
    return execute_process_hierarchy();
}

// Resource cleanup handler
void cleanup_routine(void) {
    fprintf(stdout, "Cleanup performed for process ID: %d\n",
            (int)getpid());
}

// Signal handler for interrupt (Ctrl+C)
void handle_interrupt_signal(int signal_num) {
    fprintf(stdout, "Process %d: Signal %d received. Initiating shutdown.\n",
            (int)getpid(), signal_num);
    exit(SUCCESS_TERMINATION);
}

// Signal handler for termination request
void handle_termination_signal(int signal_num) {
    fprintf(stdout, "Process %d: Termination signal %d received.\n",
            (int)getpid(), signal_num);
    exit(SUCCESS_TERMINATION);
}

// Monitor child process completion
void monitor_child_process() {
    int completion_status;

    // Wait for child process to complete
    pid_t wait_result = wait(&completion_status);

    if (wait_result == -1) {
        perror("Process monitoring failed");
        exit(FAILURE_TERMINATION);
    }

    // Analyze termination reason
    if (WIFEXITED(completion_status)) {
        fprintf(stdout, "Parent: Child process terminated normally with code: %d\n",
                WEXITSTATUS(completion_status));
    }
    else if (WIFSIGNALED(completion_status)) {
        fprintf(stdout, "Parent: Child terminated by signal: %d\n",
                WTERMSIG(completion_status));
    }
    else if (WIFSTOPPED(completion_status)) {
        fprintf(stdout, "Parent: Child process stopped by signal: %d\n",
                WSTOPSIG(completion_status));
    }
    else {
        fprintf(stdout, "Parent: Child process status undetermined\n");
    }
}

// Main execution routine
int execute_process_hierarchy(void) {
    // Register cleanup handler
    if (atexit(cleanup_routine) != 0) {
        perror("Cleanup registration error");
        return FAILURE_TERMINATION;
    }

    // Setup interrupt signal handler
    if (signal(SIGINT, handle_interrupt_signal) == SIG_ERR) {
        perror("Interrupt signal handler registration failed");
        return FAILURE_TERMINATION;
    }

    // Setup termination signal handler using sigaction
    struct sigaction termination_action;
    memset(&termination_action, 0, sizeof(termination_action));
    termination_action.sa_handler = handle_termination_signal;
    sigemptyset(&termination_action.sa_mask);
    termination_action.sa_flags = 0;

    if (sigaction(SIGTERM, &termination_action, NULL) == -1) {
        perror("Termination signal handler setup failed");
        return FAILURE_TERMINATION;
    }

    // Display process information
    fprintf(stdout, "Main process initialized. PID: %d, Parent PID: %d\n",
            (int)getpid(), (int)getppid());

    // Create child process
    pid_t child_id = fork();

    if (child_id < 0) {
        perror("Process creation failed");
        return FAILURE_TERMINATION;
    }

    if (child_id == 0) {
        // Child process execution block
        fprintf(stdout, "Child process active. PID: %d, Parent PID: %d\n",
                (int)getpid(), (int)getppid());

        // Simulate work
        sleep(3);

        fprintf(stdout, "Child process execution complete\n");

        // Exit with specific code
        exit(42);
    } else {
        // Parent process execution block
        fprintf(stdout, "Parent: Created child process with ID: %d\n",
                (int)child_id);

        // Monitor child process
        monitor_child_process(child_id);

        fprintf(stdout, "Parent process execution complete\n");
    }

    return SUCCESS_TERMINATION;
}