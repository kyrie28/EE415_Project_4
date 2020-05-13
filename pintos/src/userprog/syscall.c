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

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//////////////////////////////// PJ2 EDITED /////////////////////////////////
/* Check whether esp is in user area. Using system call number
   which is at the top of the stack, it determines the type
   of system call. it gets arguments from user stack, and
   check the address type of argument is in user area.
   Finally, it calls system call function with argument. */
static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int arg[3];

  check_address (f->esp);
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
      check_address ((void *)arg[0]);
      f->eax = (uint32_t)exec ((char *)arg[0]);
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      get_argument (f->esp, arg, 1);
      f->eax = (uint32_t)wait (arg[0]);
      break;
    case SYS_CREATE:                 /* Create a file. */
      get_argument (f->esp, arg, 2);
      check_address ((void *)arg[0]);
      f->eax = (uint32_t)create ((char *)arg[0], (unsigned)arg[1]);
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      get_argument (f->esp, arg, 1);
      check_address ((void *)arg[0]);
      f->eax = (uint32_t)remove ((char *)arg[0]);
      break;
    case SYS_OPEN:                   /* Open a file. */
      get_argument (f->esp, arg, 1);
      check_address ((void *)arg[0]);
      f->eax = (uint32_t)open ((char *)arg[0]);
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      get_argument (f->esp, arg, 1);
      f->eax = (uint32_t)filesize (arg[0]);
      break;
    case SYS_READ:                   /* Read from a file. */
      get_argument (f->esp, arg, 3);
      check_address ((void *)arg[1]);
      f->eax = (uint32_t)read (arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_WRITE:                  /* Write to a file. */
      get_argument (f->esp, arg, 3);
      check_address ((void *)arg[1]);
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
    default:
      thread_exit ();
  }
}

/* Verify validity of a user-provided pointer.
   Check if pointer points below PHYS_BASE,
   and if given address is mapped by page table. */
void
check_address (void *addr)
{
  if (!is_user_vaddr (addr))
    exit (-1);
  else
  {
    struct thread *t = thread_current ();

    if (pagedir_get_page (t->pagedir, addr) == NULL)
      exit (-1);
  }
}

/* Copy 'count' number of arguments in stack pointed by 'esp'
   to 'arg' in the same order.
   Check whether address is in user area.*/
void
get_argument (void *esp, int *arg, int count)
{
  int i;

  for (i = 0; i < count; i++)
  {
    check_address (esp + 4 * (i + 1));
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

/*  */
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

    while ((byte = input_getc ()) != -1 && (unsigned)bytes < size)
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
