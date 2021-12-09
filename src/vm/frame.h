#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <inttypes.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include <hash.h>

// TODO: add a lock on struct frame

/* A frame table maps a frame to a user page. */
struct frametable
{
  struct list list;   //List for cicular queue of elements
  struct hash table;  //Hash table fot looking up elements
  struct list_elem *current;

  // Might be too coarse-grained. Would be better to lock individual frames.
  struct lock lock;
};

struct frame
{
  // What are the keys and what are the values? I mean when struct frame is used inside hash table

  void* address;           //key

  uint32_t *pd;            // page directory of the process that owns this frame
  void* uaddr;             // corresponding page of the proces that owns this frame
  int size;                // NOT USED

  struct hash_elem elem;   //Elem to be part of frame table
  struct list_elem list_elem;

  // Possibly redundant; might have a list of struct pd_uaddr so as to be able to access only the pagedir and user address of each process that has access to frame
  struct list children;    // List of spts of child processes that the frame is shared with

  // Lock on list children
  struct lock children_lock;

  // Number of pages with mapping to this frame
  int users;

  // Lock on the whole struct frame. Idk yet whether we need it.
  struct lock lock;

  // Used for "pinning" frame in RAM so it cannot be evicted when a syscall happens. Relates to accessing user memory.
  bool pinned;
};

void* frame_insert (void *kpage, uint32_t *pd, void *vaddr, int size);
struct frame *lookup_frame(void *kpage);
void init_frame_table(void);
struct frame *find_frame (void *address);
void *get_frame (uint32_t *pd, void *vaddr);

struct frametable *get_frame_table (void);
void free_frames(void* pages, size_t page_cnt);

void pin_frame (void *address);
void unpin_frame (void *address);

void reset_all_accessed_bits(void);
void reset_accessed_bits (struct hash_elem *e, void *aux);

#endif