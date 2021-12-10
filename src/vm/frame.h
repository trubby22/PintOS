#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <inttypes.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include <hash.h>

typedef void pagedir_generic_function (uint32_t *pd, void *vpage);

// Represents struct that user_page is pointing to
enum used_in {
  FRAME,
  SWAP
};

/* A frame table maps a frame to a user page. */
struct frametable
{
  struct list list;   //List for cicular queue of elements
  struct hash table;  //Hash table fot looking up elements
  struct list_elem *current;
  // Lock on frametable
  struct lock lock;
};

// Stores information about a frame
struct frame
{
  void* address;           //key
  // Not used
  int size;

  struct hash_elem elem;   //Elem to be part of frame table
  // For putting frame in frametable.list
  struct list_elem list_elem;

  // Used for "pinning" frame in RAM so it cannot be evicted when a syscall happens. Relates to accessing user memory.
  bool pinned;

  // Sharing

  // Holds pages that map to this frame
  struct list user_pages;
  // Lock for list user_pages
  struct lock user_pages_lock;
};

// Holds data about page that is mapped to frame. Is needed for sharing.
// Note: due to extensive use of struct user_page it would be significantly better if lists holding user_pages were hashmaps instead with the following key: (uint32_t *pd, void *uaddr)
struct user_page {
  // Page directory
  uint32_t *pd;
  // User address
  void *uaddr;
  // Shared elem for adding to list users_list in frame and in swap_slot
  struct list_elem elem;
  // Elem used for adding to static list user_pages (defined in frame.c)
  struct list_elem allelem;
  // Parent struct; either frame or swap
  enum used_in used_in;
  // Pointer to the struct to which user_page belongs, so either frame or swap_slot. Useful for sharing.
  void *frame_or_swap_slot_ptr;
};

void* frame_insert (void *kpage, uint32_t *pd, void *vaddr, int size);
struct frame *lookup_frame(void *kpage);
void init_frame_table(void);
struct frame *find_frame (void *address);
void *get_frame (uint32_t *pd, void *vaddr);

struct frametable *get_frame_table (void);
struct list *get_all_user_pages (void);
struct lock *get_all_user_pages_lock (void);

bool pin_frame (void *address);
bool unpin_frame (void *address);

void reset_all_accessed_bits(void);
void reset_accessed_bits (struct hash_elem *e, void *aux);

void remove_user_page (void *kpage, void *pd);
void remove_all_frames (void);
void free_frames(void* pages, size_t page_cnt);

#endif