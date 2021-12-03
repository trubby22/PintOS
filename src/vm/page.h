#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdlib.h>
#include "filesys/off_t.h"
#include <stdbool.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"

// In the spec it says that it should be: 0x08084000 but from the tests it seems like it's: 0x08048000
#define EXE_BASE 0x08048000

// Supplemental page table
struct spt {
  // Size of executable in memory
  uint32_t size;
  // File containing executable
  struct file *file;
  // struct list segments;
  struct list segments;
};

// Executable segment
struct segment {
  // Start and end addresses of segment after it's been loaded to user virtual memory
  uint32_t start_addr;
  uint32_t end_addr;

  // Denotes whether segment has already been loaded into user virtual memory
  bool loaded;

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

struct spt *create_spt(void);

// unsigned seg_hash_func (const struct hash_elem *e, void *aux);
// bool seg_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif