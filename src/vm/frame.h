#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <inttypes.h>

void* frame_insert (void *kpage, uint32_t *pd, void *vaddr);
struct frame *lookup_frame(void *vaddr);
void init_frame_table(void);

#endif