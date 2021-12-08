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

static struct frametable frame_table;

static void fix_queue(struct frame* new);

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

static struct frame *evict (struct frame *head);

/* Returns a new frame. Evicts if needed */
void * 
frame_insert (void* kpage, uint32_t *pd, void *vaddr, int size)
{
  struct frame *frame;
	if (!kpage)
	{
    frame = evict (frame_table.head);
  }
  else
  {
    frame = malloc(sizeof(struct frame));
    ASSERT(frame);
    frame -> address = kpage;
    
    //fix circular queue
    fix_queue(frame);
    //add to frame table
    hash_insert(&frame_table.table,&frame->elem);
	}

  list_init(&frame -> children);
  lock_init(&frame->lock);
  lock_init(&frame->children_lock);
  frame -> users = 1;
  frame -> size = size; //Should always be 1
  frame -> pd = pd;
  frame -> uaddr = vaddr;
	return frame -> address;
}

static void 
fix_queue(struct frame* new)
{
  if (!frame_table.head)
  {
    frame_table.head = new;
    new -> next = new;
    return;
  }
	struct frame *head = frame_table.head;
  new->next = head->next;
  head->next = new;
  frame_table.head = new->next;
}

/* Implements a second chance eviction algorithm
   will allocate a swap slot if needed */
static struct frame *evict (struct frame *head){
  do {
    void *uaddr = head -> uaddr;
    uint32_t *pd = head -> pd;
    bool save = pagedir_is_accessed(pd,uaddr) || pagedir_is_dirty(pd,uaddr);
    if (save || head -> pinned){
      pagedir_reset(pd,uaddr);
      head = head -> next;
    } else{
      break;
    }
  } while (true);
  
  void *uaddr = head -> uaddr;
  uint32_t *pd = head -> pd;
  bool save = pagedir_is_accessed(pd,uaddr) || pagedir_is_dirty(pd,uaddr);
  if (save || head -> pinned){
    pagedir_reset(pd,uaddr);
    return evict(head->next);
  }


	//Allocate swap slot for page panic if none left
  bool success = write_swap_slot(head);
  if (!success)
  {
    PANIC("at the distro");
  }

  //Removes the refernce to this frame in the page table entry
  uint32_t *pte = get_pte(pd, uaddr);
  *pte = *pte & PTE_FLAGS;
  
	frame_table.head = head -> next;
	return head;
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

