#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *stack_pointer = f->esp;
  if (stack_pointer == NULL) {
    process_exit ();
    thread_exit ();
    return;
  }
  if (!is_user_vaddr(stack_pointer)) {
    process_exit ();
    thread_exit ();
    return;
  }
  /*
  if (!pagedir_get_page(??) == NULL) {
    process_exit ();
    thread_exit ();
    return;
  }*/
  //Gets SYS ENUM and calls correct system call
  //Return result to f->eax
  printf ("system call!\n");
  thread_exit ();
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (fd == STDOUT_FILENO) {
    unsigned remaining = size;

    while (remaining > CONSOLE_LIMIT) {
      putbuf(buffer, CONSOLE_LIMIT);
      remaining = remaining - CONSOLE_LIMIT;
    }
    putbuf(buffer, remaining);

    return size;
  }

  return 0;
}
