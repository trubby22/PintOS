#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <syscall-nr.h>
#include <filesys.h>
#include <file.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

// Used in a hashtable to map file descriptors to FILE structs.
struct file_hash_item
{
  file *file;  //The actual file
  int fd;      //File descriptor, for the hash function
  struct hash_elem elem;
};

// Hash function where the key is simply the file descriptor
// File descriptor will calculated with some sort of counter
// e.g. will start at 1 then tick up
unsigned 
hash_hash_func(const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry(e, struct file_hash_item, elem) -> fd;
}

// Is it bad practise to compare them by their keys?
// Compare hash items by their file descriptor
bool 
hash_less_fun (const struct hash_elem *a,
               const struct hash_elem *b,
               void *aux)
{
  return hash_hash_func(a,NULL) < hash_hash_func(b,NULL);
}

// the same as other one, could refactor 
unsigned 
hash_hash_func_b(const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry(e, struct process_hash_item, elem) -> pid;
}

// also same as other one? could refactor, would be easier than the 
// one above
bool 
hash_less_fun_b (const struct hash_elem *a,
                 const struct hash_elem *b,
                 void *aux)
{
  return hash_hash_func_b(a,NULL) < hash_hash_func_b(b,NULL);
}


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
  pid_t pid;
  int status, fd;
  const char* file;
  unsigned position ,length;

  switch (syscall_num)
  {
  case SYS_HALT:
    shutdown_power_off ();
    break;

  case SYS_EXIT:
    status = *(int *) arg1;
    exit (status);
    break;

  case SYS_EXEC:
    file = *(const char **) arg1;
    result = (int) exec (file);
    break;

  case SYS_WAIT:
    pid = *(int *) arg1;
    result = wait (pid);
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
    filesize (fd);
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
void 
validate_args (int expected, void *arg1, void *arg2, void *arg3)
{
  void *args[3] = {arg1,arg2,arg3};
  for (int i = 0; i < expected; i++)
  {
    validate_user_pointer ( (const void *) args[i]);
  }
}

/* Checks a user pointer is not NULL, is within user space and is 
   mapped to virtual memory. Otherwise the process is killed. */
void 
validate_user_pointer (const void *vaddr)
{
  if (vaddr && is_user_vaddr(vaddr)){
    uint32_t *pd;
    pagedir_get_page(pd,vaddr);
    if (pd){
      return;
    }
  }
  exit(1);
}

/* Given an fd will return the correspomding FILE* */
file 
get_file(int fd)
{
  struct process_hash_item *p = get_process_item();
  struct hash files = p -> files;
  //create dummy elem with fd then:
  struct file_hash_item *dummy_f;
  dummy_f -> fd = fd;
  real_elem = hash_find(files, &dummy_f -> elem);
  struct file_hash_item *f = hash_entry(real_elem, struct file_hash_item, elem);
  return f -> file;
}

/* Returns the table of files for the current process */
struct process_hash_item*
get_process_item()
{
  pid_t pid = thread_current() -> tid;
  //create dummy elem with pid then:
  struct process_hash_item *dummy_p;
  dummy_p -> pid = pid; 
  struct hash_elem *real_elem = hash_find(process_table, &dummy_p -> elem);
  struct process_hash_item *p = hash_entry(real_elem, struct process_hash_item, elem);
  return p;
}

void 
exit (int status) 
{
  thread_current()->exit_status = status;
  process_exit();
  thread_exit();
}

pid_t 
exec (const char *cmd_line) 
{
  // Assuming pid is equivalent to tid
  enum intr_level old_level;

  old_level = intr_disable ();
  pid_t pid = (pid_t) process_execute(cmd_line); 
  intr_set_level (old_level);

  return pid;
}

int 
wait (pid_t pid) 
{
  // Assuming pid is equivalent to tid
  return process_wait((tid_t) pid);
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

  return file_write (get_file(fd), buffer, size);
}

int 
open (const char *file)
{
  struct process_hash_item *p = get_process_item();

  struct file_hash_item *f;
  f -> file = filesys_open(file);
  f -> fd = p -> next_fd;
  hash_insert(p -> files, f -> elem);
  return fd;
}

bool 
create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

bool 
remove (const char *file)
{
  return filesys_remove(file);
}

void 
close (int fd)
{
  //Remove it from this processess hash table then 'close'
  struct process_hash_item *p = get_process_item();
  struct file* file = get_file(fd);
  hash_delete(p->files,file);
  file_close(file);
}

int
read (int fd, const void *buffer, unsigned size)
{
  if (fd == STDIN_FILENO) {
    //input_getc();
    return size;
  }

  return file_read (get_file(fd), buffer, size);
}


