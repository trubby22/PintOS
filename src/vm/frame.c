#include <stdbool.h>
#include <hash.h>

#define MAX_FRAME_TABLE_SIZE 100 //If met evictions are needed

/* A frame table maps a frame to a user page. */
struct frame_table
{
  int size;           //Current size of the frametable, if met evictions are needed on an add
  struct frame *head; //Head of circular queue, needed for eviction
  struct hash table;  //
};

//Should a frame table map from frame_ids -> pages
//or                            page_ids  -> frames?
struct frame
{
  //TODO: Missing page property, possibly just a uint32_t
  //TODO: Missing frame id, will be used for key of the table
  bool save;               //If 1 then frame is saved
  struct hash_elem *elem;  //Elem to be part of frame table
  struct frame *next;      //Pointer to be part of circular queue for eviction
};

struct frame_table *frame_table;
frame_table->head  = NULL;
frame_table->size  = 0;
hash_init(frame_table->table, hash_hash_func, hash_less_func);

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

void* get_frame (uint32_t frameId){
	//Create dummy frame to seach frame table
  //Search frame table
	//if something is returned return it and set save bit to 1
	//else do below
	if (frame_table.size == MAX_FRAME_TABLE_SIZE)
	{
		struct frame *frame = evict (frame_table.head);
	} else{
		//use palloc user page to create a new frame
		struct frame *head = frame_table -> head;
		frame -> next  = head -> next;
		head -> next = frame;
		frame_table -> head = frame -> next;
	}
	
}

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