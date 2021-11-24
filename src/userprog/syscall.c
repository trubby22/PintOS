#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);

// Hash function where the key is simply the file descriptor
// File descriptor will calculated with some sort of counter
// e.g. will start at 1 then tick up
unsigned 
hash_hash_fun(const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry(e, struct file_hash_item, elem) -> fd;
}

// Is it bad practise to compare them by their keys?
// Compare hash items by their file descriptor
bool 
hash_less_fun (const struct hash_elem *a,
               const struct hash_elem *b,
               void *aux UNUSED)
{
  return hash_hash_fun(a,NULL) < hash_hash_fun(b,NULL);
}

// the same as other one, could refactor 
unsigned 
hash_hash_fun_b(const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry(e, struct process_hash_item, elem) -> pid;
}

// also same as other one? could refactor, would be easier than the 
// one above
bool 
hash_less_fun_b (const struct hash_elem *a,
                 const struct hash_elem *b,
                 void *aux UNUSED)
{
  return hash_hash_fun_b(a,NULL) < hash_hash_fun_b(b,NULL);
}

struct semaphore filesystem_sema;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  sema_init(&filesystem_sema, 1);
}

static void
syscall_handler (struct intr_frame *f) 
{
  if ((f->esp + sizeof(struct intr_frame)) > PHYS_BASE)
    exit_userprog(-1);

  // Gets stack pointer from interrupt frame
  uint32_t sp = f->esp;
  
  validate_user_pointer((void *) &sp);

  // Reads syscall number from stack
  int syscall_num = (int) *((int *) sp);

  // Reads pointers from stack
  uint32_t arg1_ptr = sp + 4;
  uint32_t arg2_ptr = sp + 8;
  uint32_t arg3_ptr = sp + 12;

  // Validates user pointers
  if (syscall_num == SYS_CREATE || syscall_num == SYS_REMOVE || syscall_num == SYS_OPEN || syscall_num == SYS_EXEC) {
    validate_user_pointer((const void *) arg1_ptr);
  }
  if (syscall_num == SYS_READ || syscall_num == SYS_WRITE) {
    validate_user_pointer((const void *) arg2_ptr);
  }
  
  uint32_t result = 0;

  // Argument signatures

  int status, fd;
  pid_t pid;
  unsigned initial_size, size, position;

  char *cmd_line, *file;
  void *buffer;

  switch (syscall_num)
  {
  case SYS_HALT:
    halt_userprog();
    break;

  case SYS_EXIT:
    status = *((int *) arg1_ptr);
    exit_userprog (status);
    break;

  case SYS_EXEC:
    cmd_line = *((const char **) arg1_ptr);
    result = (uint32_t) exec_userprog ((const char *) cmd_line);
    break;

  case SYS_WAIT:
    pid = *((pid_t *) arg1_ptr);
    result = wait_userprog (pid);
    break;

  case SYS_CREATE:
    file = *((const char **) arg1_ptr);
    initial_size = *((unsigned *) arg2_ptr);

    result = create_userprog ((const char *) file, initial_size);
    break;

  case SYS_REMOVE:
    file = *((const char **) arg1_ptr);

    result = remove_userprog ((const char *) file);
    break;

  case SYS_OPEN:
    file = *((const char **) arg1_ptr);

    sema_down(&filesystem_sema);
    result = open_userprog (file);
    sema_up(&filesystem_sema);
    break;

  case SYS_FILESIZE:
    fd = *((int *)arg1_ptr);

    sema_down(&filesystem_sema);
    struct file *target_file = get_file_or_null(fd);
    if(target_file == NULL) {
      exit_userprog(-1);
      break;
    }
    result = file_length (target_file);
    sema_up(&filesystem_sema);
    break;

  case SYS_READ:
    fd = *((int *) arg1_ptr);
    buffer = *((void **) arg2_ptr);
    size = *((unsigned *) arg3_ptr);

    sema_down(&filesystem_sema);
    result = read_userprog (fd, buffer, size);
    sema_up(&filesystem_sema);
    break;

  case SYS_WRITE:
    fd = *((int *) arg1_ptr);
    buffer = *((void **) arg2_ptr);
    size = *((unsigned *) arg3_ptr);

    sema_down(&filesystem_sema);
    result = write_userprog (fd, buffer, size);
    sema_up(&filesystem_sema);
    break;

  case SYS_SEEK:
    fd = *((int *) arg1_ptr);
    position = *((unsigned *) arg2_ptr);

    seek_userprog (fd, position);
    break;

  case SYS_TELL:
    fd = *((int *) arg1_ptr);

    result = tell_userprog (fd);
    break;

  case SYS_CLOSE:
    fd = *((int *) arg1_ptr);
    
    sema_down(&filesystem_sema);
    close_userprog (fd);
    sema_up(&filesystem_sema);
    break;

  default:
    printf("An error occured while evaluating syscall_num!\n");
    break;
  }

  f->eax = result;
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
  uint32_t address = *((uint32_t *) vaddr);
  if (address && is_user_vaddr(address)){
    uint32_t *pd = thread_current()->pagedir;
    if (pagedir_get_page(pd,address)){
      return;
    }
  }
  exit_userprog(-1);
}

struct file_hash_item *
get_file_hash_item_or_null(int fd)
{
  struct process_hash_item *p = get_process_item();
  struct hash *files = p -> files;
  //create dummy elem with fd then:
  struct file_hash_item dummy_f;
  dummy_f.fd = fd;
  struct hash_elem *real_elem = hash_find(files, &dummy_f.elem);
  if (!real_elem) {
    return NULL;
  }
  struct file_hash_item *f = hash_entry(real_elem, struct file_hash_item, elem);

  return f;
}

/* Given an fd will return the correspomding FILE* */
struct file *
get_file_or_null(int fd)
{
  struct file_hash_item *hash_item = get_file_hash_item_or_null(fd);
  if (!hash_item) {
    return NULL;
  }
  return hash_item -> file;
}

void
halt_userprog (void)
{
  shutdown_power_off ();
}

void 
exit_userprog (int status) 
{
  struct thread *t = thread_current();
  t->exit_status = status;
  printf ("%s: exit(%d)\n", t->name, status);
  thread_exit();
}

pid_t 
exec_userprog (const char *cmd_line) 
{
  enum intr_level old_level;

  old_level = intr_disable ();
  pid_t pid = (pid_t) process_execute(cmd_line); 
  intr_set_level (old_level);

  return pid;
}

int 
wait_userprog (pid_t pid) 
{
  return process_wait((tid_t) pid);
}

int
write_userprog (int fd, const void *buffer, unsigned size)
{
  if (size == 0)
    return 0;
  
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

  struct file *file = get_file_or_null(fd);
  if(file == NULL) {
    return 0;
  }

  return file_write (file, buffer, size);
}

int 
open_userprog (const char *file)
{
  if (strlen(file) <= 1)
    return -1;

  struct file *file_struct = filesys_open(file);

  if (!file_struct)
    return -1;
  
  struct process_hash_item *p = get_process_item();

  struct file_hash_item *f = (struct file_hash_item *) malloc(sizeof(struct file_hash_item));
  f -> fd = p -> next_fd;
  p -> next_fd++;
  f->file = file_struct;
  hash_insert(p -> files, &f -> elem);
  return f -> fd;
}

bool 
create_userprog (const char *file, unsigned initial_size)
{
  if (!file) 
    exit_userprog(-1);

  sema_down(&filesystem_sema);
  bool success = filesys_create(file, (off_t) initial_size);
  sema_up(&filesystem_sema);

  return success;
}

bool 
remove_userprog (const char *file)
{
  sema_down(&filesystem_sema);
  bool success = filesys_remove(file);
  sema_up(&filesystem_sema);

  return success;
}

void 
close_userprog (int fd)
{
  struct file_hash_item *f = get_file_hash_item_or_null(fd);
  if(f == NULL) {
    exit_userprog(-1);
    return;
  }

  //Remove it from this processess hash table then 'close'
  if(!hash_delete(get_process_item()->files, &f->elem))
  {
    exit_userprog(-1);
    return;
  }
  file_close(f->file);
  free(f);
}

int
read_userprog (int fd, const void *buffer, unsigned size)
{
  if (fd == STDIN_FILENO) {
    char* console_out = (char *) buffer;
    uint8_t cur_key = input_getc();
    unsigned key_count = 0;
    int offset = strlen(console_out);

    while((char)cur_key != '\n') {
      console_out[offset + key_count] = (char)cur_key;
      key_count += 1;
      if(key_count == size) {
        return size;
      }
      cur_key = input_getc();
    }
    return key_count;
  }

  struct file *file = get_file_or_null(fd);
  if(file == NULL) {
    return -1;
  }
  return file_read (file, (void *) buffer, size);
}

void
seek_userprog (int fd, unsigned position)
{
  if(fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return;
  }

  struct file *file = get_file_or_null(fd);
  if(file == NULL) {
    exit_userprog(-1);
    return;
  }

  file->pos = (off_t)position;
  return;
}

unsigned
tell_userprog (int fd)
{
  struct file *file = get_file_or_null(fd);
  if(file == NULL) {
    exit_userprog(-1);
    return;
  }

  return ((unsigned)(file->pos));
}


