#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "lib/user/syscall.h"
#include <list.h>

// Map a mapid_t to a struct mapped_file

struct mapped_file {
    mapid_t mapid;                  /* Map id associated with the mapping */
    int fd;                         /* File descriptor of the file opened */
    int pgcnt;                      /* Number of continuous pages */
    void *uaddr;                    /* Address given in syscall */
    struct list_elem map_list_elem; /* Element in the list of mappings */
};

void mmap_init(void);
mapid_t mmap_add_mapping(int fd, int pgcnt, void *uaddr);
bool mmap_remove_mapping(mapid_t mapid);

#endif