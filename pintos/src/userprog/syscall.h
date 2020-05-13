#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
//////////////////////////////// PJ2 EDITED /////////////////////////////////
#include <stdbool.h>
#include <user/syscall.h>

struct lock filesys_lock;
/////////////////////////////////////////////////////////////////////////////

void syscall_init (void);

//////////////////////////////// PJ2 EDITED /////////////////////////////////
void check_address (void *addr);
void get_argument (void *esp, int *arg, int count);

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
/////////////////////////////////////////////////////////////////////////////

#endif /* userprog/syscall.h */
