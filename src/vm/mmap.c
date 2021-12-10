#include "vm/mmap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include <stdio.h>
#include "filesys/file.h"

struct list map_list;
struct lock map_list_lock;

void
mmap_init (void)
{
  list_init(&map_list);
  lock_init(&map_list_lock);
}

// Adds mapping of a file to memory
mapid_t 
mmap_add_mapping (int fd, int pgcnt, void *uaddr)
{
  static mapid_t next_id = 0;

  struct mapped_file *mapping = malloc(sizeof(struct mapped_file));
  ASSERT(mapping);

  mapping->mapid = next_id;
  mapping->fd = fd;
  mapping->pgcnt = pgcnt;
  mapping->uaddr = uaddr;

  lock_acquire(&map_list_lock);
  list_push_back(&map_list, &mapping->map_list_elem);
  lock_release(&map_list_lock);

  next_id++;
  return mapping->mapid;
}

// Removes mapping of file
bool
mmap_remove_mapping (mapid_t mapid)
{
  lock_acquire(&map_list_lock);
  struct list_elem *cur_elem = list_front(&map_list);
  lock_release(&map_list_lock);

  while (cur_elem)
  {
    struct mapped_file *mapping = list_entry(cur_elem, struct mapped_file, map_list_elem);
    ASSERT(mapping);

    if (mapping->mapid == mapid)
    {
      // TODO: use spt_removal_success to determine return value of mmap_remove_mapping
      bool spt_removal_success = spt_remove_mmap_file(mapping->uaddr);

      acquire_filesystem_lock();
      struct file *file = get_file_or_null(mapping->fd);
      uint32_t *pd = thread_current()->pagedir;
      ASSERT(file);
      file_seek(file, 0);
  
      for (int i = 0; i < mapping->pgcnt; i++) {
        void *pgaddr = mapping->uaddr + PGSIZE * i;
        if (pagedir_is_dirty(pd, pgaddr)) {
          file_write(file, pgaddr, file_length(file));
        } else {
          file_seek(file, file_tell(file) + PGSIZE);
        }

        // Removes frame
        palloc_free_page(pagedir_get_page(pd, pgaddr));
        // Removes mapping from user address to frame
        pagedir_clear_page(pd, pgaddr);
      }
      file_seek(file, 0);
      release_filesystem_lock();
    
      // Removes element from map_list
      lock_acquire(&map_list_lock);
      list_remove(cur_elem);
      lock_release(&map_list_lock);

      free(mapping);
      return true;
    }
    cur_elem = list_next(cur_elem);
  }
  PANIC("Can't find mapid in mapped files list");
}

// Remove and free all mapped_file structs from static map_list
void remove_all_mappings (void) {
  lock_acquire(&map_list_lock);

  while (!list_empty (&map_list)) {
    struct list_elem *e = list_pop_front (&map_list);
    ASSERT(e != NULL);
    struct mapped_file *mapped_file = list_entry(e, struct mapped_file, map_list_elem);
    ASSERT (mapped_file != NULL);
    free(mapped_file);
  }

  lock_release(&map_list_lock);
}