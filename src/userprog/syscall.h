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

// Used in a hashtable to map file descriptors to FILE structs.
struct file_hash_item
{
  struct file *file;  //The actual file
  int fd;      //File descriptor, for the hash function
  struct hash_elem elem;
};

unsigned hash_hash_fun(const struct hash_elem *e, void *aux UNUSED);
bool hash_less_fun (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
unsigned hash_hash_fun_b(const struct hash_elem *e, void *aux UNUSED);
bool hash_less_fun_b (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

void syscall_init (void);
void validate_args (int expected, void *arg1, void *arg2, void *arg3);
void validate_user_pointer (const void *vaddr);

struct file_hash_item *get_file_hash_item(int fd);
struct file *get_file(int fd);
void halt_userprog (void);
void exit_userprog (int);
pid_t exec_userprog (const char *);
int wait_userprog (pid_t);
int write_userprog (int fd, const void *buffer, unsigned size);
int open_userprog (const char *file);
bool create_userprog (const char *file, unsigned initial_size);
bool remove_userprog (const char *file);
void close_userprog (int fd);
int read_userprog (int fd, const void *buffer, unsigned size);
void seek_userprog (int fd, unsigned position);
unsigned tell_userprog (int fd);

#endif /* userprog/syscall.h */
