#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <inttypes.h>
#include "vm/frame.h"

void init_swap_table(void);
bool read_swap_slot(uint32_t *pd, void* vadrr, void* kpage);
bool add_swap_slot(struct frame* frame);

#endif