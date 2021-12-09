#include "vm/frame.h"
#include <stdbool.h>
#include <hash.h>
#include <debug.h>
#include <inttypes.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "vm/swap.h"
#include <list.h>

static struct frametable frame_table;
static struct list all_user_pages;
static struct lock all_user_pages_lock;

static void fix_queue(struct frame* new);
static bool at_least_one_accessed_or_dirty (struct list *user_pages);
static void reset_pagedirs_of_user_pages (struct list *user_pages);
static void clear_pages_of_user_pages (struct list *user_pages);
static void reset_accessed_bits_of_user_pages (struct list *user_pages);
static void user_pages_forall (struct list *user_pages, pagedir_generic_function *pagedir_generic_function);

unsigned 
frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *f = hash_entry(e, struct frame, elem);
  return (unsigned) f -> address;
}

bool 
frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  return frame_hash(a,NULL) < frame_hash(b, NULL);
}

// For freeing frames when OS exits
static void frame_destroy (struct hash_elem *e, void *aux) {
  struct frame *frame = hash_entry(e, struct frame, elem);
  free(frame);
}

void 
init_frame_table(void)
{
  list_init(&frame_table.list);
  hash_init(&frame_table.table, &frame_hash, &frame_less, NULL);
  lock_init(&frame_table.lock);
  list_init(&all_user_pages);
  lock_init(&all_user_pages_lock);
}

/* Looks up frame with kpage in the frame table 
   returns NULL if the frame does not exist */
struct frame *
lookup_frame(void *kpage)
{
  //search table using dummy elem
  struct frame dummy_f;
  dummy_f.address = kpage;
  struct hash_elem *f = hash_find(&frame_table.table, &dummy_f.elem);

  // miss
  if (!f){
    return NULL;
  }
  //hit
  struct frame *frame = hash_entry(f, struct frame, elem);
  return frame;
}

static struct frame *evict (struct list_elem *current);

/* Returns a new frame. Evicts if needed */
void * 
frame_insert (void* kpage, uint32_t *pd, void *vaddr, int size)
{
  struct frame *frame;
	if (!kpage)
	{
    frame = evict (frame_table.current);
  } else
  {
    frame = malloc(sizeof(struct frame));
    if (frame == NULL) {
      PANIC ("Malloc failed");
    }
    frame -> address = kpage;
    if (!frame_table.current)
    {
      lock_acquire(&frame_table.lock);
      list_push_front(&frame_table.list, &frame->list_elem);
      lock_release(&frame_table.lock);

      frame_table.current = &frame->list_elem;
    }
    list_init(&frame->user_pages);
    lock_init(&frame->user_pages_lock);
    
    list_insert(&frame_table.current, &frame->list_elem);
    hash_insert(&frame_table.table,&frame->elem);
	}

  ASSERT (frame != NULL);

  struct user_page *user_page = malloc(sizeof(struct user_page));
  if (user_page == NULL) {
    PANIC ("Malloc failed");
  }

  user_page->pd = pd;
  user_page->uaddr = vaddr;

  // Adds user_page to frame
  lock_acquire(&frame->user_pages_lock);
  list_push_back(&frame->user_pages, &user_page->elem);
  lock_release(&frame->user_pages_lock);

  // Adds user_page to static list all_user_pages
  lock_acquire(&all_user_pages_lock);
  list_push_back(&all_user_pages, &user_page->allelem);
  lock_release(&all_user_pages_lock);

  frame->id = (uint32_t) pd ^ (uint32_t) vaddr;
  frame -> size = size; //Should always be 1
	return frame -> address;
}

// Free page_cnt frames starting at address pages. Usually (maybe even always) page_cnt = 1
// Causing page fault!
void free_frames(void* pages, size_t page_cnt){
  struct frame *dummy_f;
  dummy_f -> address = pages;

  lock_acquire(&frame_table.lock);
  struct hash_elem *elem = hash_delete(&frame_table.table, &dummy_f -> elem);
  lock_release(&frame_table.lock);

  if (elem)
  {
    struct frame *f = hash_entry(elem, struct frame, elem);

    lock_acquire(&frame_table.lock);
    list_remove(&f->list_elem);
    lock_release(&frame_table.lock);

    free(f);
  }
}

/* Implements a second chance eviction algorithm
   will allocate a swap slot if needed */
static struct frame *evict (struct list_elem *current) {
  struct list_elem* next;
  bool save;
  struct frame *frame;
  struct list *user_pages;
  do {
    frame = list_entry(current, struct frame, list_elem);

    lock_acquire(&frame->user_pages_lock);

    // If all processes that had access to frame have exited, return frame without writing it to swap
    if (list_size(&frame->user_pages) == 0) {
      lock_release(&frame->user_pages_lock);
      return frame;
    }

    // Need to check all pages mapped to frame in case frame is shared
    user_pages = &frame->user_pages;
    bool save = at_least_one_accessed_or_dirty(user_pages);

    if (save || frame -> pinned){
      reset_pagedirs_of_user_pages(user_pages);

      lock_acquire(&frame_table.lock);

      next = list_next(current);
      if (is_tail(next))
      {
        current = list_front(&frame_table.list);
      } else{
        current = next;
      }
      frame_table.current = current;

      lock_release(&frame_table.lock);
    } else{
      break;
    }
  } while (true);
  
	// Allocate swap slot for page panic if none left
  bool success = write_swap_slot(frame);
  ASSERT(success);

  // Removes the refernce to this frame in the page table entry
  clear_pages_of_user_pages(user_pages);
  lock_release(&frame->user_pages_lock);
	return frame;
}

struct frametable *get_frame_table (void) {
  return &frame_table;
}

struct list *get_all_user_pages (void) {
  return &all_user_pages;
}

struct lock *get_all_user_pages_lock (void) {
  return &all_user_pages_lock;
}

// Used for user memory access in syscall handler
// idea: bring the frame at the passed address to RAM (unless it's already there) and make sure it stays there until unpin_frame is called
void pin_frame (void *address) {
  struct frame *frame = lookup_frame(address);
  ASSERT (frame);
  frame->pinned = true;
}

void unpin_frame (void *address) {
  struct frame *frame = lookup_frame(address);
  ASSERT (frame);
  frame -> pinned = false;
}

// Resets accessed bits for one frame
void reset_accessed_bits (struct hash_elem *e, void *aux UNUSED){
  struct frame *f = hash_entry(e, struct frame, elem);
  struct list *user_pages = &f->user_pages;

  lock_acquire(&f->user_pages_lock);
  reset_accessed_bits_of_user_pages (user_pages);
  lock_release(&f->user_pages_lock);
}

// Resets accessed bits for all frames in frametable
void reset_all_accessed_bits(void){
  hash_apply (&frame_table.table, reset_accessed_bits);
}

// Frees user_page regardless of whether it is currently in frame or in swap_slot
void remove_user_page (void *kpage, void *pd) {
  struct list_elem *e;

  lock_acquire(&all_user_pages_lock);

  // Iterate through all user_page structs in OS
  for (e = list_begin (&all_user_pages); e != list_end (&all_user_pages); e = list_next (e)) {
    struct user_page *user_page = list_entry (e, struct user_page, allelem);

    if (user_page->pd == pd 
    && kpage == pagedir_get_page(pd, user_page->uaddr)) {
      // Remove from list all_user_pages
      list_remove(e);
      lock_release(&all_user_pages_lock);

      // Remove from frame or swap_slot
      list_remove(&user_page->elem);
      // Remove swap slot if user_page was embedded in it and if no other processes have reference to that swap_slot. For now doesn't always properly delete if page is shared.
      // If swap slot was embedded in frame, we don't have to do anything. The frame will simply be reused earlier in evict function.
      struct swap_table *swap_table = get_swap_table();

      lock_acquire(&swap_table->lock);
      struct swap_slot *swap_slot = lookup_swap_slot(user_page->uaddr, pd);
      lock_release(&swap_table->lock);

      if (swap_slot != NULL) {
        lock_acquire(&swap_slot->lock);
        if (list_size(&swap_slot->user_pages) == 0) {
          delete_swap_slot(swap_slot);
        }
        lock_release(&swap_slot->lock);
      }
      free(user_page);
      return;
    }
  }

  lock_release(&all_user_pages_lock);

  PANIC ("Didn't find matching user page");
}

void remove_all_frames (void) {
  hash_destroy(&frame_table.table, frame_destroy);
}

// Used in eviction. Must be called with lock on user_pages list.
static bool at_least_one_accessed_or_dirty (struct list *user_pages) {
  struct list_elem *e;
  for (e = list_begin (user_pages); e != list_end (user_pages); e = list_next (e)) {
    struct user_page *user_page = list_entry(e, struct user_page, elem);
    const void *uaddr = user_page->uaddr;
    uint32_t *pd = user_page->pd;

    bool accessed = pagedir_is_accessed(pd,uaddr);
    bool dirty = pagedir_is_dirty(pd,uaddr);
    bool save = (accessed || dirty);

    if (save) {
      return true;
    }
  }

  return false;
}

// Used in eviction.
static void reset_pagedirs_of_user_pages (struct list *user_pages) {
  user_pages_forall(user_pages, pagedir_reset);
}

// Used in eviction.
static void clear_pages_of_user_pages (struct list *user_pages) {
  user_pages_forall(user_pages, pagedir_clear_page);
}

// Used in eviction.
static void reset_accessed_bits_of_user_pages (struct list *user_pages) {
  user_pages_forall(user_pages, reset_accessed_bits);
}

// Helper function
static void user_pages_forall (struct list *user_pages, pagedir_generic_function *pagedir_generic_function) {
  struct list_elem *e;
  for (e = list_begin (user_pages); e != list_end (user_pages); e = list_next (e)) {
    struct user_page *user_page = list_entry(e, struct user_page, elem);
    const void *uaddr = user_page->uaddr;
    uint32_t *pd = user_page->pd;

    pagedir_generic_function(pd, uaddr);
  }
}