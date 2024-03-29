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

// Note: swap_table would benefit from being a list instead of a hash table since hash_find is never called; so we're making the data structure more complex without using its advantages
static struct swap_table swap_table;
// Lock on swap_table
static struct lock swap_table_lock;

unsigned 
swap_hash(const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) e;
}

bool 
swap_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  return swap_hash(a,NULL) < swap_hash(b, NULL);
}

static void swap_destroy (struct hash_elem *e, void *aux) {
  struct swap_slot *swap_slot = hash_entry(e, struct swap_slot, elem);
  free(swap_slot);
}

void init_swap_table(void){
  swap_table.swap_block = block_get_role(BLOCK_SWAP);
  swap_table.bitmap = bitmap_create(4096);
  hash_init(&swap_table.table, swap_hash, swap_less, NULL);
  lock_init(&swap_table.lock);
  lock_init(&swap_table_lock);
}

// Writes data from frame to swap slot. Mallocs a swap slot in the process.
// could be void
/* Must already hold frame's user pages lock */
bool 
write_swap_slot(struct frame* frame)
{
  // Sets bits to 0
  size_t start = bitmap_scan_and_flip(swap_table.bitmap, 0, frame -> size * SECTORS_PER_PAGE, 0);

  if (start == BITMAP_ERROR) {
    PANIC("Failed to find available swap slot");
  }

  struct swap_slot *swap_slot = malloc(sizeof(struct swap_slot));
  if (swap_slot == NULL) {
    PANIC ("Swap slot allocation failed");
  }

  // Copies contents of frame to swap_slot
  for (int i = 0; i < frame -> size * SECTORS_PER_PAGE; i++) {
    ASSERT (swap_table.swap_block);
    block_write(swap_table.swap_block, start + i, frame -> address + (i * BLOCK_SECTOR_SIZE));
  }

  swap_slot -> size = frame -> size * SECTORS_PER_PAGE;
  list_init(&swap_slot->user_pages);
  lock_init(&swap_slot->lock);

  lock_acquire(&swap_slot->lock);

  // Moves user_pages from frame to swap slot preserving ordering
  while (!list_empty (&frame->user_pages)) {
    struct list_elem *e = list_pop_front (&frame->user_pages);
    struct user_page *user_page = list_entry(e, struct user_page, elem);
    user_page->frame_or_swap_slot_ptr = swap_slot;
    user_page->used_in = SWAP;
    list_push_back(&swap_slot->user_pages, e);
  }

  lock_release(&swap_slot->lock);

  hash_insert(&swap_table.table, &swap_slot->elem);
  return true;
}

// Writes data from swap slot to frame
// could be void
bool read_swap_slot(uint32_t *pd, void* vaddr, void* kpage){ //*frame instead?
  // For now works only for non-shared pages
  struct swap_slot *swap_slot = lookup_swap_slot(vaddr, pd);
  if (swap_slot == NULL)
  {
    return false;
  }
  // Copies data from swap_slot to frame
  for (int i = 0; i < swap_slot -> size; i++){
    block_write(swap_table.swap_block, swap_slot -> sector + i, kpage + (i* BLOCK_SECTOR_SIZE));
  }
  block_write(swap_table.swap_block, swap_slot -> sector, kpage);
 
  struct frame *frame = lookup_frame(kpage);
  ASSERT (frame != NULL);

  lock_acquire(&swap_slot->lock);
  lock_acquire(&frame->user_pages_lock);

  // Moves user_pages from swap_slot to frame
  while (!list_empty (&swap_slot->user_pages)) {
    struct list_elem *e = list_pop_front (&swap_slot->user_pages);
    struct user_page *user_page = list_entry(e, struct user_page, elem);
    user_page->frame_or_swap_slot_ptr = frame;
    user_page->used_in = FRAME;
    list_push_back(&frame->user_pages, e);
  }

  lock_release(&frame->user_pages_lock);
  lock_release(&swap_slot->lock);
  

  //delete_swap_slot(swap_slot);
}

// Helper function. Used in read_swap_slot and when process exits to delete swap_slots where the process's data is (unless swap_slot has data from frame that is shared and other processes with access are still alive).
void delete_swap_slot (struct swap_slot *swap_slot) {
  bitmap_set_multiple(swap_table.bitmap, swap_slot -> sector, swap_slot -> size, 0); //swap slot sector gets corrupted before here
  hash_delete(&swap_table.table, &swap_slot -> elem);
  free(swap_slot);
}

// Gets swap_slot that upage interpreted under pd points to
struct swap_slot *lookup_swap_slot (void *upage, void *pd) {
  struct list *all_user_pages = get_all_user_pages();
  struct list_elem *e;

  for (e = list_begin (all_user_pages); e != list_end (all_user_pages); e = list_next (e)) {

    struct user_page *user_page = list_entry (e, struct user_page, allelem);
    if (user_page->pd == pd && user_page->uaddr == upage && user_page->used_in == SWAP) {

      struct swap_slot *swap_slot = user_page->frame_or_swap_slot_ptr;
      return swap_slot;
    }
  }

  return NULL;
}

// Called at OS termination
void remove_all_swap_slots (void) {
  hash_destroy(&swap_table.table, swap_destroy);
}

// Get static variables from this class
struct swap_table *get_swap_table (void) {
  return &swap_table;
}

struct lock *get_swap_table_lock (void) {
  return &swap_table_lock;
}


