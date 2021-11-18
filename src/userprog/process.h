#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct argv_argc {
  char argv[4][128];
  int argc;
  char *file_name;
};

void init_hash_table (void);
struct process_hash_item *get_process_item(void);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
