#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "lib/fixed-point.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

enum comparator
{
  LESS,
  LESSEQ,
  EQUALS,
  MOREEQ,
  MORE
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
#define NICE_MIN -20                    /* Lowest niceness. */
#define NICE_MAX 20                    /* Highest niceness. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int effective_priority;             /* Priority after donations */
    int nice;                           /* Thread's nice value */
    fp32_t recent_cpu;                  /* Thread's recent cpu value */
    struct list received_priorities;    /* List of received priorities */
    struct list recepiant_threads;      /* List of child threads*/
    struct list_elem recepiant_elem;    /* List element for recepiant_threads */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "mlfqs". */
extern bool thread_mlfqs;

// Struct used for stroing priority donations in lists
struct priority_donation
{
  struct list_elem thread_elem; /* For adding to list in struct thread */
  struct list_elem lock_elem; /* For adding to list in struct lock */
  int priority; /* Stores donated priority */
};

void thread_init (void);
void thread_start (void);
size_t threads_ready(void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

bool compare_threads(const struct list_elem *a, 
                     const struct list_elem *b, void *type);
void priority_list_add(struct list* priority_list,struct thread *t);
struct thread *priority_list_head(struct list *priority_list);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

void thread_donate_priority(struct thread *, struct lock *);
void thread_donate_priority_children (struct thread *sender,struct thread *target, 
                                      struct lock *lock, int new_priority);
void thread_give_back_priority (struct lock *);

int thread_get_priority(void);
void thread_set_priority (int);
void thread_calculate_priority (struct thread *);

// Functions used for BSD-style scheduler
int thread_get_nice (void);
void thread_set_nice (int);
fp32_t thread_fp_get_recent_cpu (void);
int thread_get_recent_cpu (void);
void thread_set_recent_cpu (fp32_t);
void thread_increment_recent_cpu (void);
void thread_update_recent_cpu (struct thread *);
void thread_update_all_recent_cpus (void);
int thread_get_load_avg (void);
void thread_update_load_avg (void);
void thread_update_priority (struct thread *);
void thread_update_all_priorities (void);


#endif /* threads/thread.h */
