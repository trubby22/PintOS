#include "vm/swap.h"
#include "devices/block.h"
#include <hash.h>

/*
devices/block.h’
‘devices/block.c’
Provides sector-based read and write access to block device. You will use this
interface to access the swap partition as a block device.
*/

//Need some sort bit map to find free slots, one page equals 4 sectors
//So we need to find at least 4 contigous sectors or more depending on the size 
//given

struct swap_slot{
  uint32_t *pd;           //joint key
  void *vaddr;            //joint key
  block_sector_t sector;  //value
  int size;               //size in sectors
  struct hash_elem elem;  
};

//need not be a struct
struct swap_table{
  uint32_t bitmap; //placeholder for correct bitmap
  struct hash table
};

void init_swap_table(void){
  struct block *swap_block = block_get_role(BLOCK_SWAP);
}

