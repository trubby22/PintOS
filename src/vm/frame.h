#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <inttypes.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"

// TODO: add a lock on struct frame

/* A frame table maps a frame to a user page. */
struct frametable
{
  struct frame *head; //Head of circular queue, needed for eviction
  struct hash table;

  // Might be too coarse-grained. Would be better to lock individual frames.
  struct lock lock;
};

struct frame
{
  // What are the keys and what are the values? I mean when struct frame is used inside hash table

  uint32_t frame_number;
  void* address;           //value

  uint32_t *pd;            // page directory of the process that owns this frame
  void* uaddr;             // corresponding page of the proces that owns this frame
  int size;                //if added through frame multiple

  struct hash_elem elem;   //Elem to be part of frame table
  struct frame *next;      //Pointer to be part of circular queue for eviction

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

void pin_frame (void *address);
void unpin_frame (void *address);

#endif