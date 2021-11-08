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

  int pid, status, fd;
  const char* file;
  unsigned position ,length;

  switch (syscall_num)
  {
  case SYS_HALT:
    //halt();

  case SYS_EXIT:;
    status = *(int *) arg1;
    //exit (status);
    break;

  case SYS_EXEC:;
    file = *(const char **) arg1;
    //exec (file);
    break;

  case SYS_WAIT:;
    pid = *(int *) arg1;
    //wait (pid);
    break;

  case SYS_CREATE:;
    file = *(const char **) arg1;
    //create (file, initial_size);
    break;

  case SYS_REMOVE:;
    file = *(const char **) arg1;
    //remove (file);
    break;

  case SYS_OPEN:;
    file = *(const char **) arg1;
    //open (file);
    break;

  case SYS_FILESIZE:;
    fd = *(int *) arg1;
    //filesize (fd);
    break;

  case SYS_READ:;
    fd = *(int *) arg1;
    length = *(unsigned *) arg3;
    //read (fd, buffer, length);
    break;

  case SYS_WRITE:;
    fd = *(int *) arg1;
    length = *(unsigned *) arg3;
    //write (fd, buffer, length);
    break;

  case SYS_SEEK:;
    fd = *(int *) arg1;
    position = *(unsigned *) arg2;
    //seek (fd, position);
    break;

  case SYS_TELL:;
    fd = *(int *) arg1;
    //tell (fd);
    break;

  case SYS_CLOSE:;
    fd = *(int *) arg1;
    //close (fd);
    break;

  default:
   printf("An error occured while evaluating syscall_num!\n");
    break;

  }

  f -> eax = result;
}

void validate_user_pointer (const void *vaddr){
  if (vaddr){
    uint32_t *pd;
    pagedir_get_page(pd,vaddr);
    if (pd && is_user_vaddr(vaddr)){
      return;
    }
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
