#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "lib/fixed-point.h"
#include "threads/malloc.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-mlfqs". */
bool thread_mlfqs;

// System load average
static fp32_t load_avg;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
bool list_less_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

struct thread *priority_list_head(struct list *priority_list) {
  return list_entry(list_front(priority_list), struct thread, elem);
}

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  if (thread_mlfqs) {
    load_avg = 0;
  }

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Returns the number of threads currently in the ready list */
size_t
threads_ready (void)
{
  return list_size (&ready_list);      
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  if (thread_mlfqs) {
    /* Sets nice value of child to that of the parent. */
    t->nice = (thread_current()->nice);

    /* Sets recent_cpu of child to that of the parent. */
    t->recent_cpu = (thread_current()->recent_cpu);

    // Recalculates thread priority since we're in a child thread
    thread_update_priority(t);
  }

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);
  thread_yield();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  priority_list_add(&ready_list, t);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Adds a thread to the ready list, sorting it based on it's priority.
   Interrupts are turned off entering this function. */
void
priority_list_add(struct list *priority_list, struct thread *t)
{
  enum comparator more = MORE;
  list_insert_ordered (priority_list, &t->elem, &compare_threads, &more);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    priority_list_add(&ready_list, cur);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Donates the current thread's effective_priority to the thread given in the function call. */
void
thread_donate_priority (struct thread *target, struct lock *lock) 
{
  int new_priority = thread_current ()->effective_priority;

  if (new_priority <= target->effective_priority) {
    return;
  }
  struct priority_donation *donation = malloc(sizeof(struct priority_donation));
  // TODO: add a free for donation
  donation->priority = new_priority;

  list_insert_ordered(&target->received_priorities, &donation->thread_elem, &list_less_priority, NULL);
  list_push_back(&lock->donated_priorities, &donation->lock_elem);

  // Calculates thread's new effective priority after the donation
  thread_calculate_priority(target);
  thread_yield();
}

// Gives back priorities associated with a lock
void thread_give_back_priority (struct lock *lock)
{
  struct thread *t = thread_current();
  struct list *lock_priorities = &lock->donated_priorities;
  
  struct list_elem *e;
  
  if (!list_empty(lock_priorities)) {
    // Removes priority_donation from the list in struct thread and in struct lock
    // problematic line of code below - causes PF exception; is there something wrong with the lock_priorities list?
    for (e = list_begin (lock_priorities); e != list_end (lock_priorities); e = list_remove (e))
    {
      struct priority_donation *pd = list_entry (e, struct priority_donation, lock_elem);
      // problematic line of code below - causes PF exception
      list_remove(&pd->thread_elem);
    }

    // Calculates thread's new effective priority after giving back the donation
    thread_calculate_priority(t);
    thread_yield();
  }
}

// Compares thread_elems of struct priority_donation on their priorities
bool list_less_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct priority_donation *pda = list_entry(a, struct priority_donation, thread_elem);
  struct priority_donation *pdb = list_entry(b, struct priority_donation, thread_elem);

  return pda->priority < pdb->priority;
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  struct thread *t = thread_current();
  t->priority = new_priority;
  enum comparator c = MORE;
  list_sort(&ready_list, &compare_threads, &c);
  // Isn't it redundant?
  thread_calculate_priority(t);
  thread_yield();
}

/* Returns the current thread's effective priority. */
int
thread_get_priority (void) 
{
  struct thread *t = thread_current ();

  if(t->priority > t->effective_priority)
    return t->priority;
  return t->effective_priority;
}

/*Compares threads based on their priority. type determines what comapartor is used */
bool 
compare_threads(const struct list_elem *a, const struct list_elem *b, void *type)
{
  enum comparator t = *((enum comparator*) type);
  int8_t a_priority = list_entry(a, struct thread, elem)->effective_priority;
  int8_t b_priority = list_entry(b, struct thread, elem)->effective_priority;

  switch (t)
  {
    case LESS:
      return a_priority < b_priority;
    
    case LESSEQ:
      return a_priority <= b_priority;

    case EQUALS:
      return a_priority == b_priority;

    case MOREEQ:
      return a_priority >= b_priority;

    case MORE:
      return a_priority > b_priority;

    default:
      return false;    
  }
}

// Calculates thread's effective_priority based on the contents of the received_priorities list and the base priority; then yields thread
void thread_calculate_priority (struct thread *t)
{
  struct list priorities = t->received_priorities;
  int max_priority = 0;
  if (!list_empty(&priorities)) {
    // Sets the effective_priority to the max elem of priorities list
    struct list_elem *pri_elem = list_back(&priorities);
    struct priority_donation *pd = list_entry(pri_elem, struct priority_donation, thread_elem);
    max_priority = pd->priority;
  }

  // Sets effective priority to the higher of: (1) max of received priorities, (2) base priority
  if (max_priority > t->priority) {
    t->effective_priority = max_priority;
  } else {
    t->effective_priority = t->priority;
  }
}

/* Sets the current thread's nice value to new_nice. */
void
thread_set_nice (int new_nice) 
{
  struct thread *t = thread_current();
  if (new_nice < NICE_MIN) {
    new_nice = NICE_MIN;
  } else if (new_nice > NICE_MAX) {
    new_nice = NICE_MAX;
  }
  bool same = (t->nice == new_nice);

  if (!same) {
    t->nice = new_nice;
    thread_update_priority(t);
    thread_yield();
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  ASSERT(thread_mlfqs);

  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  ASSERT(thread_mlfqs);

  return convert_integer_n(multiply_int(load_avg, 100));
}

/* Updates the system load average. */
void
thread_update_load_avg (void) 
{
  ASSERT(thread_mlfqs);

  // Calculates the first addend of the equation for load_avg
  fp32_t fst_addend = multiply_fp(load_avg, divide_int(convert_fp(59), 60));

  // Determines whether a thread is currently running
  int running_threads;
  if (strcmp(thread_name(), "idle") != 0) {
    running_threads = 1;
  } else {
    running_threads = 0;
  }

  // Calculates the second addend of the equation for load_avg
  fp32_t snd_addend = convert_fp((int) threads_ready() + running_threads);
  snd_addend = divide_int(snd_addend, 60);
  
  // Updates load_avg
  load_avg = add_fp(fst_addend, snd_addend);
}

/* Returns the current thread's recent_cpu value in a fixed-point format. */
fp32_t thread_fp_get_recent_cpu (void) {
  ASSERT(thread_mlfqs);
  
  return thread_current()->recent_cpu;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  ASSERT(thread_mlfqs);
  
  return convert_integer_n(multiply_int(thread_fp_get_recent_cpu(), 100));
}

/* Sets the thread's recent_cpu value. */
void
thread_set_recent_cpu (fp32_t new_recent_cpu) 
{
  ASSERT(thread_mlfqs);
  
  thread_current()->recent_cpu = new_recent_cpu;
}

/* Increments the thread's recent_cpu value. */
void thread_increment_recent_cpu (void) {
  ASSERT(thread_mlfqs);
  
  fp32_t new_recent_cpu = thread_fp_get_recent_cpu();
  new_recent_cpu = add_int(new_recent_cpu, 1);
  thread_set_recent_cpu(new_recent_cpu);
}

/* Updates recent_cpu value of the argument thread. */
void
thread_update_recent_cpu (struct thread *t) 
{
  ASSERT(thread_mlfqs);
  
  fp32_t new_recent_cpu = t->recent_cpu;
  int nice = t->nice;
  fp32_t coefficient = load_avg;

  // Calculates the coefficient near load_avg in the equation first
  coefficient = multiply_int(coefficient, 2);
  coefficient = divide_fp(coefficient, add_int(coefficient, 1));

  new_recent_cpu = multiply_fp(coefficient, new_recent_cpu);
  new_recent_cpu = add_int(new_recent_cpu, nice);

  t->recent_cpu = new_recent_cpu;
}

/* Updates recent_cpu value of all threads. */
void thread_update_all_recent_cpus (void) {
  ASSERT(thread_mlfqs);
  
  struct list_elem *e;
  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
      struct thread *t = list_entry(e, struct thread, allelem);
      thread_update_recent_cpu(t);
  }
}

/* Updates priority of argument thread. */
void thread_update_priority (struct thread *t) {
  ASSERT(thread_mlfqs);
  
  int new_priority;
  fp32_t recent_cpu = t->recent_cpu;
  int nice = t->nice;

  // Calculates new priority according to the equation in the spec
  recent_cpu = divide_int(recent_cpu, 4);
  nice *= 2;
  new_priority = PRI_MAX - convert_integer_z(recent_cpu) - nice;

  // Handles cases when new_priority is out of bounds
  if (new_priority < PRI_MIN) {
    new_priority = PRI_MIN;
  } else if (new_priority > PRI_MAX) {
    new_priority = PRI_MAX;
  }

  t->priority = new_priority;
}

/* Updates priority of all threads. */
void thread_update_all_priorities (void) {
  ASSERT(thread_mlfqs);
  
  struct list_elem *e;
  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
      struct thread *t = list_entry(e, struct thread, allelem);
      thread_update_priority(t);
  }
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;

  if (thread_mlfqs) {
    t->nice = 0;
    t->recent_cpu = 0;
    // Sets thread priority for the inital thread
    t->priority = PRI_MAX;
  }

  t->effective_priority = priority;
  list_init(&t->received_priorities);

  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  enum comparator c = MORE;
  list_sort(&ready_list,&compare_threads,&c);
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
