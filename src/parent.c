#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef CHILD_PATH
#define CHILD_PATH "./child"
#endif

static char *read_filename(const char *prompt) {
    printf("%s\n", prompt);
    char *name = NULL;
    size_t cap = 0;
    ssize_t len = getline(&name, &cap, stdin);
    if (len == -1) {
        if (ferror(stdin)) {
            perror("getline filename");
        } else {
            fprintf(stderr, "expected two output file names\n");
        }
        free(name);
        return NULL;
    }
    if (len > 0 && name[len - 1] == '\n') {
        name[--len] = '\0';
    }
    if (len == 0) {
        fprintf(stderr, "empty file name\n");
        free(name);
        return NULL;
    }
    return name;
}

static pid_t start_child(int number, int maps[2], int outputs[2]) {
    pid_t pid = fork();
    if (pid != 0) {
        if (pid == -1) {
            perror("fork");
        }
        return pid;
    }
    if (dup2(outputs[number], STDOUT_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
    }
    if (close_fd(outputs[0]) == -1 || close_fd(outputs[1]) == -1 ||
        close_fd(maps[1 - number]) == -1) {
        _exit(EXIT_FAILURE);
    }
    char fd_text[32], parent_text[32], num_text[32];
    snprintf(fd_text, sizeof(fd_text), "%d", maps[number]);
    snprintf(parent_text, sizeof(parent_text), "%d", getppid());
    snprintf(num_text, sizeof(num_text), "%d", number + 1);
    execl(CHILD_PATH, "child", fd_text, parent_text, num_text, (char *)NULL);
    perror("execl child");
    _exit(EXIT_FAILURE);
}

static int wait_child(pid_t pid) {
    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return -1;
        }
    }
    if (WIFEXITED(status)) {
        printf("parent: child PID=%d, exit=%d\n", pid, WEXITSTATUS(status));
        return WEXITSTATUS(status) == 0 ? 0 : -1;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "parent: child PID=%d, signal=%d\n", pid, WTERMSIG(status));
    }
    return -1;
}

int main(void) {
    setbuf(stdout, NULL);
    printf("parent: PID=%d\n", getpid());

    char *names[2] = {NULL, NULL};
    int maps[2] = {-1, -1};
    int outputs[2] = {-1, -1};
    pid_t children[2] = {-1, -1};
    char *line = NULL;
    size_t cap = 0;
    int result = EXIT_FAILURE;

    names[0] = read_filename("File for child1 (length <= 10):");
    if (names[0] == NULL) {
        goto cleanup;
    }
    names[1] = read_filename("File for child2 (length > 10):");
    if (names[1] == NULL) {
        goto cleanup;
    }

    struct stat st[2];
    for (int i = 0; i < 2; i++) {
        outputs[i] = open(names[i], O_WRONLY | O_CREAT, 0644);
        if (outputs[i] == -1) {
            perror(names[i]);
            goto cleanup;
        }
        if (fstat(outputs[i], &st[i]) == -1) {
            perror("fstat output");
            goto cleanup;
        }
    }
    if (st[0].st_dev == st[1].st_dev && st[0].st_ino == st[1].st_ino) {
        fprintf(stderr, "output files must be different\n");
        goto cleanup;
    }
    for (int i = 0; i < 2; i++) {
        if (ftruncate(outputs[i], 0) == -1) {
            perror("ftruncate output");
            goto cleanup;
        }
        maps[i] = create_mapping();
        if (maps[i] == -1) {
            goto cleanup;
        }
    }

    sigset_t blocked;
    if (sigemptyset(&blocked) == -1 || sigaddset(&blocked, SIGUSR1) == -1 ||
        sigaddset(&blocked, SIGUSR2) == -1 || sigaddset(&blocked, SIGCHLD) == -1 ||
        sigprocmask(SIG_BLOCK, &blocked, NULL) == -1) {
        perror("block signals");
        goto cleanup;
    }
    for (int i = 0; i < 2; i++) {
        children[i] = start_child(i, maps, outputs);
        if (children[i] == -1) {
            goto cleanup;
        }
        printf("parent: started child%d, PID=%d\n", i + 1, children[i]);
    }

    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) != -1) {
        int newline = len > 0 && line[len - 1] == '\n';
        size_t text_len = (size_t)len - (size_t)newline;
        int target = text_len > 10 ? 1 : 0;
        if (put_message(maps[target], line, text_len, newline, 0) == -1 ||
            kill(children[target], SIGUSR1) == -1) {
            perror("send message");
            goto cleanup;
        }
        printf("parent: %zu chars -> child%d\n", text_len, target + 1);
        if (wait_signal(SIGUSR2, children[target], 1) == -1) {
            goto cleanup;
        }
    }
    if (ferror(stdin)) {
        perror("getline");
        goto cleanup;
    }
    for (int i = 0; i < 2; i++) {
        if (put_message(maps[i], NULL, 0, 0, 1) == -1 ||
            kill(children[i], SIGUSR1) == -1) {
            perror("send stop");
            goto cleanup;
        }
    }
    result = EXIT_SUCCESS;

cleanup:
    for (int i = 0; i < 2; i++) {
        if (children[i] > 0 && result != EXIT_SUCCESS) {
            if (kill(children[i], SIGTERM) == -1 && errno != ESRCH) {
                perror("kill child");
            }
        }
    }
    for (int i = 0; i < 2; i++) {
        if (children[i] > 0 && wait_child(children[i]) == -1) {
            result = EXIT_FAILURE;
        }
        if (maps[i] != -1 && close_fd(maps[i]) == -1) {
            result = EXIT_FAILURE;
        }
        if (outputs[i] != -1 && close_fd(outputs[i]) == -1) {
            result = EXIT_FAILURE;
        }
        free(names[i]);
    }
    free(line);
    return result;
}
