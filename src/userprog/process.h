#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct arg {
  char *str;
  struct list_elem elem;
};

struct length {
  int size;
  struct list_elem elem;
};

void init_hash_table (void);
struct process_hash_item *get_process_item(void);

bool
create_stack_page (void **esp, uint32_t pg_num);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

bool install_page(void *upage, void *kpage, bool writable);
bool load_page (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

#endif /* userprog/process.h */
