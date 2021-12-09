#include "lib/user/syscall.h"
#include "lib/stdio.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "vm/mmap.h"
#include "vm/page.h"
#include <string.h>
#include <syscall-nr.h>

#define VOID_RETURN 0

static void syscall_handler (struct intr_frame *);

// Hash function where the key is simply the file descriptor
// File descriptor will calculated with some sort of counter
// e.g. will start at 1 then tick up
unsigned 
hash_hash_fun(const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry(e, struct file_hash_item, elem)->fd;
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
  return (unsigned) hash_entry(e, struct process_hash_item, elem)->pid;
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

struct lock filesystem_lock;
struct lock console_lock;

uint32_t (*syscall_functions[15])(void **, void **, void **) = {
    &halt_userprog,
    &exit_userprog,
    &exec_userprog,
    &wait_userprog,
    &create_userprog,
    &remove_userprog,
    &open_userprog,
    &file_size_userprog,
    &read_userprog,
    &write_userprog,
    &seek_userprog,
    &tell_userprog,
    &close_userprog,
    &mmap_userprog,
    &munmap_userprog};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesystem_lock);
  lock_init(&console_lock);
  mmap_init();
}

void
syscall_exit(int status) {
  exit_userprog((void **) &status, NULL, NULL);
}

static void
syscall_handler (struct intr_frame *f) 
{
  // Gets stack pointer from interrupt frame
  uint32_t *sp = f->esp;

  validate_user_pointer((uint32_t *) sp);
  pin_obj(sp, sizeof(sp));

  // Reads syscall number from stack
  int syscall_num = (int) *sp;

  // Reads pointers from stack
  void **arg1_ptr = (void *) sp + 4;
  void **arg2_ptr = (void *) sp + 8;
  void **arg3_ptr = (void *) sp + 12;

  // Validates user pointers
  if (syscall_num == SYS_CREATE || syscall_num == SYS_REMOVE || syscall_num == SYS_OPEN || syscall_num == SYS_EXEC || 
  syscall_num == SYS_EXIT) {
    validate_user_pointer((uint32_t *) arg1_ptr);
  }
  if (syscall_num == SYS_READ || syscall_num == SYS_WRITE) {
    validate_user_pointer((uint32_t *) arg2_ptr);
  }
  
  // Pin pointers
  pin_obj(arg1_ptr, sizeof(arg1_ptr));
  pin_obj(arg2_ptr, sizeof(arg2_ptr));
  pin_obj(arg3_ptr, sizeof(arg3_ptr));

  // Pin objects that the pointers point to
  // pin_obj(*arg1_ptr, 1);
  // pin_obj(*arg2_ptr, 1);
  // pin_obj(*arg3_ptr, 1);

  f->eax = (*syscall_functions[syscall_num]) (arg1_ptr, arg2_ptr, arg3_ptr);

  // Unpin
  // unpin_obj(*arg3_ptr, 1);
  // unpin_obj(*arg2_ptr, 1);
  // unpin_obj(*arg1_ptr, 1);

  unpin_obj(arg3_ptr, sizeof(arg3_ptr));
  unpin_obj(arg2_ptr, sizeof(arg2_ptr));
  unpin_obj(arg1_ptr, sizeof(arg1_ptr));
  
}

/* Checks a user pointer is not NULL, is within user space and is 
   mapped to virtual memory. Otherwise the process is killed. */
void 
validate_user_pointer (uint32_t *vaddr)
{
  if (vaddr && is_user_vaddr(vaddr)){
    uint32_t *pd = thread_current()->pagedir;
    if (pagedir_get_page(pd, vaddr)){
      return;
    }
  }
  syscall_exit(-1);
}

struct file_hash_item *
get_file_hash_item_or_null(int fd)
{
  struct process_hash_item *p = get_process_item();
  struct hash *files = p->files;
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
  return hash_item->file;
}

uint32_t
halt_userprog (void ** arg1 UNUSED, void ** arg2 UNUSED, void ** arg3 UNUSED)
{
  shutdown_power_off ();
  return VOID_RETURN;
}

uint32_t 
exit_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  int status = *((int *) arg1);
  struct thread *t = thread_current();
  t->info->exit_status = status;
  while (!list_empty (&t->children)) {
    struct list_elem *e = list_pop_front (&t->children);
    struct child *child = list_entry (e, struct child, elem);
    free(child);
  }

  struct process_hash_item *p = get_process_item();
  struct hash *h = p->files;
  if (h != NULL) {
    free(h);
  }
  // if (p != NULL) {
  //   free(p);
  // }

  printf ("%s: exit(%d)\n", t->name, status);
  lock_release(&t->info->alive_lock);
  thread_exit();
  return VOID_RETURN;
}

uint32_t 
exec_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED) 
{
  const char *cmd_line = *((const char **) arg1);
  
  validate_user_pointer((uint32_t *) cmd_line);

  return (uint32_t) process_execute(cmd_line);
}

uint32_t 
wait_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED) 
{
  pid_t pid = *((pid_t *) arg1);
  return process_wait((tid_t) pid);
}

uint32_t
write_userprog (void **arg1, void **arg2, void **arg3)
{
  int fd = *((int *)arg1);
  void *buffer = *arg2;
  validate_user_pointer((uint32_t *) buffer);
  unsigned size = *((unsigned *) arg3);
  if (size == 0) {
    return 0;
  }

  if (fd == STDOUT_FILENO) {
    unsigned remaining = size;
    int offset = 0;

    lock_acquire(&console_lock);
    while (remaining > CONSOLE_LIMIT) {
      putbuf(buffer + offset, CONSOLE_LIMIT);
      remaining -= CONSOLE_LIMIT;
      offset += CONSOLE_LIMIT;
    }
    putbuf(buffer + offset, remaining);
    lock_release(&console_lock);
    return size;
  }

  lock_acquire(&filesystem_lock);
  struct file *file = get_file_or_null(fd);

  if(!file || file->deny_write) {
    lock_release(&filesystem_lock);
    return 0;
  }

  off_t written_size = file_write (file, buffer, size);
  lock_release(&filesystem_lock);
  return written_size;
}

uint32_t 
open_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  const char *file = *((const char **) arg1);
  if (!file)
    syscall_exit(-1);

  validate_user_pointer((uint32_t *) file);

  if (strlen(file) < 1)
    return -1;

  lock_acquire(&filesystem_lock);
  struct file *file_struct = filesys_open(file);
  lock_release(&filesystem_lock);

  if (!file_struct)
    return -1;
  
  struct process_hash_item *p = get_process_item();

  struct file_hash_item *f = (struct file_hash_item *) malloc(sizeof(struct file_hash_item));
  if (f == NULL) {
    PANIC ("Could not malloc file_hash_item when calling open_userprog");
  }
  f->fd = p->next_fd;
  p->next_fd++;
  f->file = file_struct;
  hash_insert(p->files, &f->elem);
  return f->fd;
}

uint32_t 
create_userprog (void **arg1, void **arg2, void **arg3 UNUSED)
{
  const char *file = (const char *) (*((const char **) arg1));
  validate_user_pointer((uint32_t *) file);
  unsigned initial_size = *((unsigned *) arg2);

  lock_acquire(&filesystem_lock);
  bool success = filesys_create(file, (off_t) initial_size);
  lock_release(&filesystem_lock);

  return success;
}

uint32_t 
remove_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  const char *file = (const char *) (*((const char **) arg1));
  lock_acquire(&filesystem_lock);
  bool success = filesys_remove(file);
  lock_release(&filesystem_lock);

  return success;
}

uint32_t 
close_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  int fd = *((int *) arg1);
  lock_acquire(&filesystem_lock);

  struct file_hash_item *f = get_file_hash_item_or_null(fd);
  if (!f) {
    lock_release(&filesystem_lock);
    syscall_exit(-1);
  }

  lock_release(&filesystem_lock);

  //Remove it from this processess hash table then 'close'
  if (!hash_delete(get_process_item()->files, &f->elem))
  {
    syscall_exit(-1);
    return VOID_RETURN;
  }
  file_close(f->file);
  free(f);
  return VOID_RETURN;
}

uint32_t
read_userprog (void **arg1, void **arg2, void **arg3)
{
  int fd = *((int *) arg1);
  void *buffer = *((void **) arg2);
  unsigned size = *((unsigned *) arg3);
  validate_user_pointer((uint32_t *) buffer);

  if (fd == STDIN_FILENO) {
    char* console_out = (char *) buffer;
    unsigned key_count = 0;
    int offset = strlen(console_out);

    lock_acquire(&console_lock);
    uint8_t cur_key = input_getc();
    while((char)cur_key != '\n') {
      console_out[offset + key_count] = (char)cur_key;
      key_count += 1;
      if(key_count == size) {
        lock_release(&console_lock);
        return size;
      }
      cur_key = input_getc();
    }
    lock_release(&console_lock);
    return key_count;
  }

  lock_acquire(&filesystem_lock);
  struct file *file = get_file_or_null(fd);

  if(file == NULL) {
    lock_release(&filesystem_lock);
    return -1;
  }

  off_t result = file_read (file, buffer, size);
  lock_release(&filesystem_lock);
  return result;
}

uint32_t
seek_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  int fd = *((int *) arg1);
  unsigned position = *((unsigned *) arg2);
  if(fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return VOID_RETURN;
  }

  lock_acquire(&filesystem_lock);
  struct file *file = get_file_or_null(fd);
  lock_release(&filesystem_lock);
  if(file == NULL) {
    syscall_exit(-1);
  }

  file->pos = (off_t)position;
  return VOID_RETURN;
}

uint32_t
tell_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  int fd = *((int *) arg1);
  lock_acquire(&filesystem_lock);
  struct file *file = get_file_or_null(fd);
  lock_release(&filesystem_lock);
  if(file == NULL) {
    syscall_exit(-1);
    return VOID_RETURN;
  }

  return ((unsigned)(file->pos));
}

uint32_t
mmap_userprog(void **arg1, void **arg2, void **arg3 UNUSED)
{
  int fd = *((int *) arg1);
  void *addr = (void *) *((uint32_t **) arg2);

  if (fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return -1;
  }

  if (pg_ofs(addr) || !addr) {
    return -1;
  }

  lock_acquire(&filesystem_lock);

  struct file *target_file = get_file_or_null(fd);

  // If getting the file fails, exit with -1
  if (!target_file) {
    lock_release(&filesystem_lock);
    syscall_exit(-1);
  }

  // Get filesize
  uint32_t size = file_length(target_file);

  // Ensure file is not empty
  if (size == 0) {
    lock_release(&filesystem_lock);
    return -1;
  }

  // Calculate number of pages needed
  int pgcnt = size / PGSIZE;
  if (size % PGSIZE)
    pgcnt++;

  if (addr < thread_current()->spt.exe_size + EXE_BASE) 
  {
    lock_release(&filesystem_lock);
    return -1;
  }

  // Save file's metadata in SPT. Used for lazy-loading.
  spt_add_mmap_file (target_file, addr);
  lock_release(&filesystem_lock);

  // Store the mapping in the list
  mapid_t id = mmap_add_mapping(fd, pgcnt, addr);

  // Don't allocate resources for file here because the file will be loaded lazily

  return 0;
}

uint32_t
munmap_userprog(void **arg1, void **arg2 UNUSED, void **arg3 UNUSED)
{
  mapid_t mapid = *((int *) arg1);
  mmap_remove_mapping(mapid);
  return VOID_RETURN;
}


uint32_t file_size_userprog (void **arg1, void **arg2 UNUSED, void **arg3 UNUSED) {
  int fd = *((int *) arg1);
  lock_acquire(&filesystem_lock);

  struct file *target_file = get_file_or_null(fd);

  if(!target_file) {
    lock_release(&filesystem_lock);
    syscall_exit(-1);
  }

  uint32_t fs = file_length (target_file);
  lock_release(&filesystem_lock);

  return fs;
}

void acquire_filesystem_lock(void) {
  lock_acquire(&filesystem_lock);
}

void release_filesystem_lock(void) {
  lock_release(&filesystem_lock);
}

