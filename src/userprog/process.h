#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

// Defines the maximum length and number of arguments passed to a user program
#define MAX_ARG_NUM 32
#define MAX_ARG_LEN 32

// Used to pass around data from process_execute to start_process
struct argv_argc {
  char argv[MAX_ARG_NUM][MAX_ARG_LEN];
  int argc;
  char *cmd_args_cpy;
};

struct arg {
  char *str;
  struct list_elem elem;
};

void init_hash_table (void);
struct process_hash_item *get_process_item(void);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
