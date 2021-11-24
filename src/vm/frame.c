/* A frame table maps a frame to a user page. */

/* 
Contains:
    - a pointer to a page
    - additional information

Allows us to implement an eviction policy.

Eviction policy is used when no frames are free.
*/

// TODO: Use palloc_get_page(PAL_USER) to create a frame for a user page.
// TODO: Add function to obtain an unused frame.
// TODO: Implement frame freeing via an eviction policy.

/* If no frame can be evicted without allocating a swap slot & swap is full, kernel panic */

/* 
Eviction policy
    - Chose a frame to evict
    - Remove all references to this frame from all page tables
    - If necessary, write the page to the file system or to swap
*/