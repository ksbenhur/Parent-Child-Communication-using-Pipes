#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#define NUM_CHILDREN 5


// Function to format the timestamp
void format_timestamp(char *buffer, struct timeval *start_time) {
    struct timeval current_time, elapsed_time;
    gettimeofday(&current_time, NULL);

    // Calculate elapsed time
    timersub(&current_time, start_time, &elapsed_time);

    long total_msec = elapsed_time.tv_sec * 1000 + elapsed_time.tv_usec / 1000;

    int minutes = total_msec / (60 * 1000);
    int remaining_msec = total_msec % (60 * 1000);
    int secs = remaining_msec / 1000;
    int msecs = remaining_msec % 1000;

    sprintf(buffer, "%d:%02d.%03d", minutes, secs, msecs);
}

const char* get_ordinal_suffix(int num) {
    int tens = num % 100;
    if (tens >= 11 && tens <= 13) {
        return "th";
    }
    switch (num % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

int main() {
    int pipes[NUM_CHILDREN][2];
    pid_t pids[NUM_CHILDREN];
    struct timeval start_time;

    // Get the start time
    gettimeofday(&start_time, NULL);

    // Create pipes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Fork child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pids[i] == 0) {
            // Child process
            close(pipes[i][0]); // Close read end

            if (i < 4) {
                // Children 1-4
                srand(time(NULL) ^ (getpid()<<16));
                int message_count = 1;

                struct timeval current_time, end_time;
                gettimeofday(&current_time, NULL);
                struct timeval duration = {30, 0}; // 30 seconds
                timeradd(&current_time, &duration, &end_time); // Set end time

                while (1) {
                    gettimeofday(&current_time, NULL);

                    // Calculate remaining time
                    struct timeval remaining_time;
                    timersub(&end_time, &current_time, &remaining_time);

                    // Check if time limit has been reached
                    struct timeval zero_time = {0, 0};
                    if (timercmp(&remaining_time, &zero_time, <=)) {
                        break;
                    }

                    // Sleep for a random time between 0 to 2 seconds
                    int sleep_time = rand() % 3; // Can be 0, 1, or 2

                    // Adjust sleep_time if it exceeds remaining time
                    int remaining_secs = remaining_time.tv_sec;
                    if (sleep_time > remaining_secs) {
                        sleep_time = remaining_secs;
                    }

                    if (sleep_time > 0) {
                        sleep(sleep_time);
                    } else {
                        // If sleep_time is zero, introduce a small delay to prevent tight looping
                        usleep(100000); // Sleep for 100 milliseconds
                    }

                    // Check time again after sleep or delay
                    gettimeofday(&current_time, NULL);
                    timersub(&end_time, &current_time, &remaining_time);
                    if (timercmp(&remaining_time, &zero_time, <=)) {
                        break;
                    }

                    char timestamp[16];
                    format_timestamp(timestamp, &start_time);

                    char message[256];
                    sprintf(message, "%s: Child %d message %d\n", timestamp, i + 1, message_count);

                    ssize_t bytes_written = write(pipes[i][1], message, strlen(message));
                    if (bytes_written == -1) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    message_count++;
                }
            } else {
                // Child 5
                struct timeval current_time, end_time;
                gettimeofday(&current_time, NULL);
                struct timeval duration = {30, 0}; // 30 seconds
                timeradd(&current_time, &duration, &end_time); // Set end time

                int message_count = 1;

                while (1) {
                    gettimeofday(&current_time, NULL);

                    // Calculate remaining time
                    struct timeval remaining_time;
                    timersub(&end_time, &current_time, &remaining_time);

                    // Check if time limit has been reached
                    struct timeval zero_time = {0, 0};
                    if (timercmp(&remaining_time, &zero_time, <=)) {
                        break;
                    }

                    printf("Enter a message: ");
                    fflush(stdout);

                    // Set up select() to wait for input on stdin with a timeout
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);

                    // select() modifies the timeout, so we need to create a copy
                    struct timeval timeout = remaining_time;

                    int select_result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
                    if (select_result > 0) {
                        // Input is ready
                        char input[256];
                        if (fgets(input, sizeof(input), stdin) != NULL) {
                            input[strcspn(input, "\n")] = '\0';

                            gettimeofday(&current_time, NULL);
                            timersub(&end_time, &current_time, &remaining_time);
                            if (timercmp(&remaining_time, &zero_time, <=)) {
                                break;
                            }

                            char timestamp[16];
                            format_timestamp(timestamp, &start_time);

                            const char* suffix = get_ordinal_suffix(message_count);

                            char message[512];
                            sprintf(message, "%s: Child 5: %d%s text msg from the terminal: %s\n",
                                    timestamp, message_count, suffix, input);

                            ssize_t bytes_written = write(pipes[i][1], message, strlen(message));
                            if (bytes_written == -1) {
                                perror("write");
                                exit(EXIT_FAILURE);
                            }
                            message_count++;
                        }
                    } else if (select_result == 0) {
                        // Timeout reached, no input received
                        break;
                    } else {
                        // Error
                        if (errno == EINTR) {
                            continue; // Interrupted by signal, retry
                        }
                        perror("select");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            close(pipes[i][1]); // Close write end
            exit(EXIT_SUCCESS);
        }
    }

    // Parent process
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(pipes[i][1]); // Close write end
    }

    FILE *output_file = fopen("output.txt", "w");
    if (!output_file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fd_set read_fds;
    int max_fd = 0;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipes[i][0] > max_fd) {
            max_fd = pipes[i][0];
        }
    }

    char buffers[NUM_CHILDREN][1024];
    int buffer_lengths[NUM_CHILDREN] = {0};
    int children_remaining = NUM_CHILDREN;

    while (children_remaining > 0) {
        FD_ZERO(&read_fds);
        for (int i = 0; i < NUM_CHILDREN; i++) {
            if (pipes[i][0] != -1) {
                FD_SET(pipes[i][0], &read_fds);
            }
        }

        int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("select");
            break;
        }

        for (int i = 0; i < NUM_CHILDREN; i++) {
            if (pipes[i][0] != -1 && FD_ISSET(pipes[i][0], &read_fds)) {
                char read_buffer[256];
                int bytes_read = read(pipes[i][0], read_buffer, sizeof(read_buffer));
                if (bytes_read > 0) {
                    memcpy(buffers[i] + buffer_lengths[i], read_buffer, bytes_read);
                    buffer_lengths[i] += bytes_read;
                    buffers[i][buffer_lengths[i]] = '\0';

                    char *line_start = buffers[i];
                    char *newline_pos = NULL;

                    while ((newline_pos = strchr(line_start, '\n')) != NULL) {
                        *newline_pos = '\0';

                        char parent_timestamp[16];
                        format_timestamp(parent_timestamp, &start_time);

                        fprintf(output_file, "%s: %s\n", parent_timestamp, line_start);
                        fflush(output_file);

                        line_start = newline_pos + 1;
                    }

                    int remaining_length = buffer_lengths[i] - (line_start - buffers[i]);
                    memmove(buffers[i], line_start, remaining_length);
                    buffer_lengths[i] = remaining_length;
                } else if (bytes_read == 0) {
                    close(pipes[i][0]);
                    pipes[i][0] = -1;
                    children_remaining--;
                } else {
                    perror("read");
                }
            }
        }
    }

    // Wait for child processes to terminate
    for (int i = 0; i < NUM_CHILDREN; i++) {
        waitpid(pids[i], NULL, 0);
    }

    fclose(output_file);
    return 0;
}

