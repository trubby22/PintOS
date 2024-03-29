#include <inttypes.h>
#include <stdio.h>
#include "userprog/exception.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "lib/user/syscall.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
    {
       /* User's code segment, so it's a user exception, as we
          expected.  Kill the user process.  */
       printf("%s: dying due to interrupt %#04x (%s).\n",
              thread_name(), f->vec_no, intr_name(f->vec_no));
       intr_dump_frame(f);
       thread_current()->info->exit_status = -1;
       syscall_exit(-1);
   }

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  
         Shouldn't happen.  Panic the kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - this shouldn't be possible!");
    }
}

// Checks whether fault_addr lies within spt_page. If yes, loads the relevant page into memory. Used for lazy-loading.
static bool 
check_and_possibly_load_page (struct spt_page *spt_page, void *fault_addr) 
{
  bool loaded = spt_page->loaded;
  // Checks whether fault_addr lies in spt_page
  bool too_low = fault_addr < spt_page->upage;
  bool too_high = fault_addr > spt_page->upage + PGSIZE;
  bool stack = spt_page->type == STACK;

  if (loaded || too_low || too_high || stack)
    return false;

  acquire_filesystem_lock();
  // Loads missing page from file or executable
  spt_page->loaded = load_page(spt_page->file, spt_page->ofs, spt_page->upage, spt_page->read_bytes, spt_page->zero_bytes, spt_page->writable);
  release_filesystem_lock();
  
  return true;
}

// Go over list of process's pages using SPT and see whether fault_addr belongs to any of the pages that are scheduled to be lazy-loaded. If a page is found, laod it into memory.
bool
attempt_load_pages(void *fault_addr)
{
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;
  ASSERT(pages);

  struct list_elem *e;

  lock_acquire(&spt->pages_lock);

  // Ensures list has been initalized
  ASSERT(list_begin(pages));
  ASSERT(list_end(pages));

  for (e = list_begin(pages); e != list_end(pages); e = list_next(e))
  {
    struct spt_page *spt_page = list_entry (e, struct spt_page, elem);
    ASSERT(spt_page);

    if (check_and_possibly_load_page(spt_page, fault_addr)) 
    {
      lock_release(&spt->pages_lock);
      return true;
    }
  }
  lock_release(&spt->pages_lock);
  return false;
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool present;      /* True: writing r/o page, false: not-present page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  present = (f->error_code & PF_P) == PF_P;
  write = (f->error_code & PF_W) == PF_W;
  user = (f->error_code & PF_U) == PF_U;

  // Searches for fault_addr in SPT. If inside SPT, in most cases the fault is handled and process continues. Otherwise, terminte process.
  // Checks if fault_addr belongs to executable or memory-mapped file
  if (!present && user && attempt_load_pages(fault_addr))
    return;
  
  // Check that the user stack pointer appears to be in stack space:
  void *esp = f->esp;

  ASSERT(esp);

  // Check if it's a stack addr
  // Since stack can only grow via PUSH or PUSHA assembly instruction, the fault_addr must be either 4 or 32 bytes below esp.
  if (
    is_user_vaddr(esp) && 
    esp >= PHYS_BASE - STACK_LIMIT && 
    (fault_addr == esp || fault_addr == esp - 4 || fault_addr == esp - 28 || fault_addr == esp - 32)) 
  {
    // Create new page
    if (create_stack_page(&esp)) {
      return;
    }
  }

  if (!present && user)
  {
     uint32_t *pd = thread_current()->pagedir;
     if (pagedir_restore(pd,fault_addr))
     {
        return;
     }  
  }

  /* To implement virtual memory, delete the rest of the function
    body, and replace it with code that brings in the page to
    which fault_addr refers. */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          !present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}
