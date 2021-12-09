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

static struct swap_table swap_table;

unsigned 
swap_hash(const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *s = hash_entry(e, struct frame, elem);

  ASSERT (s->id);

  return (unsigned) s->id;
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
  if (start == BITMAP_ERROR) {
    return false; //or panic
  } else {
    struct swap_slot *swap_slot = malloc(sizeof(swap_slot));
    if (swap_slot == NULL) {
      PANIC ("Swap slot allocation failed");
    }
    for (int i = 0; i < frame -> size * SECTORS_PER_PAGE; i++) {
      ASSERT (swap_table.swap_block);
      block_write(swap_table.swap_block, start + i, frame -> address + (i * BLOCK_SECTOR_SIZE));
    }
    swap_slot -> size = frame -> size * SECTORS_PER_PAGE;
    list_init(&swap_slot->user_pages);
    lock_init(&swap_slot->lock);

    lock_acquire(&frame->user_pages_lock);
    lock_acquire(&swap_slot->lock);

    // Moves user_pages from frame to swap slot
    while (!list_empty (&frame->user_pages)) {
      struct list_elem *e = list_pop_front (&frame->user_pages);
      list_push_back(&swap_slot->user_pages, e);
    }

    swap_slot->id = frame->id;

    lock_release(&swap_slot->lock);
    lock_release(&frame->user_pages_lock);

    hash_insert(&swap_table.table, &swap_slot->elem);
  }
  return true;
}

// could be void
void read_swap_slot(uint32_t *pd, void* vaddr, void* kpage){ //*frame instead?
  struct swap_slot dummy_s;
  struct user_page user_page;

  user_page.pd = pd;
  user_page.uaddr = vaddr;
  list_push_back(&dummy_s.user_pages, &user_page.elem);

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




