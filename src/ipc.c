#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int close_fd(int fd) {
    if (close(fd) == -1) {
        perror("close");
        return -1;
    }
    return 0;
}

int create_mapping(void) {
    char name[] = "/tmp/lab4-XXXXXX";
    int fd = mkstemp(name);
    if (fd == -1) {
        perror("mkstemp");
        return -1;
    }
    if (unlink(name) == -1) {
        perror("unlink");
        close_fd(fd);
        return -1;
    }
    return fd;
}

int put_message(int fd, const char *text, size_t len, int newline, int stop) {
    if (len > SIZE_MAX - sizeof(Message)) {
        errno = EOVERFLOW;
        perror("message size");
        return -1;
    }
    size_t size = sizeof(Message) + len;
    off_t file_size = (off_t)size;
    if (file_size < 0 || (size_t)file_size != size) {
        errno = EOVERFLOW;
        perror("file size");
        return -1;
    }
    if (ftruncate(fd, file_size) == -1) {
        perror("ftruncate");
        return -1;
    }
    Message *msg = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (msg == MAP_FAILED) {
        perror("mmap parent");
        return -1;
    }
    msg->length = len;
    msg->stop = stop;
    msg->newline = newline;
    if (len != 0) {
        memcpy(msg->text, text, len);
    }
    if (munmap(msg, size) == -1) {
        perror("munmap parent");
        return -1;
    }
    return 0;
}

Message *get_message(int fd, size_t *size) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return NULL;
    }
    if (st.st_size < (off_t)sizeof(Message) || (uintmax_t)st.st_size > SIZE_MAX) {
        fprintf(stderr, "bad mapping size\n");
        return NULL;
    }
    *size = (size_t)st.st_size;
    Message *msg = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (msg == MAP_FAILED) {
        perror("mmap child");
        return NULL;
    }
    if (msg->length != *size - sizeof(Message)) {
        fprintf(stderr, "bad message length\n");
        if (munmap(msg, *size) == -1) {
            perror("munmap");
        }
        return NULL;
    }
    return msg;
}

int wait_signal(int signal_number, pid_t sender, int watch_children) {
    sigset_t set;
    if (sigemptyset(&set) == -1 || sigaddset(&set, signal_number) == -1 ||
        (watch_children && sigaddset(&set, SIGCHLD) == -1)) {
        perror("signal set");
        return -1;
    }
    for (;;) {
        siginfo_t info;
        int sig = sigwaitinfo(&set, &info);
        if (sig == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("sigwaitinfo");
            return -1;
        }
        if (sig == SIGCHLD) {
            if (info.si_code == CLD_EXITED || info.si_code == CLD_KILLED ||
                info.si_code == CLD_DUMPED) {
                fprintf(stderr, "parent: child PID=%d exited before acknowledgement\n",
                        info.si_pid);
                return -1;
            }
            continue;
        }
        if (info.si_pid == sender) {
            return 0;
        }
    }
}

int write_all(int fd, const void *data, size_t len) {
    const char *p = data;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n == -1 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            if (n == 0) {
                errno = EIO;
            }
            perror("write");
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}
