#include "vm/frame.h"
#include <stdbool.h>
#include <hash.h>
#include <debug.h>
#include <inttypes.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"

/* A frame table maps a frame to a user page. */
struct frametable
{
  struct frame *head; //Head of circular queue, needed for eviction
  struct hash table;  //
};

struct frame
{
  uint32_t frame_number;
  void* address;           //value
  uint32_t *pd;            //page directory that owns this frame
  void* uaddr;             //page that owns this frame
  int size;                //if added through frame multiple
  struct hash_elem elem;   //Elem to be part of frame table
  struct frame *next;      //Pointer to be part of circular queue for eviction
};

static void fix_queue(struct frame* new);

unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *f = hash_entry(e, struct frame, elem);
  return (unsigned) f -> address;
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  return frame_hash(a,NULL) < frame_hash(b, NULL);
}

static struct frametable frame_table;

void init_frame_table(void)
{
  hash_init(&frame_table.table, frame_hash, frame_less, NULL);
}

/* Looks up frame with kpage in the frame table 
   returns NULL if the frame does not exist */
   //Is this function even needed?
struct frame *lookup_frame(void *kpage){
  //search table using dummy elem
  struct frame *dummy_f;
  dummy_f -> address = kpage;
  struct hash_elem *f = hash_find(&frame_table.table, &dummy_f->elem);
  //miss
  if (!f){
    return NULL;
  }
  //hit
  struct frame *frame = hash_entry(f, struct frame, elem);
  return frame -> address;
}

static struct frame *evict (struct frame *head);

/* Returns a new frame. Evicts if needed */
void* frame_insert (void* kpage, uint32_t *pd, void *vaddr, int size){
  struct frame *frame;
	if (!kpage)
	{
    frame = evict (frame_table.head);
    //free the page in case the size is different
    palloc_free_multiple(frame -> address, frame -> size);
    frame -> address = palloc_get_multiple(PAL_USER | PAL_ZERO, size);
	} else{
    frame = malloc(sizeof(struct frame));
    ASSERT(frame);
    frame -> address = kpage;
    //fix circular queue
    fix_queue(frame);
    //add to frame table
    hash_insert(&frame_table.table,&frame->elem);
	}
  frame -> size = size;
  frame -> pd = pd;
  frame -> uaddr = vaddr;
	return frame -> address;
}

static void fix_queue(struct frame* new){
  if (!frame_table.head)
  {
    frame_table.head = new;
    return;
  }
	struct frame *head = frame_table.head;
	new -> next  = head -> next;
	head -> next = new;
	frame_table.head = new -> next;
}


/* Implements a second chance eviction algorithm
   will allocate a swap slot if needed */
static struct frame *evict (struct frame *head){
  void *uaddr = head -> uaddr;
  uint32_t *pd = head -> pd;
  bool save = pagedir_is_accessed(pd,uaddr) || pagedir_is_dirty(pd,uaddr);
  if (save){
    pagedir_reset(pd,uaddr);
    return evict(head->next);
  }
	//Allocate swap slot for page panic if none left
 
  //Removes the refernce to this frame in the page table entry
  uint32_t *pte = get_pte(pd, uaddr);
  *pte = *pte & PTE_FLAGS;
  
	frame_table.head = head -> next;
	return head;
}
