#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <hash.h>


typedef int pid_t;
#define CONSOLE_LIMIT 300

// Need a hash table from prcoess/thread id -> files
// Files being another hashtable from file descriptors to FILE*'s

struct process_hash_item
{
  struct hash files;   // hashtable of files this process has file descriptors for
  pid_t pid;           // pid calculated from the threads tid? 
  int next_fd;         // the next fd generated for a new file, MAX == 128. Starts at 2
  struct hash_elem *elem;
};

void syscall_init (void);
void validate_args (int expected, void *arg1, void *arg2, void *arg3);
void validate_user_pointer (const void *vaddr);

void exit (int);
pid_t exec (const char *);
int wait (pid_t);
int write (int fd, const void *buffer, unsigned size);

#endif /* userprog/syscall.h */
