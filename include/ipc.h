#ifndef IPC_H
#define IPC_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    size_t length;
    int stop;
    int newline;
    char text[];
} Message;

int create_mapping(void);
int put_message(int fd, const char *text, size_t len, int newline, int stop);
Message *get_message(int fd, size_t *size);
int wait_signal(int signal_number, pid_t sender, int watch_children);
int write_all(int fd, const void *data, size_t len);
int close_fd(int fd);

#endif
