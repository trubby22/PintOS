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
  int *esp = (int *) f -> esp;
  int syscall_num = *esp;
  void *arg1 = (void *) *(esp - 12);
  void *arg2 = (void *) *(esp - 8);
  void *arg3 = (void *) *(esp - 4);

  uint32_t result = 0;
  pid_t pid;
  int status, fd;
  const char* filename;
  const void* buffer;
  unsigned position, length;

  switch (syscall_num)
  {
  case SYS_HALT:
    halt_userprog();
    break;

  case SYS_EXIT:
    validate_user_pointer(arg1);
    status = *(int *) arg1;
    exit_userprog (status);
    break;

  case SYS_EXEC:
    validate_user_pointer(arg1);
    filename = *(const char **) arg1;
    result = (int) exec_userprog (filename);
    break;

  case SYS_WAIT:
    validate_user_pointer(arg1);
    pid = *(int *) arg1;
    result = wait_userprog (pid);
    break;

  case SYS_CREATE:
    validate_user_pointer(arg1);
    filename = *(const char **) arg1;
    validate_user_pointer(arg2);
    length = *(const char **) arg1;
    sema_down(&filesystem_sema);
    create_userprog (filename, length);
    sema_up(&filesystem_sema);
    break;

  case SYS_REMOVE:
    validate_user_pointer(arg1);
    filename = *(const char **) arg1;
    sema_down(&filesystem_sema);
    remove_userprog (filename);
    sema_up(&filesystem_sema);
    break;

  case SYS_OPEN:
    validate_user_pointer(arg1); 
    filename = *(const char **) arg1;
    sema_down(&filesystem_sema);
    open_userprog (filename);
    sema_up(&filesystem_sema);
    break;

  case SYS_FILESIZE:
    validate_user_pointer(arg1);
    fd = *(int *) arg1;
    sema_down(&filesystem_sema);
    struct file *target_file = get_file(fd);
    result = file_length (target_file);
    sema_up(&filesystem_sema);
    break;

  case SYS_READ:
    validate_user_pointer(arg1); 
    validate_user_pointer(arg2);
    validate_user_pointer(arg3);
    buffer = *(const void **) arg2;
    fd = *(int *) arg1;
    length = *(unsigned *) arg3;
    sema_down(&filesystem_sema);
    read_userprog (fd, buffer, length);
    sema_up(&filesystem_sema);
    break;

  case SYS_WRITE:
    validate_user_pointer(arg1); 
    validate_user_pointer(arg2); 
    validate_user_pointer(arg3); 
    buffer = *(const void **) arg2;
    fd = *(int *) arg1;
    length = *(unsigned *) arg3;
    sema_down(&filesystem_sema);
    write_userprog (fd, buffer, length);
    sema_up(&filesystem_sema);
    break;

  case SYS_SEEK:
    validate_user_pointer(arg1); 
    validate_user_pointer(arg2); 
    fd = *(int *) arg1;
    position = *(unsigned *) arg2;
    seek_userprog (fd, position);
    break;

  case SYS_TELL:
    validate_user_pointer(arg1); 
    fd = *(int *) arg1;
    tell_userprog (fd);
    break;

  case SYS_CLOSE:
    validate_user_pointer(arg1);
    fd = *(int *) arg1; 
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
  if (vaddr && is_user_vaddr(vaddr)){
    uint32_t *pd = thread_current()->pagedir;
    pagedir_get_page(pd,vaddr);
    if (pd){
      return;
    }
  }
  exit_userprog(1);
}

/* Given an fd will return the correspomding FILE* */
struct file *
get_file(int fd)
{
  struct process_hash_item *p = get_process_item();
  struct hash *files = p -> files;
  //create dummy elem with fd then:
  struct file_hash_item dummy_f;
  dummy_f.fd = fd;
  struct hash_elem *real_elem = hash_find(files, &dummy_f.elem);
  struct file_hash_item *f = hash_entry(real_elem, struct file_hash_item, elem);
  return f -> file;
}

void
halt_userprog (void)
{
  shutdown_power_off ();
}

void 
exit_userprog (int status) 
{
  thread_current()->exit_status = status;
  thread_exit();
}

pid_t 
exec_userprog (const char *cmd_line) 
{
  // Assuming pid is equivalent to tid
  enum intr_level old_level;

  old_level = intr_disable ();
  pid_t pid = (pid_t) process_execute(cmd_line); 
  intr_set_level (old_level);

  return pid;
}

int 
wait_userprog (pid_t pid) 
{
  // Assuming pid is equivalent to tid
  return process_wait((tid_t) pid);
}

int
write_userprog (int fd, const void *buffer, unsigned size)
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
open_userprog (const char *file)
{
  struct process_hash_item *p = get_process_item();

  struct file_hash_item *f = (struct file_hash_item *) malloc(sizeof(struct file_hash_item));
  f -> file = filesys_open(file);
  f -> fd = p -> next_fd;
  hash_insert(p -> files, &f -> elem);
  return f -> fd;
}

bool 
create_userprog (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

bool 
remove_userprog (const char *file)
{
  return filesys_remove(file);
}

void 
close_userprog (int fd)
{
  //Remove it from this processess hash table then 'close'
  struct process_hash_item *p = get_process_item();
  struct file* file_to_close = get_file(fd);

  struct hash *files = p -> files;
  //create dummy elem with fd then:
  struct file_hash_item dummy_f;
  dummy_f.fd = fd;
  struct hash_elem *real_elem = hash_find(files, &dummy_f.elem);
  struct file_hash_item *f = hash_entry(real_elem, struct file_hash_item, elem);


  hash_delete(p->files, real_elem);
  file_close(file_to_close);
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

  struct file_hash_item dummy_f;
  dummy_f.fd = fd;
  if(hash_find(get_process_item() -> files, &dummy_f.elem) == NULL) {
    return -1;
  }
  return file_read (get_file(fd), (void *) buffer, size);
}

void
seek_userprog (int fd, unsigned position)
{
  if(fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return;
  }

  get_file(fd)->pos = (off_t)position;
  return;
}

unsigned
tell_userprog (int fd)
{
  struct file *tell_file = get_file(fd);

  return ((unsigned)(get_file(fd)->pos));
}


