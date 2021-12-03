#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <inttypes.h>
void* get_frame (uint32_t *pd, void *vaddr);
uint32_t lookup_frame(uint32_t frame_number);
void init_frame_table(void);

#endif