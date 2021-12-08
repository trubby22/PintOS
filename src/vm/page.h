#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdlib.h>
#include "filesys/off_t.h"
#include <stdbool.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"

// In the spec it says that it should be: 0x08084000 but from the tests it seems like it's: 0x08048000
#define EXE_BASE 0x08048000

enum data_type {
  STACK,
  EXECUTABLE,
  FILE
};

// Supplemental page table
struct spt {
  // Size of stack
  uint32_t stack_size;
  // Size of executable in memory
  uint32_t exe_size;

  // List of all pages used by process (executable, file mappings)
  struct list pages;
  // Lock on struct list pages
  // If only one process at a time can access pages, this lock is redundant
  struct lock pages_lock;
};

// Executable page
struct spt_page {
  // Data type stored in page
  enum data_type type;
  // Denotes whether page has been loaded
  bool loaded;
  // File to be loaded
  struct file *file;
  // File name. Used for executable pages.
  char *file_name;

  // Metadata passed in to load_segment
  // Offset within executable file
  off_t ofs;
  // Page's virtual memory address
  uint8_t *upage;
  // Number of bytes to fill with data
  uint32_t read_bytes;
  // Number of bytes to fill with zeros
  uint32_t zero_bytes;
  // Says whether segment can be written to. If false, segment is read-only.
  bool writable;

  // Elem for adding to spt
  struct list_elem elem;
  // Elem for adding spt_page to list of children in parent's frame
  struct list_elem parent_elem;

  // Pointer to thread that owns spt_page
  // Possibly redundant
  struct thread *thread;
};

void spt_add_mmap_file (int fd, void *upage);
bool spt_remove_mmap_file (void *upage);
void spt_add_stack_page (void *upage);
void share_pages (struct thread *parent, struct thread *child);
void free_process_resources (struct thread *t);

bool pin_obj (void *uaddr, int size);
bool unpin_obj (void *uaddr, int size);

#endif