#include "vm/swap.h"
#include "devices/block.h"
#include <hash.h>
#include <bitmap.h>
#include "vm/frame.h"
#include "threads/malloc.h"
#include <stdbool.h>

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

unsigned 
swap_hash(const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *s = hash_entry(e, struct frame, elem);
  return (unsigned) s -> pd ^ (unsigned) s -> uaddr;
}

bool 
swap_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  return swap_hash(a,NULL) < swap_hash(b, NULL);
}


void init_swap_table(void){
  swap_table.swap_block = block_get_role(BLOCK_SWAP);
  swap_table.bitmap = bitmap_create(4096);
  hash_init(&swap_table.table, swap_hash, swap_less, NULL);
}


// could be void
bool write_swap_slot(struct frame* frame){
  size_t start = bitmap_scan_and_flip(swap_table.bitmap, 0, frame -> size * SECTORS_PER_PAGE, 0);
  if (start == BITMAP_ERROR)
  {
    return false; //or panic
  } else{
    struct swap_slot *swap_slot = malloc(sizeof(swap_slot));
    for (int i = 0; i < frame -> size * SECTORS_PER_PAGE; i++) 
      block_write(swap_table.swap_block, start + i, frame -> address + (i * BLOCK_SECTOR_SIZE));
    swap_slot -> size = frame -> size * SECTORS_PER_PAGE;
    swap_slot -> pd = frame -> pd;
    swap_slot -> vaddr = frame -> uaddr;
    hash_insert(&swap_table.table,&swap_slot->elem);
  }
  return true;
}

// could be void
void read_swap_slot(uint32_t *pd, void* vadrr, void* kpage){ //*frame instead?
  struct swap_slot dummy_s;
  dummy_s.pd = pd;
  dummy_s.vaddr = vadrr;
  struct hash_elem *elem = hash_find(&swap_table.table, &dummy_s.elem);
  struct swap_slot *swap_slot = hash_entry(elem, struct swap_slot, elem);
  bitmap_set_multiple(swap_table.bitmap, swap_slot -> sector, swap_slot -> size, 0);
  for (int i = 0; i < swap_slot -> size; i++){
    block_write(swap_table.swap_block, swap_slot -> sector + i, kpage + (i* BLOCK_SECTOR_SIZE));
  }
  block_write(swap_table.swap_block, swap_slot -> sector, kpage);
  hash_delete(&swap_table.table, &swap_slot -> elem);
  free(swap_slot);
}




