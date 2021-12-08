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

void
mmap_init (void)
{
  list_init(&map_list);
}

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

  list_push_back(&map_list, &mapping->map_list_elem);

  next_id++;
  return mapping->mapid;
}

bool
mmap_remove_mapping (mapid_t mapid)
{
  struct list_elem *cur_elem = list_front(&map_list);
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
      off_t original_pos = file->pos;
      file_seek(file, 0);
      
      for (int i = 0; i < mapping->pgcnt; i++) {
        void *pgaddr = mapping->uaddr + PGSIZE * i;
        if (pagedir_is_dirty(pd, pgaddr))
          file_write(file, pgaddr, file_length(file));

        palloc_free_page(pagedir_get_page(pd, pgaddr));
        pagedir_clear_page(pd, pgaddr);
      }
      file_seek(file, 0);
      release_filesystem_lock();
    
      list_remove(cur_elem);
      free(mapping);
      return true;
    }
    cur_elem = list_next(cur_elem);
  }
  return false;
}