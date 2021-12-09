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

// TODO: extract parts of load_segment and load_page into this class

// This function must be called after filesystem_lock has been acquired
// TODO: don't panic here. Instead, return false and handle that in the caller
void spt_add_mmap_file (int fd, void *upage) {
  struct file *file = get_file_or_null(fd);
  if (file == NULL) {
    PANIC ("Could not find file for given fd in page.c: spt_add_mmap_page");
  }

  // TODO: remember to free spt_page
  struct spt_page *spt_page = malloc(sizeof(struct spt_page));
  if (spt_page == NULL) {
    PANIC ("Could not malloc spt_page in page.c: spt_add_mmap_page");
  }

  // Variables passed as arguments to load_segment below
  uint32_t read_bytes = file_length (file);
  uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
  // It is assumed all mapped file pages are writable.
  bool writable = true;
  bool is_executable = false;

  // Add metadata for each of the file's pages to spt
  load_segment(file, 0, upage, read_bytes, zero_bytes, writable, is_executable);

  // Set file's read head back to 0
  file_seek (file, 0);
}

// Removes spt_page from pages and deallocates spt_page
bool spt_remove_mmap_file (void *upage) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct list_elem *e;
  bool success = false;

  lock_acquire(&spt->pages_lock);

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

// Do I even need to record stack pages in SPT?
void spt_add_stack_page (void *upage) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  // TODO: remember to free spt_page
  struct spt_page *spt_page = malloc(sizeof(struct spt_page));
  if (spt_page == NULL) {
    PANIC ("Could not malloc spt_page in page.c: spt_add_mmap_page");
  }

  spt_page->upage = upage;
  spt_page->type = STACK;
  spt_page->thread = thread_current();

  lock_acquire(&spt->pages_lock);
  list_push_back(pages, &spt_page->elem);
  lock_release(&spt->pages_lock);
}

// Creates a duplicate od spt_page and returns pointer to it
static struct spt_page *cpy_spt_page (struct spt_page *src) {
  // TODO: remember to free spt_page
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

// Copies non-stack parent's spt pages to child's spt
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

    // Stack pages are not shared
    
    if (parent_spt_page->type == STACK ||
    !same_executable && parent_spt_page->type == EXECUTABLE) {
      continue;
    }

    struct spt_page *child_spt_page = cpy_spt_page(parent_spt_page);
    child_spt_page->thread = child;

    lock_acquire(&spt_child->pages_lock);
    list_push_back(&child_pages, child_spt_page);
    lock_release(&spt_child->pages_lock);

    // TODO: avoid race conditions here (esp. when adding stuff to shared_frame)
    // Add mapping from page to kernel address

    struct frametable *frame_table = get_frame_table();

    // Need to use a lock here to ensure frame's address doesn't change between call to pagedir_get_page and lookup_frame
    lock_acquire(&frame_table->lock);

    void *kpage = pagedir_get_page(parent->pagedir, child_spt_page->upage);
    install_page(child_spt_page->upage, kpage, child_spt_page->writable);
    struct frame *shared_frame = lookup_frame(kpage);

    lock_release(&frame_table->lock);

    // Need to have a lock here because a list is used and it may be used by many children of the same parent at once.
    lock_acquire(&shared_frame->user_pages_lock);

    // TODO: remember to free
    struct user_page *user_page = malloc(sizeof(struct user_page));
    if (user_page == NULL) {
      PANIC ("Malloc failed");
    }

    user_page->pd = child->pagedir;
    user_page->uaddr = child_spt_page->upage;

    list_push_back(&shared_frame->user_pages, &user_page->elem);

    lock_release(&shared_frame->user_pages_lock);
  }

  lock_release(&spt_parent->pages_lock);
}

// TODO: method init_spt_page that sets up spt_page with default values

// Assuming the address that we get in syscall handler is user address. If it turns out that it's a kernel address, pinning could be done in a simpler manner using frame table directly.
// Pins frames holding object. Returns true if at least one page has been pinned, false otherwise.
bool pin_obj (void *uaddr, int size) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct list_elem *e;
  bool success = false;

  lock_acquire(&spt->pages_lock);

  for (e = list_begin (pages); e != list_end (pages); e = list_next (e)) {
    struct spt_page *spt_page = list_entry (e, struct spt_page, elem);

    // Checks if object belongs to spt_page
    if ((spt_page->upage <= uaddr && uaddr < spt_page->upage + PGSIZE) ||
    (uaddr < spt_page->upage && spt_page->upage < uaddr + size) ||
    (spt_page->upage <= uaddr + size && uaddr + size < spt_page->upage + PGSIZE)) {
      void *kpage = pagedir_get_page(t->pagedir, spt_page->upage);
      pin_frame(kpage);
      success = true;
    }

  }

  lock_release(&spt->pages_lock);
  return success;
}

// TODO: reduce code duplication between pin_obj and unpin_obj
bool unpin_obj (void *uaddr, int size) {
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  struct list_elem *e;
  bool success = false;

  lock_acquire(&spt->pages_lock);

  for (e = list_begin (pages); e != list_end (pages); e = list_next (e)) {
    struct spt_page *spt_page = list_entry (e, struct spt_page, elem);

    // Checks if object belongs to spt_page
    if ((spt_page->upage <= uaddr && uaddr < spt_page->upage + PGSIZE) ||
    (uaddr < spt_page->upage && spt_page->upage < uaddr + size) ||
    (spt_page->upage <= uaddr + size && uaddr + size < spt_page->upage + PGSIZE)) {
      void *kpage = pagedir_get_page(t->pagedir, spt_page->upage);
      unpin_frame(kpage);
      success = true;
    }

  }

  lock_release(&spt->pages_lock);
  return success;
}

