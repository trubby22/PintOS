#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdlib.h>
#include "filesys/off_t.h"
#include <stdbool.h>

#define EXE_BASE 0x08084000

// Supplemental page table
struct spt {
  // Size of executable in memory
  uint32_t size;
  // File containing executable
  struct file *file;

};

// Executable segment
struct exe_seg {
  // struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable
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
};

#endif