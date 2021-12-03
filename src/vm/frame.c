#include "vm/frame.h"
#include <stdbool.h>
#include <hash.h>
#include "threads/palloc.h"
#include <debug.h>
#include <inttypes.h>
#include "threads/malloc.h"
#include "userprog/pagedir.h"

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
  bool save;               //If 1 then frame is saved, not needed
  struct hash_elem elem;   //Elem to be part of frame table
  struct frame *next;      //Pointer to be part of circular queue for eviction
};

static void fix_queue(struct frame* new);

unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *f = hash_entry(e, struct frame, elem);
  return f -> address;
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  return frame_hash(a,NULL) < frame_hash(b, NULL);
}

static struct frametable frame_table;

void init_frame_table(void)
{
  struct hash table;
  frame_table.table = table;
  hash_init(&table, frame_hash, frame_less, NULL);
}

/* Looks up frame with frameid FID in the frame table 
   returns NULL if the frame does not exist */
   //Is this function even needed?
uint32_t lookup_frame(uint32_t frame_number){
  //search table using dummy elem
  struct hash_elem *dummy_f;
  struct hash_elem *f = hash_find(&frame_table.table, dummy_f);
  //miss
  if (!f){
    return NULL;
  }
  //hit
  struct frame *frame = hash_entry(f, struct frame, elem);
  return frame -> address;
}


/* Returns a new frame. Evicts if needed */
// TODO: Replace calls to palloc_get_page in pagedir to calls to get_frame
void* get_frame (uint32_t *pd, void *vaddr){ 
  struct frame* frame;
  void *frame_address = palloc_get_page(PAL_USER);
	if (!frame_address)
	{
    //Should call frame = evict (frame_table -> head); but will just panic for now
		PANIC("Ran out of free frames");
	} else{
    frame = malloc(sizeof(struct frame));
    ASSERT(frame);
    frame -> address = frame_address;
    //frame -> frame_number = ((uint32_t) frame_address) >> 12; //Correct?
    //fix circular queue
    fix_queue(frame);
    //add to frame table
    //hash_insert(&frame_table->table,&frame->elem);
	}
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
  //Remove all old references to this frame by getting the pte and bitwise & with flags
  //to clear frame reference
  //An aux function would be helpful possibly in pagedir
  
	frame_table.head = head -> next;
	return head;
}
