#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include "lib/kernel/bitmap.h"
#include "userprog/syscall.h"

struct supp_page_table {
  struct hash table;
  struct bitmap *valid_bit_map;
};

struct page {
  int page_number;
  pid_t pid;
  struct frame *frame_ptr;
};

void init_supp_page_table(void *);

// Converts virtual address to physical address. If the virtual address is invalid, causes a PF.
void *convert_virtual_to_physical(void *);

#endif