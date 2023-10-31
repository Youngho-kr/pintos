#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
void sys_halt (void) NO_RETURN;
void sys_exit (int status) NO_RETURN;
tid_t sys_exec (const char *file);
int sys_wait (pid_t);
bool sys_create (const char *file, unsigned initial_size);
bool sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
int sys_read (int fd, void *buffer, unsigned length);
int sys_write (int fd, const void *buffer, unsigned length);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);

/*
   All file system call have check file descriptor is valid
   Check fd < 2 or fd > MAX_FD
   fd = 0 : stdout
   fd = 1 : stdout
   if thread fd is NULL, return -1
*/

/* File synchronize */
struct lock syscall_lock;

/* Init for system call */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* File open, close, write, read sync */
  lock_init(&syscall_lock);
}

/* Check ptr is valid address */
void checkPtr(void *ptr) {
  checkAddress(ptr);
  checkAddress(ptr + 3);
}
void checkAddress(void *ptr) {
  if(ptr == NULL)
    sys_exit(-1);
  if(is_kernel_vaddr(ptr))
    sys_exit(-1);
  if(pagedir_get_page(thread_current()->pagedir, ptr) == NULL)
    sys_exit(-1);
}

/* system call handler */
/* Shutdown */
void sys_halt() {
  shutdown_power_off();
}
/* Exit with exit code */
void sys_exit(int status) {
  struct thread *cur = thread_current();
  cur->exit_status = status;

  printf("%s: exit(%d)\n", cur->name, cur->exit_status);

  thread_exit();
}
/* Execute file 
   return child process id
*/
tid_t sys_exec(const char *cmd_line) {
  checkPtr(cmd_line);

  return process_execute(cmd_line);
}
/* Wait exec file 
   return exit code 
*/
int sys_wait(tid_t pid) {
  return process_wait(pid);
}
/* Create file with file name and initial size 
   return true if success, otherwise false
*/
bool sys_create(const char *file, unsigned initial_size) {
  checkPtr(file);
  return filesys_create(file, initial_size);
}
/* Remove file with file name 
   return true if success, otherwise false
*/
bool sys_remove(const char *file) {
  checkPtr(file);
  return filesys_remove(file);
}
/* Open file with file name
   return true if success, otherwise false
*/
int sys_open(const char *file) {
  checkPtr(file);
  
  struct thread *cur = thread_current();

  int idx;
  for(idx = 2; idx < MAX_FD; idx++) {
    if(cur->fd[idx] == NULL)
      break;
  }
  if(idx == MAX_FD)
    return -1;

  lock_acquire(&syscall_lock);

  cur->fd[idx] = filesys_open(file);

  if(cur->fd[idx] == NULL) {
    lock_release(&syscall_lock);
    return -1;
  }
  
  /* Deny write for executing file */
  if(strcmp(cur->name, file) == 0)
    file_deny_write(cur->fd[idx]);

  lock_release(&syscall_lock);

  return idx;
}
/* Return file size for fd */
int sys_filesize(int fd) {
  if(fd < 2 || fd >= MAX_FD)
    return -1;
  struct thread *cur = thread_current();
  if(cur->fd[fd] == NULL)
    return -1;
  return file_length(cur->fd[fd]);
}
/* Read file fd 
   return actually read bytes
*/
int sys_read(int fd, void *buffer, unsigned size) {
  checkPtr(buffer);
  if(fd == 0) {
    int count = 0;
    for(int i = 0; i < size; i++) {
      input_getc(buffer + i);
      count++;
    }
    return count;
  }
  else if(fd < 2 || fd >= MAX_FD) {
    return -1;
  }
  else {
    struct thread *cur = thread_current();

    lock_acquire(&syscall_lock);

    if(cur->fd[fd] == NULL) {
      lock_release(&syscall_lock);
      return -1;
    }
    int result = file_read(cur->fd[fd], buffer, size);

    lock_release(&syscall_lock);

    return result;
  }
}
/* Write file fd 
   return actually write bytes
*/
int sys_write(int fd, const void *buffer, unsigned size) {
  checkPtr(buffer);
  if(fd == 1) {
    int count = 0;
    for(int i = 0; i < size; i++) {
      putbuf(buffer + i, 1);
      count++;
    }
    return count;
  }
  else if(fd < 2 || fd >= MAX_FD) {
    return -1;
  }
  else {
    struct thread *cur = thread_current();

    lock_acquire(&syscall_lock);

    if(cur->fd[fd] == NULL) {
      lock_release(&syscall_lock);
      return -1;
    }
    int result = file_write(cur->fd[fd], buffer, size);
    lock_release(&syscall_lock);
    return result;
  }
}
/* Change file offset */
void sys_seek(int fd, unsigned position) {
  if(fd < 2 || fd >= MAX_FD)
    return -1;
  struct thread *cur = thread_current();
  if(cur->fd[fd] == NULL)
    return -1;
  file_seek(cur->fd[fd], position);
}
/* Return file offset */
unsigned sys_tell(int fd) {
  if(fd < 2 || fd >= MAX_FD)
    return -1;
  struct thread *cur = thread_current();
  if(cur->fd[fd] == NULL)
    return -1;
  return file_tell(cur->fd[fd]);
}
/* Close file fd */
void sys_close(int fd) {
  if(fd < 2 || fd >= MAX_FD)
    sys_exit(-1);
  struct thread *cur = thread_current();
  if(cur->fd[fd] == NULL)
    sys_exit(-1);
  file_close(cur->fd[fd]);
  cur->fd[fd] = NULL;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  checkPtr(f->esp);

  // hex_dump(f->esp, f->esp, 100, 1);

  switch(*(int *)(f->esp)) {
    /* Project 1 */
    case SYS_HALT:                   /* Halt the operating system. */
      sys_halt();
      break;
    case SYS_EXIT:                   /* Terminate this process. */
      checkPtr((int *)(f->esp + 4));
      sys_exit(*(int *)(f->esp + 4));
      break;
    case SYS_EXEC:                   /* Start another process. */
      checkPtr((int *)(f->esp + 4));
      f->eax = sys_exec(*(char **)(f->esp + 4));
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      checkPtr((int *)(f->esp + 4));
      f->eax = sys_wait(*(tid_t *)(f->esp + 4));
      break;
    /* Project 2 */
    case SYS_CREATE:                 /* Create a file. */
      checkPtr((int *)(f->esp + 4));
      checkPtr((int *)(f->esp + 8));
      f->eax = sys_create(*(char **)(f->esp + 4), *(unsigned *)(f->esp + 8));
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      checkPtr((int *)(f->esp + 4));
      f->eax = sys_remove(*(char **)(f->esp + 4));
      break;
    case SYS_OPEN:                   /* Open a file. */
      checkPtr((int *)(f->esp + 4));
      f->eax = sys_open(*(char **)(f->esp + 4));
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      checkPtr((int *)(f->esp + 4));
      f->eax = sys_filesize(*(int *)(f->esp + 4));
      break;
    case SYS_READ:                   /* Read from a file. */
      checkPtr((int *)(f->esp + 4));
      checkPtr((int *)(f->esp + 8));
      checkPtr((int *)(f->esp + 12));
      f->eax = sys_read(*(int *)(f->esp + 4), *(char **)(f->esp + 8), *(unsigned *)(f->esp + 12));
      break;
    case SYS_WRITE:                  /* Write to a file. */
      checkPtr((int *)(f->esp + 4));
      checkPtr((int *)(f->esp + 8));
      checkPtr((int *)(f->esp + 12));
      f->eax = sys_write(*(int *)(f->esp + 4), *(char **)(f->esp + 8), *(unsigned *)(f->esp + 12));
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      checkPtr((int *)(f->esp + 4));
      checkPtr((int *)(f->esp + 8));
      sys_seek(*(int *)(f->esp + 4), *(int *)(f->esp + 8));
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      checkPtr((int *)(f->esp + 4));
      f->eax = sys_tell(*(int *)(f->esp + 4));
      break;
    case SYS_CLOSE:                  /* Close a file. */
      checkPtr((int *)(f->esp + 4));
      sys_close(*(int *)(f->esp + 4));
      break;

    /* Project 3 and optionally project 4. */
    case SYS_MMAP:                   /* Map a file into memory. */
    case SYS_MUNMAP:                 /* Remove a memory mapping. */

    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
    case SYS_MKDIR:                  /* Create a directory. */
    case SYS_READDIR:                /* Reads a directory entry. */
    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
    case SYS_INUMBER:
      break;
  }
}
