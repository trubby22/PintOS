#include "vm/page.h"
#include "lib/kernel/hash.h"
#include "userprog/syscall.h"
#include "lib/debug.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "lib/string.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "userprog/exception.h"

static bool pin_or_unpin_obj (void *uaddr, int size, pin_or_unpin_frame *);

bool
spt_contains_uaddr(void *upage)
{
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct list_elem *e;

  lock_acquire(&spt->pages_lock);

  // Searches pages for the one we want to remove
  for (e = list_begin (pages); e != list_end (pages); e = list_next (e)) {
    struct spt_page *spt_page = list_entry (e, struct spt_page, elem);

    if (spt_page->upage == upage) {
      lock_release(&spt->pages_lock);
      return true;
    }
  }
  lock_release(&spt->pages_lock);
  return false;
}

// Adds an entry in SPT when a file is mapped to memory
// This function must be called after filesystem_lock has been acquired
void spt_add_mmap_file (struct file *file, void *upage) {
  struct spt_page *spt_page = malloc(sizeof(struct spt_page));
  if (spt_page == NULL) {
    PANIC ("Could not malloc spt_page in page.c: spt_add_mmap_page");
  }

  // Variables passed as arguments to load_segment below
  uint32_t read_bytes = file_length (file);
  uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
  // It is assumed all mapped file pages are writable.
  bool writable = true;

  // Add metadata for each of the file's pages to spt
  load_segment(file, 0, upage, read_bytes, zero_bytes, writable, FILE);

  // Set file's read head back to 0
  file_seek (file, 0);
}

// Removes spt_page from list pages in SPT and deallocates spt_page
bool spt_remove_mmap_file (void *upage) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct list_elem *e;
  bool success = false;

  lock_acquire(&spt->pages_lock);

  // Searches pages for the one we want to remove
  for (e = list_begin (pages); e != list_end (pages); e = list_next (e)) {
    struct spt_page *spt_page = list_entry (e, struct spt_page, elem);

    if (spt_page->upage == upage) {
      list_remove(&spt_page->elem);
      free(spt_page);
      success = true;
      break;
    }

  }

  lock_release(&spt->pages_lock);
  return success;
}

// Adds an entry for a stack page to SPT
void spt_add_stack_page (void *upage) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct spt_page *spt_page = malloc(sizeof(struct spt_page));
  if (spt_page == NULL) {
    PANIC ("Could not malloc spt_page in page.c: spt_add_mmap_page");
  }

  spt_page->upage = upage;
  spt_page->type = STACK;
  spt_page->loaded = false;

  lock_acquire(&spt->pages_lock);
  list_push_back(pages, &spt_page->elem);
  lock_release(&spt->pages_lock);
}

// Creates a duplicate od spt_page and returns pointer to it. Used for sharing.
static struct spt_page *cpy_spt_page (struct spt_page *src) {
  struct spt_page *dest = malloc(sizeof(struct spt_page));
  if (dest == NULL) {
    PANIC ("Could not malloc spt_page in page.c: cpy_spt_page");
  }

  dest->type = src->type;
  dest->loaded = src->loaded;
  dest->file = src->file;
  
  dest->ofs = src->ofs;
  dest->upage = src->upage;
  dest->read_bytes = src->read_bytes;
  dest->zero_bytes = src->zero_bytes;
  dest->writable = src->writable;

  return dest;
}

// Copies non-stack parent's spt pages to child's spt. Later, sets up child's pagedir to map pages to the same frames as its parent. Finally, adds references to child in the frame itself via struct user_page.
void share_pages (struct thread *parent, struct thread *child) {
  struct spt *spt_parent = &parent->spt;
  struct spt *spt_child = &child->spt;

  struct list *parent_pages = &spt_parent->pages;
  struct list *child_pages = &spt_child->pages;

  struct list_elem *e;
  bool same_executable = strcmp(parent->name, child->name) == 0;

  lock_acquire(&spt_parent->pages_lock);

  for (e = list_begin (parent_pages); e != list_end (parent_pages); e = list_next (e)) {
    struct spt_page *parent_spt_page = list_entry (e, struct spt_page, elem);

    // Stack pages are not shared; if parent and child have different executables, don't copy metadata about it
    if (parent_spt_page->type == STACK ||
    (!same_executable && parent_spt_page->type == EXECUTABLE)) {
      continue;
    }

    struct spt_page *child_spt_page = cpy_spt_page(parent_spt_page);

    lock_acquire(&spt_child->pages_lock);
    list_push_back(child_pages, &child_spt_page->elem);
    lock_release(&spt_child->pages_lock);

    struct frametable *frame_table = get_frame_table();

    // Need to use a lock here to ensure frame's address doesn't change between call to pagedir_get_page and lookup_frame
    lock_acquire(&frame_table->lock);

    void *kpage = pagedir_get_page(parent->pagedir, child_spt_page->upage);
    // Adds mapping from page to kernel address
    install_page(child_spt_page->upage, kpage, child_spt_page->writable);
    struct frame *shared_frame = lookup_frame(kpage);

    lock_release(&frame_table->lock);

    struct user_page *user_page = malloc(sizeof(struct user_page));
    if (user_page == NULL) {
      PANIC ("Malloc failed");
    }

    user_page->pd = child->pagedir;
    user_page->uaddr = child_spt_page->upage;
    user_page->frame_or_swap_slot_ptr = shared_frame;
    user_page->used_in = SWAP;

    struct list *all_user_pages = get_all_user_pages();
    struct lock *all_user_pages_lock = get_all_user_pages_lock();

    lock_acquire(all_user_pages_lock);
    // Adds uer_page to all_user_pages. Useful for easy deallocation of user_page.
    list_push_back(all_user_pages, &user_page->allelem);
    lock_release(all_user_pages_lock);

    lock_acquire(&shared_frame->user_pages_lock);
    list_push_back(&shared_frame->user_pages, &user_page->elem);
    lock_release(&shared_frame->user_pages_lock);
  }

  lock_release(&spt_parent->pages_lock);
}

// Pins frames holding object. Returns true if at least one page has been pinned, false otherwise. Used for user memory access in syscall handler.
bool pin_obj (void *uaddr, int size) {
  int size_cpy = size;
  void *uaddr_cpy = uaddr;
  // Loads pages into memory if they have not been loaded yet and if the object does NOT live in stack (we can't load stack from file)
  for (size_cpy; size_cpy > 0; size_cpy -= PGSIZE) {
    attempt_load_pages(uaddr_cpy);
    uaddr_cpy += PGSIZE;
  }

  ASSERT (is_user_vaddr(uaddr));
  // Assuming the page is now either in frame or in swap, ensure it's in frame and pin this frame
  bool success = pin_or_unpin_obj(uaddr, size, pin_frame);
  return success;
}

bool unpin_obj (void *uaddr, int size) {
  ASSERT (is_user_vaddr(uaddr));
  bool success = pin_or_unpin_obj(uaddr, size, unpin_frame);
  return success;
}

// Helper function for pin_obj and unpin_obj
static bool pin_or_unpin_obj (void *uaddr, int size, pin_or_unpin_frame *pin_or_unpin_frame) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct list_elem *e;
  bool success = false;

  struct list *all_user_pages = get_all_user_pages();
  struct lock *all_user_pages_lock = get_all_user_pages_lock();

  lock_acquire(all_user_pages_lock);

  for (e = list_begin (all_user_pages); e != list_end (all_user_pages); e = list_next (e)) {
    struct user_page *user_page = list_entry (e, struct user_page, allelem);

    // Checks if object belongs to user_page
    bool contains_start = user_page->uaddr <= uaddr && uaddr < user_page->uaddr + PGSIZE;
    bool contains_mid = uaddr < user_page->uaddr && user_page->uaddr < uaddr + size;
    bool contains_end = user_page->uaddr <= uaddr + size && uaddr + size < user_page->uaddr + PGSIZE;

    if (contains_start || contains_mid || contains_end) {
      struct frametable *frame_table = get_frame_table();
      
      // Need to acquire lock to make sure that frame is not evicted between the time that it's swapped in to RAM and the time that it's pinned.
      lock_acquire(&frame_table->lock);

      // For pinning: if page in swap_slot, first swap it back in to a frame in RAM. Regardless whether page was in swap or already in frame, we get back the kernel address of the frame.
      // For unpinning it's assumed that the page is already in the frame in RAM since it's pinned. The frame cannot be removed during the syscall because the running process is one of its owners.
      void *kpage = pagedir_get_page(t->pagedir, user_page->uaddr);
      success = pin_or_unpin_frame(kpage);

      lock_release(&frame_table->lock);
    }
  }

  lock_release(all_user_pages_lock);

  return success;
}

// Frees spt_pages of process when process exits. Used in pagedir_destroy.
void free_process_spt (void) {
  struct thread *cur = thread_current();
  struct spt *spt = &cur->spt;
  struct list *pages = &spt->pages;

  lock_acquire(&spt->pages_lock);

  while (!list_empty (pages)) {
    struct list_elem *e = list_pop_front (pages);
    struct spt_page *spt_page = list_entry(e, struct spt_page, elem);
    free(spt_page);
  }

  lock_release(&spt->pages_lock);
}

