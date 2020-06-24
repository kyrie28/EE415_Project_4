#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
//////////////////////////////// PJ2 EDITED /////////////////////////////////
#include <stdbool.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// PJ3 EDITED /////////////////////////////////
#include <string.h>
#include "threads/malloc.h"
#include "vm/frame.h"
/////////////////////////////////////////////////////////////////////////////

static void syscall_handler (struct intr_frame *);
void do_munmap (struct mmap_file *mmap_file);

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/////////////////////////////// PJ2&3 EDITED ////////////////////////////////
/* Check whether esp is in user area. Using system call number
   which is at the top of the stack, it determines the type
   of system call. it gets arguments from user stack, and
   check the address type of argument is in user area.
   Finally, it calls system call function with argument. */
static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int arg[3];

  check_address (f->esp, f->esp);
  switch (*(int *)f->esp)
  {
    case SYS_HALT:                   /* Halt the operating system. */
      halt ();
      break;
    case SYS_EXIT:                   /* Terminate this process. */
      get_argument (f->esp, arg, 1);
      exit (arg[0]);
      break;
    case SYS_EXEC:                   /* Start another process. */
      get_argument (f->esp, arg, 1);
      check_valid_string ((void *)arg[0], f->esp);
      f->eax = (uint32_t)exec ((char *)arg[0]);
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      get_argument (f->esp, arg, 1);
      f->eax = (uint32_t)wait (arg[0]);
      break;
    case SYS_CREATE:                 /* Create a file. */
      get_argument (f->esp, arg, 2);
      check_valid_string ((void *)arg[0], f->esp);
      f->eax = (uint32_t)create ((char *)arg[0], (unsigned)arg[1]);
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      get_argument (f->esp, arg, 1);
      check_valid_string ((void *)arg[0], f->esp);
      f->eax = (uint32_t)remove ((char *)arg[0]);
      break;
    case SYS_OPEN:                   /* Open a file. */
      get_argument (f->esp, arg, 1);
      check_valid_string ((void *)arg[0], f->esp);
      f->eax = (uint32_t)open ((char *)arg[0]);
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      get_argument (f->esp, arg, 1);
      f->eax = (uint32_t)filesize (arg[0]);
      break;
    case SYS_READ:                   /* Read from a file. */
      get_argument (f->esp, arg, 3);
      check_valid_buffer ((void *)arg[1], (unsigned)arg[2], f->esp, true);
      f->eax = (uint32_t)read (arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_WRITE:                  /* Write to a file. */
      get_argument (f->esp, arg, 3);
      check_valid_buffer ((void *)arg[1], (unsigned)arg[2], f->esp, false);
      f->eax = (uint32_t)write (arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      get_argument (f->esp, arg, 2);
      seek (arg[0], (unsigned)arg[1]);
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      get_argument (f->esp, arg, 1);
      f->eax = tell (arg[0]);
      break;
    case SYS_CLOSE:                  /* Close a file. */
      get_argument (f->esp, arg, 1);
      close (arg[0]);
      break;
    case SYS_MMAP:                   /* Map a file into memory. */
      get_argument (f->esp, arg, 2);
      f->eax = mmap (arg[0], (void *)arg[1]);
      break;
    case SYS_MUNMAP:                 /* Remove a memory mapping. */
      get_argument (f->esp, arg, 1);
      munmap (arg[0]);
      break;
    default:
      printf ("Error: invalid system call %d\n", *(int *)f->esp);
      thread_exit ();
  }
}

/* Verify validity of a user-provided pointer.
   Check if pointer points below PHYS_BASE,
   and if given address is mapped by page table.
   return vm entry pointer if it exists.
   otherwise, return NULL. */
struct vm_entry *
check_address (const void *addr, void *esp UNUSED)
{
  if (addr < (void *)0x08048000 || !is_user_vaddr (addr))
    exit (-1);

  return find_vme (addr);
}
/////////////////////////////////////////////////////////////////////////////

//////////////////////////////// PJ3 EDITED /////////////////////////////////
/* for read() and write() system call, we should check
   whether the buffer is valid or not.
   check its validity using check_address() */
void
check_valid_buffer (void *buffer, unsigned size, void *esp UNUSED, bool to_write)
{
  unsigned i;

  for (i = 0; i < size; i++)
  {
    struct vm_entry *vme = check_address (buffer + i, esp);

    if (vme == NULL)
      exit (-1);
    if (to_write && !vme->writable)
      exit (-1);
  }
}

/* for system calls that are using string for their argument,
   we should check whether the string is valid or not.
   check its validity using check_address() */
void
check_valid_string (const void *str, void *esp UNUSED)
{
  unsigned i;

  for (i = 0; i <= strlen (str); i++)
  {
    struct vm_entry *vme = check_address (str + i, esp);

    if (vme == NULL)
      exit (-1);
  }
}
/////////////////////////////////////////////////////////////////////////////

//////////////////////////////// PJ2 EDITED /////////////////////////////////
/* Copy 'count' number of arguments in stack pointed by 'esp'
   to 'arg' in the same order.
   Check whether address is in user area.*/
void
get_argument (void *esp, int *arg, int count)
{
  int i;

  for (i = 0; i < count; i++)
  {
    check_address (esp + 4 * (i + 1), esp);
    arg[i] = *(int *)(esp + 4 * (i + 1));
  }
}

/* Shut down the operating system. */
void
halt (void)
{
  shutdown_power_off ();
}

/* Print process name and exit status of this process
   and terminate it. */
void
exit (int status)
{
  struct thread *t = thread_current ();

  t->exit_status = status;
  printf ("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}

/* Execute child process with cmd_line.
   Wait until loading is completed and return pid */
pid_t
exec (const char *cmd_line)
{
  pid_t pid;
  struct thread *cp;

  pid = process_execute (cmd_line);
  if (pid == TID_ERROR)
    return FAILURE;

  cp = get_child_process (pid);
  if (cp == NULL)
    return FAILURE;

  sema_down (&cp->load_sema);
  ASSERT (cp->load_flag != DEFAULT);
  if (cp->load_flag == FAILURE)
    return FAILURE;

  return pid;
}

/* Wait until process PID exits. */
int
wait (pid_t pid)
{
  return process_wait (pid);
}

/* Create a file with file name 'file' and size 'initial_size'. */
bool
create (const char *file, unsigned initial_size)
{
  return filesys_create (file, initial_size);
}

/* Delete a file with file name 'file'. */
bool
remove (const char *file)
{
  return filesys_remove (file);
}

/* Open file whose name is 'file'
   and add file object to file descriptor table.
   Return file descriptor.
   If file doesn't exist, then return -1 */
int
open (const char *file)
{
  struct file *f = filesys_open (file);

  if (f == NULL)
    return -1;
  return process_add_file (f);
}

/* Search file by fd and return its length.
   f file doesn't exist, then return -1 */
int
filesize (int fd)
{
  struct file *f = process_get_file (fd);

  if (f == NULL)
    return -1;
  return file_length (f);
}

/* If fd = STDIN_FILENO then get characters from input buffer
   and store them into given buffer.
   If fd != STDIN_FILENO then read characters from file
   which corresponds to fd and store them into given buffer.
   Return bytes it reads from file. */
int
read (int fd, void *buffer, unsigned size)
{
  struct file *f;
  int bytes = 0;

  /* Acquire lock for the case of multiple file access. */
  lock_acquire (&filesys_lock);
  if (fd == STDIN_FILENO)
  {
    uint8_t byte;

    while ((byte = input_getc ()) != '\0' && (unsigned)bytes < size)
      *(uint8_t *)(buffer + bytes++) = byte;
  }
  else
  {
    f = process_get_file (fd);
    if (f == NULL)
    {
      lock_release (&filesys_lock);
      return -1;
    }
    bytes = file_read (f, buffer, size);
  }
  lock_release (&filesys_lock);

  return bytes;
}

/* If fd = STDOUT_FILENO then print 'size'
   number of characters to screen. If
   fd != STDOUT_FILENO then write characters
   to file which corresponds to fd.
   Return bytes it writes to file. */
int
write (int fd, const void *buffer, unsigned size)
{
  struct file *f;
  int bytes = 0;

  /* Acquire lock for the case of multiple file access. */
  lock_acquire (&filesys_lock);
  if (fd == STDOUT_FILENO)
  {
    putbuf ((char *)buffer, size);
    bytes = size;
  }
  else
  {
    f = process_get_file (fd);
    if (f == NULL)
    {
      lock_release (&filesys_lock);
      return -1;
    }
    bytes = file_write (f, buffer, size);
  }
  lock_release (&filesys_lock);

  return bytes;
}

/* Changes the next byte to be read
   or written in open file fd to position. */
void
seek (int fd, unsigned position)
{
  struct file *f = process_get_file (fd);

  if (f != NULL)
    file_seek (f, position);
}

/* Return the position of the next byte
   to be read or written in open file fd. */
unsigned
tell (int fd)
{
  struct file *f = process_get_file (fd);

  if (f == NULL)
    return 0;
  return file_tell (f);
}

/* Close file descriptor fd */
void
close (int fd)
{
  process_close_file (fd);
}
/////////////////////////////////////////////////////////////////////////////

//////////////////////////////// PJ3 EDITED /////////////////////////////////
/* Load file data into ADDR by demand paging.
   Mmaped page ADDR is swapped out to its original file.
   For fragmented page, fill the unused fraction of page with zero.
   If success, returns mapping_id. Otherwise, return -1. */
int
mmap (int fd, void *addr)
{
  struct mmap_file *mmf;
  struct file *file_;
  off_t read_bytes;
  size_t zero_bytes, ofs;
  struct list_elem *e, *next;

  /* Check arguments. */
  if (fd == STDIN_FILENO || \
      fd == STDOUT_FILENO || \
      addr == NULL || \
      (uintptr_t)addr % PGSIZE != 0 || \
      find_vme (addr) != NULL)
    return -1;

  /* Get and reopen file. */
  if ((file_ = process_get_file (fd)) == NULL)
    return -1;
  if ((read_bytes = file_length (file_)) <= 0)
    return -1;
  if ((file_ = file_reopen (file_)) == NULL)
    return -1;

  /* Initialize mmap_file. */
  if ((mmf = (struct mmap_file *)malloc (sizeof (struct mmap_file))) == NULL)
    return -1;
  memset (mmf, 0, sizeof *mmf);
  mmf->mapid = fd;
  mmf->file = file_;
  list_init (&mmf->vme_list);

  zero_bytes = PGSIZE - (read_bytes % PGSIZE);
  ofs = 0;
  while (read_bytes > 0 || zero_bytes > 0)
  {
    struct vm_entry *vme;

    /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = (size_t)read_bytes < PGSIZE ? (size_t)read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Construct VM Entries. */
    vme = (struct vm_entry *)malloc (sizeof (struct vm_entry));
    if (vme == NULL)
      goto mmap_fail;
    memset (vme, 0, sizeof *vme);

    vme->type = VM_FILE;
    vme->vaddr = addr;
    vme->writable = true;
    vme->file = mmf->file;

    vme->offset = ofs;
    vme->read_bytes = page_read_bytes;
    vme->zero_bytes = page_zero_bytes;

    /* Insert VM Element to hash table. */
    if (!insert_vme (&thread_current ()->vm, vme))
      goto mmap_fail;

    /* Insert VM Element to vme_list of mmap_file. */
    list_push_back (&mmf->vme_list, &vme->mmap_elem);

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    ofs += page_read_bytes;
    addr += PGSIZE;
  }

  /* Insert mmap_file to mmap_list. */
  list_push_back (&thread_current ()->mmap_list, &mmf->elem);

  return mmf->mapid;

mmap_fail:
  for (e = list_begin (&mmf->vme_list);
       e != list_end (&mmf->vme_list);
       e = next)
  {
    struct vm_entry *cur_vme = list_entry (e, struct vm_entry, mmap_elem);

    next = list_next (e);
    delete_vme (&thread_current ()->vm, cur_vme);
    free (cur_vme);
  }
  free (mmf);

  return -1;
}

/* Unmap the mappings in the mmap_list
   which has not been previously unmapped. */
void
munmap (mapid_t mapid)
{
  struct list *mml = &thread_current ()->mmap_list;
  struct list_elem *e, *next;

  for (e = list_begin (mml); e != list_end (mml); e = next)
  {
    struct mmap_file *mmf = list_entry (e, struct mmap_file, elem);

    next = list_next (e);
    if (mapid == CLOSE_ALL || mmf->mapid == mapid)
    {
      list_remove (e);
      do_munmap (mmf);
    }
  }
}

void
do_munmap (struct mmap_file *mmap_file)
{
  struct list *vmel = &mmap_file->vme_list;
  struct list_elem *e, *next;
  struct file *file = mmap_file->file;

  for (e = list_begin (vmel); e != list_end (vmel); e = next)
  {
    struct vm_entry *vme = list_entry (e, struct vm_entry, mmap_elem);

    next = list_next (e);
    if (pagedir_is_dirty (thread_current ()->pagedir, vme->vaddr))
      file_write_at (file, vme->vaddr, vme->read_bytes, vme->offset);
    ASSERT (delete_vme (&thread_current ()->vm, vme));
    free_page (pagedir_get_page(thread_current ()->pagedir, vme->vaddr));
    free (vme);
  }

  file_close (file);
}
/////////////////////////////////////////////////////////////////////////////
