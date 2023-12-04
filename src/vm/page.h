#ifndef PAGE_H
#define PAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <list.h>
#include <hash.h>

struct vm_entry {
    uint8_t type;
    void *vaddr;
    bool writeable;

    bool is_loaded;
    struct file* file;

    struct list_elem mmap_elem;

    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    size_t swap_slot;

    struct hash_elem elem;
};


#endif