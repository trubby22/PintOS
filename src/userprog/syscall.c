#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

// Need a hash table from prcoess/thread id -> files
// Files being another hashtable from file descriptors to FILE*'s

// 


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

  int expected = 3; //Will check all args for now
  validate_args(expected, arg1, arg2, arg3);

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

/* Checks that the first int expected number of args are valid */
void validate_args (int expected, void *arg1, void *arg2, void *arg3)
{
  void *args[3] = {arg1,arg2,arg3};
  for (int i = 0; i < expected; i++)
  {
    validate_user_pointer ( (const void *) args[i]);
  }
}

/* Checks a user pointer is not NULL, is within user space and is 
   mapped to virtual memory. Otherwise the process is killed. */
void validate_user_pointer (const void *vaddr){
  if (vaddr && is_user_vaddr(vaddr)){
    uint32_t *pd;
    pagedir_get_page(pd,vaddr);
    if (pd){
      return;
    }
  }
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

int open (const char *file){
  //Missing lots of functionality
  //File might already be open from another process
  //in which case a unique fd should be made?
/*   FILE *fp;
  fp = fopen(file, "w");
  int fd = (int) fp;
  return fd; */

  return 0;
}

//Should only close properly with no other processes have it open
void close (int fd);


int
read (int fd, const void *buffer, unsigned size)
{
  if (fd == STDIN_FILENO) {
    // use input_getc()
  }

  // otherwise us fgets(buffer, size, fd => fp, )

  return 0;
}


