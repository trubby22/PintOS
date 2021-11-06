#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

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
      // TODO: halt
      break;
    case 1:
      // TODO: exit
      break;
    case 2:
      // TODO: exec
      break;
    case 3:
      // TODO: wait
      break;
    case 4:
      // TODO: create
      break;
    case 5:
      // TODO: remove
      break;
    case 6:
      // TODO: open
      break;
    case 7:
      // TODO: filesize
      break;
    case 8:
      // TODO: read
      break;
    case 9:
      break;
    case 10:
      // TODO: seek
      break;
    case 11:
      // TODO: tell
      break;
    case 12:
      // TODO: close
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


