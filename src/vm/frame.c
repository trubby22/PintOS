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

struct frame_owner{
  uint32_t *pd;
  void *uaddr;
  struct list_elem elem;
};

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

void 
init_frame_table(void)
{
  list_init(&frame_table.list);
  hash_init(&frame_table.table, &frame_hash, &frame_less, NULL);
  lock_init(&frame_table.lock);
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
    ASSERT(frame);
    frame -> address = kpage;
    if (!frame_table.current)
    {
      list_push_front(&frame_table.list, &frame->list_elem);
      frame_table.current = &frame->list_elem;
      
    }
    
    list_insert(&frame_table.current, &frame->list_elem);
    hash_insert(&frame_table.table,&frame->elem);
	}

  ASSERT (frame != NULL);
  list_init(&frame -> children);
  lock_init(&frame->lock);
  lock_init(&frame->children_lock);
  frame -> users = 1;
  frame -> size = size; //Should always be 1
  frame -> pd = pd;
  frame -> uaddr = vaddr;
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
static struct frame *evict (struct list_elem *current){
  struct list_elem* next;
  void *uaddr;
  bool save;
  uint32_t *pd;
  struct frame *frame;
  do {
    frame = list_entry(current, struct frame, list_elem);
    uaddr = frame -> uaddr;
    pd = frame -> pd;
    save = pagedir_is_accessed(pd,uaddr) || pagedir_is_dirty(pd,uaddr);
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
  frame -> pinned = false;
}

void reset_accessed_bits (struct hash_elem *e, void *aux UNUSED){
  struct frame *f = hash_entry(e, struct frame, elem);
  pagedir_reset(f->pd, f->uaddr);
}

void reset_all_accessed_bits(void){
  hash_apply (&frame_table.table, reset_accessed_bits);
}

