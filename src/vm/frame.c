#include "vm/frame.h"
#include <stdbool.h>
#include <hash.h>
#include <palloc.h>
#include <debug.h>

#define MAX_FRAME_TABLE_SIZE 100 //If met evictions are needed
#define frameid int

/* A frame table maps a frame to a user page. */
struct frametable
{
  int size;           //Current size of the frametable, if met evictions are needed on an add
  struct frame *head; //Head of circular queue, needed for eviction
  struct hash table;  //
};

struct frame
{
  frameid id;              //key
  void* address;           //value?
  //Needs to track owner?
  bool save;               //If 1 then frame is saved
  struct hash_elem elem;  //Elem to be part of frame table
  struct frame *next;      //Pointer to be part of circular queue for eviction
};

unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *f = hash_entry(e, frame, elem);
  return f -> id;
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  return frame_hash(a,NULL) < frame_hash(b, NULL);

}

struct frametable *frame_table;
frame_table->head  = NULL;
frame_table->size  = 0;
hash_init(frame_table->table, frame_hash, frame_less);

/* 
Contains:
  - a pointer to a page
  - additional information

Allows us to implement an eviction policy.

Eviction policy is used when no frames are free.
*/

// TODO: Use palloc_get_page(PAL_USER) to create a frame for a user page.
// TODO: Add function to obtain an unused frame.
// TODO: Implement frame freeing via an eviction policy.

/* If no frame can be evicted without allocating a swap slot & swap is full, kernel panic */

/* 
Eviction policy
  - Chose a frame to evict
  - Remove all references to this frame from all page tables
  - If necessary, write the page to the file system or to swap
*/

/* Looks up frame with frameid FID in the frame table 
   returns NULL if the frame does not exist */
uint32_t lookup_frame(frameid fid){
  //search table using dummy elem
  struct hash_elem *dummy_f;
  dummy_f -> id = fid;
  struct hash_elem *f = hash_find(frame_table, dummy_f);
  //miss
  if (!f){
    return NULL;
  }
  //hit
  struct frame *frame = hash_entry(f, struct frame, elem);
  f -> save = 1;
  return f -> address;
}

/* Returns a new frame. Evicts if needed */
frameid get_frame (){
  //New frames should be malloc'd
  struct frame* frame;
	if (frame_table -> size == MAX_FRAME_TABLE_SIZE)
	{
    //Should call frame = evict (frame_table -> head); but will just panic for now
		PANIC("Ran out of free frames");
	} else{
		//use palloc user page to create a new frame
    frame -> address = palloc_get_page(PAL_USER);
    //fix circular queue
		struct frame *head = frame_table -> head;
		frame -> next  = head -> next;
		head -> next = frame;
		frame_table -> head = frame -> next;
    //assign frame id and fix table size
    frame_table -> size++;
    frame -> id = frame_table -> size;
	}
	return frame -> id;
}

/* Implements a second chance eviction algorithm
   will allocate a swap slot if needed */
static struct frame *evict (struct frame *head){
  bool save = head->save;
  if (save){
    head -> save = 0;
    return evict(head->next);
  }
	//Allocate swap slot
	frame_table -> head = head -> next;
	return head;
}