#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "filesys/file.h"
#include <inttypes.h>

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
int sys_mmap (int fd, void *addr);
void sys_munmap (int mapid);
void do_munmap(struct mmap_file *mmap_file);
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
// void checkPtr(void *ptr) {
//   // checkAddress(ptr);
//   // checkAddress(ptr + 3);
// }
// void checkAddress(void *ptr) {
//   if(ptr == NULL)
//     sys_exit(-1);
//   if(is_kernel_vaddr(ptr))
//     sys_exit(-1);
//   if(pagedir_get_page(thread_current()->pagedir, ptr) == NULL)
//     sys_exit(-1);
// }

struct vm_entry *check_address (void *addr) {
  // printf("check address %p\n", addr);
  if (addr == NULL)
    sys_exit(-1);
  if (addr < (void *)0x08048000 || addr >= (void *)PHYS_BASE)
    sys_exit(-1);
  return find_vme(addr);
}
/* Function for Read system call */
void check_valid_buffer (void *buffer,  unsigned size, bool to_write) {
  // printf("buffer\n");
  for (unsigned i = 0; i < size; i++) {
    struct vm_entry *vme = check_address(buffer + i);
    if(vme == NULL)
      sys_exit(-1);
    // if(to_write)
    //   sys_exit(-1);
    if(!vme->writeable)  
      sys_exit(-1);
  }
  // printf("==buffer\n");
}
/* Function for Write system call */
void check_valid_string (const void *str) {
  struct vm_entry *vme = check_address(str);
  if(vme == NULL)
    sys_exit(-1);
}

/* system call handler */
/* Shutdown */
void sys_halt() {
  shutdown_power_off();
}
/* Exit with exit code */
void sys_exit(int status) {
  /* Release all lock */
  if(syscall_lock.holder == thread_current())
    lock_release(&syscall_lock);

  struct thread *cur = thread_current();
  cur->exit_status = status;

  printf("%s: exit(%d)\n", cur->name, cur->exit_status);

  thread_exit();
}
/* Execute file 
   return child process id
*/
tid_t sys_exec(const char *cmd_line) {
  // printf("SYS_EXEC: %s\n", cmd_line);
  tid_t result;
  lock_acquire(&syscall_lock);
  result = process_execute(cmd_line);
  lock_release(&syscall_lock);
  return result;
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
  // checkPtr(file);
  bool result;
  lock_acquire(&syscall_lock);
  result = filesys_create(file, initial_size);
  lock_release(&syscall_lock);
  return result;
}
/* Remove file with file name 
   return true if success, otherwise false
*/
bool sys_remove(const char *file) {
  // checkPtr(file);
  bool result;
  lock_acquire(&syscall_lock);
  result = filesys_remove(file);
  lock_release(&syscall_lock);
  return result;
}
/* Open file with file name
   return true if success, otherwise false
*/
int sys_open(const char *file) {
  // checkPtr(file);
  lock_acquire(&syscall_lock);
  
  struct thread *cur = thread_current();

  int idx;
  for(idx = 2; idx < MAX_FD; idx++) {
    if(cur->fd[idx] == NULL)
      break;
  }
  if(idx == MAX_FD) {
    lock_release(&syscall_lock);
    return -1;
  }

  cur->fd[idx] = filesys_open(file);

  if(cur->fd[idx] == NULL) {
    lock_release(&syscall_lock);
    return -1;
  }

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
  // checkPtr(buffer);
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
    lock_acquire(&syscall_lock);

    struct thread *cur = thread_current();

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
  // checkPtr(buffer);
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
    lock_acquire(&syscall_lock);

    struct thread *cur = thread_current();

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
/* Mapping file to virtual memory */
int sys_mmap (int fd, void *addr) {
  if (addr == NULL || (int)addr % PGSIZE != 0) 
    return -1;

  if (fd < 2 || fd >= MAX_FD)
    return -1;

  struct file *file = thread_current()->fd[fd];
  if(file == NULL)
    return -1;

  struct mmap_file *mmap_file = (struct mmap_file *)malloc(sizeof(struct mmap_file));
  if (mmap_file == NULL)
    return -1;
  mmap_file->mm_file = file_reopen(file);
  mmap_file->mapid = thread_current()->mapid++;
  list_init(&mmap_file->vme_list);

  int file_len = file_length(mmap_file->mm_file);
  int offset = 0;
  int read_bytes;
  int zero_bytes;
  // printf("filelen: %d, PGSIZE: %d\n", file_len, PGSIZE);
  while (file_len > 0) {
    if (file_len >= PGSIZE) {
      read_bytes = PGSIZE;
      zero_bytes = 0;
    }
    else {
      read_bytes = file_len;
      zero_bytes = PGSIZE - file_len;
    }

    if (find_vme(addr) != NULL) {
      sys_munmap(mmap_file->mapid);
      return -1;
    }
    struct vm_entry *vme = (struct vm_entry *)malloc(sizeof(struct vm_entry));

    vme->type = VM_FILE;
    vme->vaddr = addr;
    vme->writeable = true;
    vme->is_loaded = false;
    vme->vm_file = mmap_file->mm_file;
    vme->offset = offset;
    vme->read_bytes = read_bytes;
    vme->zero_bytes = zero_bytes;

    if(!insert_vme(&thread_current()->vm, vme))
      return -1;

    list_push_back(&mmap_file->vme_list, &vme->mmap_elem);

    file_len -= read_bytes;
    offset += read_bytes;
    addr += PGSIZE;
  }

  list_push_back(&thread_current()->mmap_list, &mmap_file->elem);

  return mmap_file->mapid;
}
void sys_munmap (int mapid) {
  struct thread *cur = thread_current();
  struct list_elem *e;
  for(e = list_begin(&cur->mmap_list);
      e != list_end(&cur->mmap_list);
      )
      {
        struct mmap_file *mmap_file = list_entry(e, struct mmap_file, elem);
        if(mmap_file->mapid == mapid) {
          do_munmap(mmap_file);
          e = list_remove(&mmap_file->elem);
          free(mmap_file);
        }
        else {
          e = list_next(e);
        }
      }
}
void do_munmap(struct mmap_file *mmap_file) {
  struct thread *cur = thread_current();
  struct list_elem *e;
  for(e = list_begin(&mmap_file->vme_list);
      e != list_end(&mmap_file->vme_list);
      )
      {
        // printf("do munmap\n");
        struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);
        // printf("do munmap find %d\n", vme->read_bytes);
        // if(vme->is_loaded) {
          if(pagedir_is_dirty(cur->pagedir, vme->vaddr)) {
            // lock_acquire(&syscall_lock);
            file_write_at(vme->vm_file, vme->vaddr, vme->read_bytes, vme->offset);
            // lock_release(&syscall_lock);
          }
          // palloc_free_page(pagedir_get_page(cur->pagedir, vme->vaddr));
        // }
        e = list_remove(&vme->mmap_elem);
        delete_vme(&cur->vm, vme);
        free(vme);
        // printf("do munmap free\n");
      }

  // lock_acquire(&syscall_lock);
  // file_close(mmap_file->mm_file);
  // lock_release(&syscall_lock);
  // printf("end do munmap\n");

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_address(f->esp);

  // printf("syscall handler\n");
  // hex_dump(f->esp, f->esp, 100, 1);

  switch(*(int *)(f->esp)) {
    /* Project 1 */
    case SYS_HALT:                   /* Halt the operating system. */
      sys_halt();
      break;
    case SYS_EXIT:                   /* Terminate this process. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      sys_exit(*(int *)(f->esp + 4));
      break;
    case SYS_EXEC:                   /* Start another process. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      check_valid_string(*(void **)(f->esp + 4));
      f->eax = sys_exec(*(char **)(f->esp + 4));
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      f->eax = sys_wait(*(tid_t *)(f->esp + 4));
      break;
    /* Project 2 */
    case SYS_CREATE:                 /* Create a file. */
      // checkPtr((void *)(f->esp + 4));
      // checkPtr((void *)(f->esp + 8));
      check_address((void *)(f->esp + 4));
      check_address((void *)(f->esp + 8));
      check_valid_string(*(char **)(f->esp + 4));
      f->eax = sys_create(*(char **)(f->esp + 4), *(unsigned *)(f->esp + 8));
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      // checkPtr((int *)(f->esp + 4));
      check_valid_string((void *)(f->esp + 4));
      f->eax = sys_remove(*(char **)(f->esp + 4));
      break;
    case SYS_OPEN:                   /* Open a file. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      check_valid_string(*(char **)(f->esp + 4));
      f->eax = sys_open(*(char **)(f->esp + 4));
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      f->eax = sys_filesize(*(int *)(f->esp + 4));
      break;
    case SYS_READ:                   /* Read from a file. */
      // checkPtr((int *)(f->esp + 4));
      // checkPtr((int *)(f->esp + 8));
      // checkPtr((int *)(f->esp + 12));
      check_address((void *)(f->esp + 4));
      check_address((void *)(f->esp + 8));
      check_address((void *)(f->esp + 12));
      check_valid_buffer(*(char **)(f->esp + 8), *(unsigned *)(f->esp + 12), false);
      int result = sys_read(*(int *)(f->esp + 4), *(void **)(f->esp + 8), *(unsigned *)(f->esp + 12));
      f->eax = result;
      break;
    case SYS_WRITE:                  /* Write to a file. */
      // checkPtr((int *)(f->esp + 4));
      // checkPtr((int *)(f->esp + 8));
      // checkPtr((int *)(f->esp + 12));
      check_address((void *)(f->esp + 4));
      check_address((void *)(f->esp + 8));
      check_address((void *)(f->esp + 12));
      check_valid_string(*(char **)(f->esp + 8));
      f->eax = sys_write(*(int *)(f->esp + 4), *(void **)(f->esp + 8), *(unsigned *)(f->esp + 12));
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      // checkPtr((int *)(f->esp + 4));
      // checkPtr((int *)(f->esp + 8));
      check_address((void *)(f->esp + 4));
      check_address((void *)(f->esp + 8));
      sys_seek(*(int *)(f->esp + 4), *(unsigned int *)(f->esp + 8));
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      f->eax = sys_tell(*(unsigned int *)(f->esp + 4));
      break;
    case SYS_CLOSE:                  /* Close a file. */
      // checkPtr((int *)(f->esp + 4));
      check_address((void *)(f->esp + 4));
      sys_close(*(int *)(f->esp + 4));
      break;

    /* Project 3 and optionally project 4. */
    case SYS_MMAP:                   /* Map a file into memory. */
      check_address((void *)(f->esp + 4));
      check_address((void *)(f->esp + 8));
      // check_valid_string(*(void **)(f->esp + 8));
      f->eax = sys_mmap(*(int *)(f->esp + 4), *(void **)(f->esp + 8));
      break;
    case SYS_MUNMAP:                 /* Remove a memory mapping. */
      check_address((void *)(f->esp + 4));
      sys_munmap(*(int *)(f->esp + 4));
      break;
    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
    case SYS_MKDIR:                  /* Create a directory. */
    case SYS_READDIR:                /* Reads a directory entry. */
    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
    case SYS_INUMBER:
      break;
  }
}
