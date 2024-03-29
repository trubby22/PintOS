#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "vm/frame.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/page.h"
#include <bitmap.h>

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void *init_stack(struct list *args_list);

static struct hash process_table;

// Initializes hash table
void
init_hash_table (void) 
{
  hash_init(&process_table, hash_hash_fun_b, hash_less_fun_b, NULL);
}

/* Returns the table of files for the current process */
struct process_hash_item *
get_process_item(void)
{
  pid_t pid = thread_current()->tid;
  //create dummy elem with pid then:
  struct process_hash_item dummy_p;
  dummy_p.pid = pid; 
  struct hash_elem *real_elem = hash_find(&process_table, &dummy_p.elem);
  struct process_hash_item *p = hash_entry(real_elem, struct process_hash_item, elem);
  return p;
}

static void free_args(struct list *args_list) {
  while (!list_empty (args_list)) {
    struct list_elem *e = list_pop_front (args_list);
    struct arg *arg = list_entry (e, struct arg, elem);
    free(arg->str);
    free(arg);
  }
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmd_args) 
{
  tid_t tid;

  ASSERT (intr_get_level () == INTR_ON);

  int size;
  struct list args_list;
  list_init(&args_list);

  char cmd_args_cpy[strlen(cmd_args) + 1];
  strlcpy(cmd_args_cpy, cmd_args, strlen(cmd_args) + 1);

  // Tokenize the command line
  char *token, *save_ptr;
  for (token = strtok_r(cmd_args_cpy, " ", &save_ptr); token != NULL;
       token = strtok_r(NULL, " ", &save_ptr))
  {
    char *str = (char *) malloc(strlen(token) + 1);
    if (str == NULL) {
      PANIC ("Failed to malloc char * in process.c/process_execute");
    }
    strlcpy(str, token, strlen(token) + 1);

    struct arg *arg = (struct arg *) malloc(sizeof(struct arg));
    if (arg == NULL) {
      PANIC ("Failed to malloc struct arg in process.c/process_execute");
    }
    arg->str = str;
 
    list_push_back(&args_list, &arg->elem);
  }

  struct list_elem *e = list_begin(&args_list);
  struct arg *arg = list_entry (e, struct arg, elem);
  char *fst_str = arg->str;
  char file_name[strlen(fst_str) + 1];
  strlcpy(file_name, fst_str, strlen(fst_str) + 1);

  // Checks if executable exists
  acquire_filesystem_lock();

  struct file *file = filesys_open(file_name);

  if (file == NULL) {
    
    free_args(&args_list);

    return TID_ERROR;
  } else {
    file_close(file);
  }

  release_filesystem_lock();

  struct thread *t = thread_current();

  struct data_from_parent data_from_parent;
  data_from_parent.args_list = &args_list;
  data_from_parent.parent = t;

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, &data_from_parent);

  // Adds a record of child in parent
  struct list_elem *elem = list_front(&thread_current()->children);
  struct child *child = list_entry (elem, struct child, elem);

  sema_down(&child->sema);

  // By this line the child has already loaded its executable - so we know whether that was a success or not

  free_args(&args_list);

  if (child->exit_status == TID_ERROR) {
    tid = TID_ERROR;
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *data_from_parent)
{
  struct data_from_parent *data = (struct data_from_parent *) data_from_parent;
  struct list *args_list = data->args_list;
  struct thread *parent = data->parent;
  struct spt *parent_spt = &parent->spt;

  // Initisalise new process_hash_item
  // Has to be done once thread has started running
  struct process_hash_item *p = (struct process_hash_item *)malloc(sizeof(struct process_hash_item));
  if (p == NULL) {
    PANIC("Failure mallocing struct process_hash_item in start_process");
  }
  p->next_fd = 2;

  // Initializes thread's files
  struct hash *files = (struct hash *) malloc(sizeof(struct hash));
  if (files == NULL) {
    PANIC("Failure mallocing struct hash in start_process");
  }
  hash_init(files, hash_hash_fun, hash_less_fun, NULL);
  
  p->files = files;
  p->pid = thread_current()->tid;
  hash_insert(&process_table, &p->elem);
  
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  struct thread *t = thread_current();
  const char *function_name = t->name;

  // Gets spt_pages and mappings to frames from parent
  share_pages (parent, t);

  acquire_filesystem_lock();
  // Loads metadata about eexecutable to SPT
  success = load (function_name, &if_.eip, &if_.esp);
  release_filesystem_lock();

  t->info->load_success = success;
  // Upping the sema on the line below gives control back to the parent
  sema_up(&t->info->sema);

  // Initializes the stack and saves stack pointer in interrupt frame
  if_.esp = init_stack((struct list *) args_list);

  /* If load failed, quit. */
  if (!success) 
    syscall_exit (-1);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status. 
 * If it was terminated by the kernel (i.e. killed due to an exception), 
 * returns -1.  
 * If TID is invalid or if it was not a child of the calling process, or if 
 * process_wait() has already been successfully called for the given TID, 
 * returns -1 immediately, without waiting.
 * 
 * This function will be implemented in task 2.
 * For now, it does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread *t = thread_current();
  struct list *list = &t->children;

  struct child *target;
  struct list_elem *e;
  bool found = false;

  for (e = list_begin (list); e != list_end (list);
       e = list_next (e)) {
    struct child *child = list_entry (e, struct child, elem);
    if (child->tid == child_tid) {
      target = child;
      found = true;
      break;
    }
  }

  if (!found) {
    return -1;
  }

  lock_acquire(&target->alive_lock);

  int exit_status = target->exit_status;

  list_remove(&target->elem);
  free(target);

  return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  free_process_spt();
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable, enum data_type type);
bool load_page (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }
  
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }

              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable, EXECUTABLE))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  // file_close (file);
  // Denies write since it's an executable and resets file's head to 0 because the executable will need to be read once more to actually load its pages into memory
  file_deny_write(file);
  file_seek (file, 0);
  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
  // Loads metatadata about a segment for either an executable or a file. Not used for stack pages.
bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, enum data_type type) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  struct thread *t = thread_current ();
  struct spt *spt = &t->spt;
  struct list *pages = &spt->pages;

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      // Checks for a "clash", i.e. whether the page has already been loaded by an adjacent segment
      struct spt_page *prev;

      lock_acquire(&spt->pages_lock);
      bool pages_is_empty = list_empty(pages);
      if (!pages_is_empty) {
        prev = list_entry(list_back(pages), struct spt_page, elem);
      }
      lock_release(&spt->pages_lock);

      if (!pages_is_empty && prev->upage == upage) {
        // If either segment is writable, page must be writable
        prev->writable = prev->writable || writable;
        // Set read_bytes to max of the two segments
        prev->read_bytes = prev->read_bytes > read_bytes ? prev->read_bytes : read_bytes;
        prev->zero_bytes = PGSIZE - prev->read_bytes;
      } else {
        struct spt_page *spt_page = (struct spt_page *) malloc(sizeof(struct spt_page));
        if (spt_page == NULL) {
          PANIC ("Failed to malloc struct spt_page in process.c/load");
        }

        // Initializes spt_page
        spt_page->ofs = ofs;
        spt_page->upage = upage;
        spt_page->read_bytes = page_read_bytes;
        spt_page->zero_bytes = page_zero_bytes;
        spt_page->writable = writable;

        spt_page->type = type;
        spt_page->loaded = false;
        spt_page->file = file;

        lock_acquire(&spt->pages_lock);
        // Adds spt_page to pages list in spt
        list_push_back(pages, &spt_page->elem);
        lock_release(&spt->pages_lock);
      }

      // Moves file's head
      file->pos += page_read_bytes;

      /* Advance. */
      if (type == EXECUTABLE) {
        spt->exe_size += PGSIZE;
      }
      ofs += PGSIZE;
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }

  return true;
}

// Used for loading a single page from file into memory.
bool
load_page (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) == PGSIZE);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  
  /* Check if virtual page already allocated */
  struct thread *t = thread_current ();
  uint8_t *kpage = pagedir_get_page (t->pagedir, upage);
  
  if (!kpage)
  {
    /* Get a new page of memory. */
    kpage = palloc_get_page_aux (PAL_USER | PAL_ZERO, t->pagedir, upage);
    if (!kpage)
    {
      return false;
    }

    /* Add the page to the process's address space. */
    if (!install_page (upage, kpage, writable)) 
    {
      palloc_free_page (kpage);
      return false;
    }        
  }

  /* Load data into the page. */
  int file_read_bytes = file_read (file, kpage, read_bytes);
  if (file_read_bytes != (int) read_bytes)
    {
      palloc_free_page (kpage);
      return false; 
    }
    
  return true;
}

// Creates an additional stack page and loads in memory. Used for stack growth.
bool
create_stack_page (void **esp)
{
  uint8_t *kpage;
  bool success = false;
  struct thread *t = thread_current();
  struct spt *spt = &t->spt;
  // User address of new stack page
  void *upage = (void *) PHYS_BASE - spt->stack_size - PGSIZE;

  kpage = palloc_get_page_aux (PAL_USER | PAL_ZERO, t->pagedir, upage);
  if (!kpage) {
    PANIC("Failed to allocate page for stack");
  }
  success = install_page (upage, kpage, true);
  
  if (success) {
    spt_add_stack_page(upage);
    spt->stack_size += PGSIZE;
    *esp = (void *) PHYS_BASE - spt->stack_size;
  } else {
    palloc_free_page (kpage);
    PANIC("Failed to install stack page");
  }

  return success;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  return create_stack_page(esp);
}

// Puts cmd line arguments on the stack
static void *init_stack(struct list *args_list) {
  // Stack pointer - should point to the fake return address of the program
  uint32_t sp = 0;

  int size = list_size(args_list);
  int length_arr[size];

  // Array containing pointers to argv elements
  char *argv_ptr_arr[size];
  // Pointer to lowest 0 added to align the address
  uint8_t *align_addr;
  // Null pointer sentinel
  char **null_ptr_sentinel;
  // Array containing pointers to pointers to argv elements
  char **argv_ptr_ptr_arr[size];
  // Pointer to the pointer to the pointer to argv[0]
  char ***argv_ptr;
  // Pointer to argc
  int *argc_ptr;
  // Pointer to the fake return address
  int *ret_addr;

  // Calculates the length of each argument string
  int i = 0;
  struct list_elem *e;
  for (e = list_begin (args_list); e != list_end (args_list);
        e = list_next (e)) {
    struct arg *arg = list_entry (e, struct arg, elem);
    length_arr[i] = strlen((const char *) arg->str) + 1;
    i++;
  }

  // Sets up argv[i] adresses
  for (int i = size - 1; i >= 0; i--) {
    if (i == size - 1) {
      argv_ptr_arr[i] = PHYS_BASE - length_arr[i];
    } else {
      argv_ptr_arr[i] = argv_ptr_arr[i + 1] - length_arr[i];
    }
  }

  // Puts argv[i] on the stack, where 0 <= i <= size - 1
  i = 0;
  for (e = list_begin (args_list); e != list_end (args_list);
        e = list_next (e)) {
    struct arg *arg = list_entry (e, struct arg, elem);
    strlcpy((char *) argv_ptr_arr[i], arg->str, length_arr[i]);
    // hex_dump (0, argv_ptr_arr[i], length_arr[i], true);
    i++;
  }

  // Aligns the next address to a multiple of 4
  uint32_t align_size = ((uint32_t) argv_ptr_arr) % 4;
  align_addr = (uint8_t *) (argv_ptr_arr[0] - align_size);
  memset(align_addr, 0, align_size);
  // hex_dump (0, align_addr, align_size * sizeof(uint8_t), false);

  // Sets up null pointer sentinel
  null_ptr_sentinel = (char **) ((uint32_t) align_addr - sizeof(char *));
  // hex_dump (0, null_ptr_sentinel, sizeof(char *), false);

  // Puts a pointer to argv[i] on the stack, where 0 <= i <= size - 1 
  for (int i = size - 1; i >= 0; i--) {
    if (i == size - 1) {
      argv_ptr_ptr_arr[i] = (char **) ((uint32_t) null_ptr_sentinel - sizeof(char *));
    } else {
      argv_ptr_ptr_arr[i] = (char **) ((uint32_t) argv_ptr_ptr_arr[i + 1] - sizeof(char *));
    }

    int num = argv_ptr_arr[i];

    memcpy(argv_ptr_ptr_arr[i], &num, sizeof(char *));
    // hex_dump (0, argv_ptr_ptr_arr[i], sizeof(char *), false);
  }

  // Sets up argv pointer
  argv_ptr = (char ***) ((uint32_t) argv_ptr_ptr_arr[0] - sizeof(char **));
  int num = argv_ptr_ptr_arr[0];
  memcpy(argv_ptr, &num, sizeof(char **));
  // hex_dump (0, argv_ptr, sizeof(char **), false);

  // Sets up argc on the stack
  argc_ptr = (int *) ((uint32_t) argv_ptr - sizeof(char ***));
  memcpy(argc_ptr, &size, sizeof(int));
  // hex_dump (0, argc_ptr, sizeof(int), false);

  int zero = 0;

  // Sets up fake return address
  ret_addr = (int *) ((uint32_t) argc_ptr - sizeof(int *));
  memcpy(ret_addr, &zero, sizeof(int));
  // hex_dump (0, ret_addr, sizeof(int), false);

  // Sets up stack pointer 
  sp = (uint32_t) ret_addr;

  // hex_dump (0, sp, (PHYS_BASE - (uint32_t) sp), true);
  return (void *)sp;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}