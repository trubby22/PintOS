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

// Supplemental page table
struct spt {
  // Size of executable in memory
  uint32_t size;

  // List of executable pages
  // struct list exe_pages;
  // List of pages belonging to memory-mapped files
  // struct list mmap_pages;

  // List of all pages used by process (executable, file mappings)
  struct list pages;
  // Lock on struct list pages
  struct lock pages_lock;
};

// Executable page
struct spt_page {
  // Start address of page after it's been loaded to user virtual memory (end address = start_addr + PGSIZE)
  // TODO: remove start_addr because it's a copy of upage
  uint32_t start_addr;
  // Denotes whether page has been loaded
  bool loaded;
  // File to be loaded
  struct file *file;

  // Metadata passed in to load_segment
  // Offset within executable file
  off_t ofs;
  // Address at which the segment is to be saved
  uint8_t *upage;
  // Number of bytes to fill with data
  uint32_t read_bytes;
  // Number of bytes to fill with zeros
  uint32_t zero_bytes;
  // Says whether segment can be written to. If false, segment is read-only.
  bool writable;
  // Elem for adding to hash segments in spt
  struct list_elem elem;
};

void spt_add_mmap_file (int fd, void *upage);
bool spt_remove_mmap_file (void *upage);

#endif