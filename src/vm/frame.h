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

  // Lock on frametable
  struct lock lock;
};

struct frame
{
  void* address;           //key

  uint32_t id;

  uint32_t *pd;            // page directory of the process that owns this frame
  void* uaddr;             // corresponding page of the proces that owns this frame
  int size;                // NOT USED

  struct hash_elem elem;   //Elem to be part of frame table
  struct list_elem list_elem;

  // Used for "pinning" frame in RAM so it cannot be evicted when a syscall happens. Relates to accessing user memory.
  bool pinned;

  // Lock on frame.
  struct lock lock;

  // Sharing

  // Holds pages that map to this frame
  struct list user_pages;
  // Lock for list user_pages
  struct lock user_pages_lock;
};

// Holds data about page that is mapped to frame. Is needed for sharing.
struct user_page {
  // Page directory
  uint32_t *pd;
  // User address
  void *uaddr;
  // Shared elem for adding to list users_list in frame and in swap_slot
  struct list_elem elem;
  // Elem used for adding to static list user_pages (defined in frame.c)
  struct list_elem allelem;
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

void remove_user_page (void *kpage, void *pd);

void remove_all_frames (void);

#endif