#include "ipc.h"
#include "text.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static int parse_number(const char *s) {
    char *end;
    errno = 0;
    long value = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || value <= 0 || value > INT_MAX) {
        fprintf(stderr, "bad child argument: %s\n", s);
        return -1;
    }
    return (int)value;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "child is started by parent\n");
        return EXIT_FAILURE;
    }
    int fd = parse_number(argv[1]);
    int parent = parse_number(argv[2]);
    int number = parse_number(argv[3]);
    if (fd == -1 || parent == -1 || number == -1) {
        return EXIT_FAILURE;
    }
    fprintf(stderr, "child%d: PID=%d, parent=%d\n", number, getpid(), parent);
    int result = EXIT_SUCCESS;
    for (;;) {
        if (wait_signal(SIGUSR1, (pid_t)parent, 0) == -1) {
            result = EXIT_FAILURE;
            break;
        }
        size_t size;
        Message *msg = get_message(fd, &size);
        if (msg == NULL) {
            result = EXIT_FAILURE;
            break;
        }
        int stop = msg->stop;
        if (!stop) {
            reverse_text(msg->text, msg->length);
            if (write_all(STDOUT_FILENO, msg->text, msg->length) == -1 ||
                       (msg->newline && write_all(STDOUT_FILENO, "\n", 1) == -1)) {
                result = EXIT_FAILURE;
            }
            fprintf(stderr, "child%d: processed %zu bytes\n", number, msg->length);
        }
        if (munmap(msg, size) == -1) {
            perror("munmap child");
            result = EXIT_FAILURE;
        }
        if (stop || result != EXIT_SUCCESS) {
            break;
        }
        if (kill((pid_t)parent, SIGUSR2) == -1) {
            perror("kill parent");
            result = EXIT_FAILURE;
            break;
        }
    }
    if (close_fd(fd) == -1 || close_fd(STDOUT_FILENO) == -1) {
        result = EXIT_FAILURE;
    }
    return result;
}
