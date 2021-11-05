#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <inttypes.h>
#include <stdbool.h>

typedef int pid_t;

void syscall_init (void);
void validate_user_pointer (uint32_t *pd, void *user_pointer);


#endif /* userprog/syscall.h */
