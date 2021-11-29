#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include "lib/kernel/bitmap.h"
#include "userprog/syscall.h"

#define MAX_SUPP_PAGE_TABLE_SIZE 100

struct supp_page_table {
  struct hash table;
  struct bitmap *bitmap;
};

struct page { 
  // keys
  int page_number;
  pid_t pid;

  // values
  struct frame *frame_ptr;
  bool valid;
  bool accessed;
  bool dirty;

  // hash_elem
  struct hash_elem elem;
};

void init_supp_page_table(void);
void *convert_virtual_to_physical(void *);

unsigned sptable_hash_func (const struct hash_elem *e, void *aux);
bool sptable_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif