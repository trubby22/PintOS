#include "vm/frame.h"
#include <stdbool.h>
#include <hash.h>
#include "threads/palloc.h"
#include <debug.h>
#include <inttypes.h>
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"

static struct frametable frame_table;

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

void init_frame_table(void)
{
  struct hash table;
  frame_table.table = table;
  hash_init(&table, frame_hash, frame_less, NULL);
}

/* Looks up frame with frameid FID in the frame table 
   returns NULL if the frame does not exist */
   //Is this function even needed?
// What's the point of looking up on frame number when we hash on frame address? Frame number is really useful for tracking a frame when it changes its location (RAM, disk, etc.) but I think lookup should be on the same variable that we use in frame_hash
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

// Returns pointer to struct frame given frame_number
struct frame *find_frame (void *address) {
  //search table using dummy elem
  struct frame dummy;
  dummy.address = address;

  struct hash_elem *f = hash_find(&frame_table.table, &dummy.elem);

  //miss
  if (!f){
    return NULL;
  }

  //hit
  struct frame *frame = hash_entry(f, struct frame, elem);
  return frame;
}


/* Returns a new frame. Evicts if needed */
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
    //fix circular queue
    fix_queue(frame);
    //add to frame table
    //hash_insert(&frame_table.table,&frame->elem);
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
 
  //Removes the refernce to this frame in the page table entry
  uint32_t *pte = get_pte(pd, uaddr);
  *pte = *pte & PTE_FLAGS;
  
	frame_table.head = head -> next;
	return head;
}

struct frametable *get_frame_table (void) {
  return &frame_table;
}
