#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <inttypes.h>
#include "vm/frame.h"
#include "devices/block.h"

#define SECTORS_PER_PAGE 8

struct swap_slot{
  uint32_t *pd;           //joint key
  void *vaddr;            //joint key
  block_sector_t sector;  //value
  int size;               //size in sectors
  struct hash_elem elem;  

  // Holds pages that map to this swap slot
  struct list user_pages;

  // Lock on swap slot
  struct lock lock;
}; 

//need not be a struct
struct swap_table{
  struct block *swap_block; 
  struct bitmap *bitmap; //bitmap to determine free sectors in block
  struct hash table;  //swap table recording swap slots for reading and writing

  // Lock on swap table
  struct lock lock;
};

void init_swap_table(void);
void read_swap_slot(uint32_t *pd, void* vadrr, void* kpage);
bool write_swap_slot(struct frame* frame);

#endif