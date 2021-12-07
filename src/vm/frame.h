#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <inttypes.h>
#include <hash.h>

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


void* frame_insert (void *kpage, uint32_t *pd, void *vaddr, int size);
struct frame *lookup_frame(void *kpage);
void init_frame_table(void);

#endif