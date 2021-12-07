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
  uint32_t *pd;            //page directory that owns this frame
  void* uaddr;             //page that owns this frame
  bool save;               //If 1 then frame is saved, not needed
  struct hash_elem elem;   //Elem to be part of frame table
  struct frame *next;      //Pointer to be part of circular queue for eviction

  struct list children;    // List of spts of child processes that the frame is shared with

  // Lock on list children
  struct lock children_lock;

  // Lock on the whole struct frame. Idk yet whether we need it.
  struct lock lock;
};

void init_frame_table(void);
uint32_t lookup_frame(uint32_t frame_number);
struct frame *find_frame (void *address);
void *get_frame (uint32_t *pd, void *vaddr);

struct frametable *get_frame_table (void);

#endif