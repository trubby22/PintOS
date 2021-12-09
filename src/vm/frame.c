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

static struct frame *evict (struct list_elem *cuurent);

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
      list_push_front(&frame_table.list, &frame->list_elem);
      frame_table.current = &frame->list_elem;
      
    }
    list_init(&frame->user_pages);
    lock_init(&frame->lock);
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

//Causing page fault!
void free_frames(void* pages, size_t page_cnt){
 struct frame *dummy_f;
 dummy_f -> address = pages;
 struct hash_elem *elem = hash_delete(&frame_table.table, &dummy_f -> elem);
 if (elem)
 {
   struct frame *f = hash_entry(elem, struct frame, elem);
   list_remove(&f->list_elem);
   free(f);
 }
}

/* Implements a second chance eviction algorithm
   will allocate a swap slot if needed */
static struct frame *evict (struct list_elem *current) {
  struct list_elem* next;
  void *uaddr;
  bool save;
  uint32_t *pd;
  struct frame *frame;
  do {
    frame = list_entry(current, struct frame, list_elem);

    lock_acquire(&frame->user_pages_lock);

    // If all processes that had access to this frame have exited, return frame without writing it to swap
    if (list_size(&frame->user_pages) == 0) {
      pagedir_clear_page(pd, uaddr);
      lock_release(&frame->user_pages_lock);
      return frame;
    }

    struct list_elem *elem = list_front(&frame->user_pages);
    struct user_page *user_page = list_entry(elem, struct user_page, elem);
    uaddr = user_page->uaddr;
    pd = user_page->pd;

    lock_release(&frame->user_pages_lock);

    bool accessed = pagedir_is_accessed(pd,uaddr);
    bool dirty = pagedir_is_dirty(pd,uaddr);
    bool save = (accessed || dirty);
    if (save || frame -> pinned){
      pagedir_reset(pd,uaddr);
      next = list_next(current);
      if (is_tail(next))
      {
        current = list_front(&frame_table.list);
      } else{
        current = next;
      }
      frame_table.current = current;
    } else{
      break;
    }
  } while (true);
  
	//Allocate swap slot for page panic if none left
  bool success = write_swap_slot(frame);
  ASSERT(success);

  //Removes the refernce to this frame in the page table entry
  //No it doesn't??
  pagedir_clear_page(pd, uaddr);
	return frame;
}

struct frametable *get_frame_table (void) {
  return &frame_table;
}

// idea: bring the frame at the passed address to RAM (unless it's already there) and make sure it stays there until unpin_frame is called
void pin_frame (void *address) {
  struct frame *frame = lookup_frame(address);
  if (!frame)
    PANIC("Kill");
  frame->pinned = true;
}

void unpin_frame (void *address) {
  struct frame *frame = lookup_frame(address);
  ASSERT (frame);
  frame -> pinned = false;
}

void reset_accessed_bits (struct hash_elem *e, void *aux UNUSED){
  struct frame *f = hash_entry(e, struct frame, elem);
  pagedir_reset(f->pd, f->uaddr);
}

void reset_all_accessed_bits(void){
  hash_apply (&frame_table.table, reset_accessed_bits);
}

// Frees user_page regardless of whether it is currently in frame or in swap_slot
void remove_user_page (void *kpage, void *pd) {
  struct list_elem *e;

  lock_acquire(&all_user_pages_lock);

  for (e = list_begin (&all_user_pages); e != list_end (&all_user_pages); e = list_next (e)) {
    struct user_page *user_page = list_entry (e, struct user_page, allelem);
    if (user_page->pd == pd 
    && kpage == pagedir_get_page(pd, user_page->uaddr)) {
      // Remove from static list
      list_remove(e);
      lock_release(&all_user_pages_lock);
      // Remove from frame or swap_slot
      list_remove(&user_page->elem);
      // Remove swap slot if user_page was embedded in it and if no other processes have reference to that swap_slot. For now doesn't always properly delete if page is shared.
      // If swap slot was embedded in frame, we don't have to do anything. The frame will simply be reused earlier in evict function.
      struct swap_slot *swap_slot = lookup_swap_slot(user_page->uaddr, pd);
      if (swap_slot != NULL && list_size(&swap_slot->user_pages) == 0) {
        delete_swap_slot(swap_slot);
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