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
  struct hash *files;   // hashtable of files this process has file descriptors for
  pid_t pid;           // pid calculated from the threads tid? 
  int next_fd;         // the next fd generated for a new file, MAX == 128. Starts at 2
  struct hash_elem elem;
};

unsigned hash_hash_fun(const struct hash_elem *e, void *aux UNUSED);
bool hash_less_fun (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
unsigned hash_hash_func_b(const struct hash_elem *e, void *aux UNUSED);
bool hash_less_fun_b (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

void syscall_init (void);
void validate_args (int expected, void *arg1, void *arg2, void *arg3);
void validate_user_pointer (const void *vaddr);

struct file *get_file(int fd);
void exit (int);
pid_t exec (const char *);
int wait (pid_t);
int write (int fd, const void *buffer, unsigned size);
int open (const char *file);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
void close (int fd);
int read (int fd, const void *buffer, unsigned size);

#endif /* userprog/syscall.h */
