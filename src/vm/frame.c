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


struct frame
{
    //TODO: Missing page property, possibly just a uint32_t
    //TODO: Missing frame id, will be used for key of the table
    bool save;               //If 1 then frame is saved
    struct hash_elem *elem;  //Elem to be part of frame table
    struct frame *next;      //Pointer to be part of circular queue for eviction
};


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

struct frame *evict (struct frame *head){
    bool save = head -> save;
    if (head -> save){
        

    }

}