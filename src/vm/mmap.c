#include "vm/mmap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include <stdio.h>

struct list map_list;

void
mmap_init (void)
{
  list_init(&map_list);
}

mapid_t 
mmap_add_mapping (int fd, int pgcnt, void *uaddr, void *kaddr)
{
  static mapid_t next_id = 0;

  struct mapped_file *mapping = malloc(sizeof(struct mapped_file));
  ASSERT(mapping);

  mapping->mapid = next_id;
  mapping->fd = fd;
  mapping->pgcnt = pgcnt;
  mapping->uaddr = uaddr;
  mapping->kaddr = kaddr;

  list_push_back(&map_list, &mapping->map_list_elem);

  next_id++;
  // printf("made mapping with id: %u\n", mapping->mapid);
  return mapping->mapid;
}

bool
mmap_remove_mapping (mapid_t mapid)
{
  // printf("REMOVING mapping with id: %u\n", mapid);
  struct list_elem *cur_elem = list_front(&map_list);
  // return -1;
  while (cur_elem)
  {
    struct mapped_file *mapping = list_entry(cur_elem, struct mapped_file, map_list_elem);
    ASSERT(mapping);
    // printf("Mapping: %u\n", mapping->mapid);
    if (mapping->mapid == mapid)
    {

      // TODO: use spt_removal_success to determine return value of mmap_remove_mapping
      bool spt_removal_success = spt_remove_mmap_file(mapping->uaddr);

      for (int i = 0; i < mapping->pgcnt; i++) {
        ASSERT(pagedir_get_page(thread_current()->pagedir, mapping->uaddr + PGSIZE * i));
        pagedir_clear_page(thread_current()->pagedir, mapping->uaddr + PGSIZE * i);
        return -1;
      }
      list_remove(cur_elem);
      free(mapping);
      return true;
    }
    cur_elem = list_next(cur_elem);
  }
  return false;
}