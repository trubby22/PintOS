#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "src/lib/kernel/hash.h"
#include "src/lib/kernel/bitmap.h"
#include "userprog/syscall.h"

struct supp_page_table {
  struct hash table;
  struct bitmap valid_bit_map;
};

struct page {
  int page_number;
  pid_t pid;
  struct frame *frame_ptr;
};

#endif