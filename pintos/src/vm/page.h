//////////////////////////////// PJ3 EDITED /////////////////////////////////
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

struct vm_entry
{
  uint8_t type;                 /* VM_BIN or VM_FILE or VM_ANON */
  void *vaddr;                  /* address of virtual page */
  bool writable;                /* is writable */
  bool is_loaded;               /* is loaded */
  struct file* file;            /* file pointer for memory mapped file */

  struct list_elem mmap_elem;   /* list element of vme_list for mmap */

  size_t offset;                /* file offset */
  size_t read_bytes;            /* actual data size in virtual page */
  size_t zero_bytes;            /* size of zeros */

  size_t swap_slot;             /* swap slot */

  struct hash_elem elem;        /* element for hash table */
};

struct mmap_file {
  int mapid;                    /* mapping id */
  struct file *file;            /* mapped file object */
  struct list_elem elem;        /* list element of mmap_list */
  struct list vme_list;         /* list of corresponding vm entries */
};

struct page {
  void *kaddr;                  /* physical address of page */
  struct vm_entry *vme;         /* pointer to VM entry */
  struct thread *thread;        /* pointer to thread using this page */
  struct list_elem lru;         /* list element for lru_list */
};

void vm_init (struct hash *vm);
void vm_destroy (struct hash *vm);
bool insert_vme (struct hash *vm, struct vm_entry *vme);
bool delete_vme (struct hash *vm, struct vm_entry *vme);
struct vm_entry *find_vme (const void *vaddr);
bool load_file (void *kaddr, struct vm_entry *vme);

#endif /* vm/page.h */
/////////////////////////////////////////////////////////////////////////////
