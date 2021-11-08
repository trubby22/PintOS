#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <inttypes.h>
#include <stdbool.h>

typedef int pid_t;
#define CONSOLE_LIMIT 300

void syscall_init (void);
void validate_user_pointer (const void *vaddr);


int write (int fd, const void *buffer, unsigned size);

#endif /* userprog/syscall.h */
