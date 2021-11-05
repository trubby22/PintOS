#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.c"
#include "threads/vaddr.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);

/* struct syscall_func{
  int expected_argv;
  union {
    void (*fun_ptr)(void);
    void (*fun_ptr)(int);
    pid_t exec (const char *file);
    bool create (const char *file, unsigned initial_size);
    bool remove (const char *file);
    int open (const char *file);
    int filesize (int fd);
    int read (int fd, void *buffer, unsigned length);
    void seek (int fd, unsigned position);
    unsigned tell (int fd);
    void close (int fd);
  }
} */



uint32_t (*syscall_funcs[])() = {halt, exit, exec, wait, create, remove, open, filesize,
                             read, write, seek, tell, close};

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
  void *arg1 = *(esp - 12);
  void *arg2 = *(esp - 8);
  void *arg3 = *(esp - 4);


  uint32_t (*syscall_func)() = syscal l_funcs[syscall_num];

  int argv = (arg1 != NULL) + (arg2 != NULL) + (arg3 != NULL);

  uint32_t result;

  switch (argv)
  {
  case 0:
    result = syscall_func();
    break;
  
  case 1:
    result = syscall_func(arg1);
    break;

  case 2:
    result = syscall_func(arg1,arg2);
    break;

  case 3:
    result = syscall_func(arg1,arg2,arg3);
    break;  

  default:
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
      remaining = remaining - CONSOLE_LIMIT;
      offset = offset + CONSOLE_LIMIT;
    }
    putbuf(buffer + offset, remaining);

    return size;
  }

  return 0;
}
