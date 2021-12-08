#include "vm/swap.h"
#include "devices/block.h"
#include <hash.h>
#include <bitmap.h>
#include "vm/frame.h"

/*
Provides sector-based read and write access to block device. You will use this
interface to access the swap partition as a block device.
*/


#define SECTORS_PER_PAGE 8

struct swap_slot{
  uint32_t *pd;           //joint key
  void *vaddr;            //joint key
  block_sector_t sector;  //value
  int size;               //size in sectors
  struct hash_elem elem;  
}; 

//need not be a struct
struct swap_table{
  struct block *swap_block; 
  struct bitmap *bitmap; //bitmap to determine free sectors in block
  struct hash table;  //swap table recording swap slots for reading and writing
};

static struct swap_table swap_table;

void init_swap_table(void){
  swap_table.swap_block = block_get_role(BLOCK_SWAP);
  swap_table.bitmap = bitmap_create(4096);
  hash_init(&swap_table.table, NULL, NULL, NULL);
}


// could be void
bool add_swap_slot(struct frame* frame){
  size_t start = bitmap_scan_and_flip(swap_table.bitmap, 0, frame -> size * SECTORS_PER_PAGE, 0);
  if (start == BITMAP_ERROR)
  {
    return false; //or panic
  } else{
    //need to loop to do the whole frame!
    for (int i = 0; i < frame -> size * SECTORS_PER_PAGE; i++) 
      block_write(swap_table.swap_block, start + i, frame -> address + (i * BLOCK_SECTOR_SIZE));
  }
  return true;
}

// could be void
bool read_swap_slot(uint32_t *pd, void* vadrr, void* kpage){ //*frame instead?
  struct swap_slot *dummy_s;
  dummy_s -> pd = pd;
  dummy_s -> vaddr = vadrr;
  struct hash_elem *elem = hash_find(&swap_table.table, &dummy_s->elem);
  struct swap_slot *swap_slot = hash_entry(elem, struct swap_slot, elem);
  bitmap_set_multiple(&swap_table.bitmap, swap_slot -> sector, swap_slot -> size, 0);
  block_write(swap_table.swap_block, swap_slot -> sector, kpage);
  hash_delete(&swap_table.table, &swap_slot -> elem);
  return true;
}




