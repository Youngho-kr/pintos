#ifndef VM_PAGE_H
#define VM_PAGE_H

// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <inttypes.h>
#include <list.h>
#include <hash.h>
#include <debug.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

struct vm_entry {
    uint8_t type;
    void *vaddr;
    bool writeable;

    bool is_loaded;
    struct file* vm_file;

    // struct list_elem mmap_elem;

    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    // size_t swap_slot;

    struct hash_elem elem;
};

void vm_init (struct hash *vm);

bool insert_vme (struct hash *vm, struct vm_entry *vme);
bool delete_vme (struct hash *vm, struct vm_entry *vme);

struct vm_entry *find_vme (void *vaddr);

void vm_entry (struct hash *vm);

bool load_file (void *kaddr, struct vm_entry *vme);

#endif