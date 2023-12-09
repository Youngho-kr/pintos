#include "vm/page.h"


static unsigned vm_hash_func (const struct hash_elem *e, void *aux) {
    struct vm_entry *vme = hash_entry(e, struct vm_entry, hash_elem);

    return hash_bytes(&vme->vaddr, sizeof vme->vaddr);
}

static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct vm_entry *va = hash_entry(a, struct vm_entry, hash_elem);
    struct vm_entry *vb = hash_entry(b, struct vm_entry, hash_elem);

    return va->vaddr < vb->vaddr;
}

static void vm_destroy_func (struct hash_elem *e, void *aux) {
    struct vm_entry *vme = hash_entry(e, struct vm_entry, hash_elem);
    if(vme->is_loaded) {
        palloc_free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
        pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
    }
    free(vme);
}

void vm_init (struct hash *vm) {
    hash_init (vm, vm_hash_func, vm_less_func, NULL);
}

bool insert_vme (struct hash *vm, struct vm_entry *vme) {
    if(hash_insert(vm, &vme->hash_elem) == NULL)
        return true;
    return false;
}
bool delete_vme (struct hash *vm, struct vm_entry *vme) {
    if(hash_delete(vm, &vme->hash_elem) != NULL) {
        // vm_destroy(vm);
        return true;
    }
    return false;
}

struct vm_entry *find_vme (void *vaddr) {
    struct vm_entry vme;
    memset(&vme, 0, sizeof(struct vm_entry));
    vme.vaddr = pg_round_down(vaddr);
    // printf("find\n");
    // printf("%p %p\n", &thread_current()->vm, &vme.hash_elem);
    struct hash_elem *e = hash_find(&thread_current()->vm, &vme.hash_elem);
    // printf("==find\n");
    if (e == NULL)
        return NULL;
    
    return hash_entry(e, struct vm_entry, hash_elem);
}

void vm_destroy(struct hash *vm) {
    // printf("destroy\n");
    hash_destroy(vm, vm_destroy_func);
    // printf("==destroy\n");
}

bool load_file(void *kaddr, struct vm_entry *vme) {
    // printf("load_file\n");
    ASSERT (vme->vm_file != NULL);
    if (file_read_at(vme->vm_file, kaddr, vme->read_bytes, vme->offset) != vme->read_bytes)
        return false;

    memset (kaddr + vme->read_bytes, 0, vme->zero_bytes);

    return true;
}