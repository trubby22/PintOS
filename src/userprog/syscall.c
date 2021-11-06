#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *esp = (int *) f -> esp;
  int syscall_num = *esp;
  void *arg1 = (void *) *(esp - 12);
  void *arg2 = (void *) *(esp - 8);
  void *arg3 = (void *) *(esp - 4);
  uint32_t result = 0;

  switch(syscall_num) {
    case 0:
      halt();
      break;
    case 1:
      exit((int) arg1);
      break;
    case 2:
      result = (uint32_t) exec((const char *) arg1);
      break;
    case 3:
      result = (uint32_t) wait((pid_t) arg1);
      break;
    case 4:
      result = (uint32_t) create((const char *) arg1, (unsigned) arg2);
      break;
    case 5:
      result = (uint32_t) remove((const char *) arg1);
      break;
    case 6:
      result = (uint32_t) open((const char *) arg1);
      break;
    case 7:
      result = (uint32_t) filesize((int) arg1);
      break;
    case 8:
      result = (uint32_t) read((int) arg1, (void *) arg2, (unsigned int) arg3);
      break;
    case 9:
      result = (uint32_t) write((int) arg1, (const void *) arg2, (unsigned int) arg3);
      break;
    case 10:
      seek((int) arg1, (unsigned) arg2);
      break;
    case 11:
      result = (uint32_t) tell((int) arg1);
      break;
    case 12:
      close((int) arg1);
      break;
    default:
      // TODO: Change that to an exception and catch it later
      printf("An error occured while evaluating syscall_num!\n");
      break;
  }

  f -> eax = result;
}

void validate_user_pointer (uint32_t *pd, void *user_pointer){
  if (user_pointer &&  pagedir_get_page(pd,user_pointer) && is_user_vaddr(user_pointer))
  {
    return;
  }
  //kill the process
  process_exit();
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (fd == STDOUT_FILENO) {
    unsigned remaining = size;
    int offset = 0;

    while (remaining > CONSOLE_LIMIT) {
      putbuf(buffer + offset, CONSOLE_LIMIT);
      remaining -= CONSOLE_LIMIT;
      offset += CONSOLE_LIMIT;
    }
    putbuf(buffer + offset, remaining);

    return size;
  }

  return 0;
}
