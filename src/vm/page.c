#include "vm/page.h"
#include "lib/kernel/bitmap.h"
#include "threads/thread.h"

static struct supp_page_table sp_table;

void init_supp_page_table(void) {
  hash_init(&sp_table.table, sptable_hash_func, sptable_less_func, NULL);
  sp_table.bitmap = bitmap_create(MAX_SUPP_PAGE_TABLE_SIZE);
}

// Converts virtual address to physical address. If the virtual address is invalid, causes a PF.
void *convert_virtual_to_physical(void *vaddr) {
  uint32_t int_vaddr = (uint32_t) vaddr;

  // Gets first 20 bits of virtual address
  uint32_t page_number = int_vaddr >> 12;

  // Gets last 12 bits of virtual address
  uint32_t offset = int_vaddr & 0xfff;

  // Assuming pid = tid
  pid_t pid = thread_current()->tid;

  // Searches the page table for the provided vaddr
  struct page dummy;

  dummy.pid = pid;
  dummy.page_number = page_number;
  
  struct hash_elem *target_elem = hash_find(&sp_table.table, &dummy.elem);

  uint32_t frame_number;
  if (target_elem == NULL) {
    // TODO: I'm waiting for get_frame_number to be implemented
    // Desired function signature:
    // uin32_t get_frame_number(pid_t pid, uint32_t page_number)
    frame_number = get_frame_number(pid, page_number);
  } else {
    struct page *target_page = hash_entry(target_elem, struct page, elem);

    // Checks page's "valid bit". Terminates process if invalid.
    if (!target_page->valid) {
      exit_userprog(-1, NULL, NULL);
    }

    struct frame *target_frame = target_page->frame_ptr;

    // TODO: I am still waiting until struct frame gets completed
    // frame_number = target_frame->frame_number;
    frame_number = 0;
  }

  uint32_t int_paddr = frame_number << 12 + offset;

  // Expects address to be in user memory
  validate_user_pointer((uint32_t *) int_paddr);

  return (void *) int_paddr;
}

void free_process_pages(pid_t pid) {
  // TODO
}

unsigned sptable_hash_func (const struct hash_elem *e, void *aux) {
  struct page *page = hash_entry(e, struct page, elem);
  return hash_int(page->pid) ^ hash_int(page->page_number);
}

bool sptable_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux) {
  struct page *page_a = hash_entry(a, struct page, elem);
  struct page *page_b = hash_entry(b, struct page, elem);
  return (uint32_t) page_a->frame_ptr < (uint32_t) page_b->frame_ptr;
}
