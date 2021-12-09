#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdlib.h>
#include "filesys/off_t.h"
#include <stdbool.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"

// Address in user memory at which executable segment starts
// In the spec it says that it should be: 0x08084000 but from the tests it seems like it's: 0x08048000
#define EXE_BASE 0x08048000

// Used in pinning
typedef void pin_or_unpin_frame (void *address);

// Represents type of data a page holds
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

  // List of all pages used by process (executable, file mappings).
  // Note: could have used a hash map here for better time complexity of search but since SPT is per-process (as opposed to global), this data structure is not that big.
  struct list pages;
  // Lock on struct list pages. Needed for the time when child is already running and is copying pages from its parent. At the same time the parent might be running as well.
  struct lock pages_lock;
};

// Executable page
struct spt_page {
  // Data type stored in page
  enum data_type type;
  // Denotes whether page has been loaded into memory
  bool loaded;
  // File to be loaded
  struct file *file;
  // File name. Used for executable pages. Used in sharing to determine whether sharing of executable pages is necessary.
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
  // Says whether segment can be written to. If false, segment is read-only. For files writable = true, for executable = false. Might become useful if we want to extend sharing with COW.
  bool writable;

  // Elem for adding to spt
  struct list_elem elem;
};

void spt_add_mmap_file (struct file *file, void *upage);
bool spt_remove_mmap_file (void *upage);
void spt_add_stack_page (void *upage);
void share_pages (struct thread *parent, struct thread *child);

bool pin_obj (void *uaddr, int size);
bool unpin_obj (void *uaddr, int size);

void free_process_spt (void);

#endif