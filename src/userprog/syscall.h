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
void syscall_exit(int status);
void validate_args(int expected, void *arg1, void *arg2, void *arg3);
void validate_user_pointer (uint32_t *vaddr);

struct file_hash_item *get_file_hash_item_or_null(int fd);
struct file *get_file_or_null(int fd);
bool fd_exists(int fd);
uint32_t halt_userprog (void **, void **, void **);
uint32_t exit_userprog (void **, void **, void **);
uint32_t exec_userprog (void **, void **, void **);
uint32_t wait_userprog (void **, void **, void **);
uint32_t write_userprog (void **, void **, void **);
uint32_t open_userprog (void **, void **, void **);
uint32_t create_userprog (void **, void **, void **);
uint32_t remove_userprog (void **, void **, void **);
uint32_t close_userprog (void **, void **, void **);
uint32_t read_userprog (void **, void **, void **);
uint32_t seek_userprog (void **, void **, void **);
uint32_t tell_userprog (void **, void **, void **);
uint32_t file_size_userprog (void **, void **, void **);

#endif /* userprog/syscall.h */
