#include "vm/page.h"
#include "lib/kernel/bitmap.h"

static struct supp_page_table sp_table;

void init_supp_page_table(void) {
  hash_init(&sp_table.table, sptable_hash_func, sptable_less_func, NULL);
  sp_table.bitmap = bitmap_create(MAX_SUPP_PAGE_TABLE_SIZE);
}

// Converts virtual address to physical address. If the virtual address is invalid, causes a PF.
void *convert_virtual_to_physical(void *vaddr) {

}

unsigned sptable_hash_func (const struct hash_elem *e, void *aux) {

}

bool sptable_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux) {

}