#include "vm/page.h"
#include "lib/kernel/hash.h"

// unsigned seg_hash_func (const struct hash_elem *e, void *aux) {
//   struct segment *seg = hash_entry(e, struct segment, elem);
//   return seg->start_addr;
// }

// bool seg_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux) {
//   return seg_hash_func(a, NULL) < seg_hash_func(b, NULL);
// }

