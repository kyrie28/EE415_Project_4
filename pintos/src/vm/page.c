//////////////////////////////// PJ3 EDITED /////////////////////////////////
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include <string.h>
#include <stdio.h>
#include "userprog/pagedir.h"
#include "vm/frame.h"

static unsigned vm_hash_func (const struct hash_elem *e, void *aux);
static bool vm_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux);
static void vm_destroy_func (struct hash_elem *e, void *aux UNUSED);

/* initialize hash table for vm. */
void
vm_init (struct hash *vm)
{
  ASSERT (vm != NULL);
  hash_init (vm, vm_hash_func, vm_less_func, NULL);
}

/* destroy hash table for vm. */
void
vm_destroy (struct hash *vm)
{
  ASSERT (vm != NULL);
  hash_destroy (vm, vm_destroy_func);
}

/* hash function for vm. it uses hash_int (). */
static unsigned
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  ASSERT (e != NULL);
  return hash_int ((int)hash_entry (e, struct vm_entry, elem)->vaddr);
}

/* compare funtion for hash table. */
static bool
vm_less_func (const struct hash_elem *a,
              const struct hash_elem *b,
              void *aux UNUSED)
{
  struct vm_entry *va;
  struct vm_entry *vb;

  ASSERT (a != NULL && b != NULL);
  va = hash_entry (a, struct vm_entry, elem);
  vb = hash_entry (b, struct vm_entry, elem);

  if (va->vaddr < vb->vaddr)
    return true;
  else
    return false;
}

/* funtion that destroys vm entries for hash table. */
static void
vm_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  ASSERT (e != NULL);
  free (hash_entry (e, struct vm_entry, elem));
}

/* insert vm element to vm hash table.
   return true if success, false otherwise. */
bool
insert_vme (struct hash *vm, struct vm_entry *vme)
{
  ASSERT (vm != NULL && vme != NULL);
  if (hash_insert (vm, &vme->elem) == NULL)
    return true;
  else
    return false;
}

/* delete vm element from vm hash table.
   return true if success, false otherwise. */
bool
delete_vme (struct hash *vm, struct vm_entry *vme)
{
  ASSERT (vm != NULL && vme != NULL);
  if (hash_delete (vm, &vme->elem) != NULL)
    return true;
  else
    return false;
}

/* find corresponding vm_entry using vaddr in hash table.
   return its address if success, or return NULL otherwise. */
struct vm_entry *
find_vme (const void *vaddr)
{
  struct vm_entry vme_temp;
  struct hash_elem *found;

  vme_temp.vaddr = pg_round_down (vaddr);
  found = hash_find (&thread_current ()->vm, &vme_temp.elem);
  if (found != NULL)
    return hash_entry (found, struct vm_entry, elem);
  else
    return NULL;
}


/* load file, which is saved in vme, to kaddr.
   if success, return true. Otherwise, return false. */
bool
load_file (void *kaddr, struct vm_entry *vme)
{
  ASSERT (kaddr != NULL && vme != NULL);
  if (vme->read_bytes != (size_t)file_read_at (vme->file, kaddr, vme->read_bytes, vme->offset))
    return false;
  memset (kaddr + vme->read_bytes, 0, vme->zero_bytes);
  return true;
}
/////////////////////////////////////////////////////////////////////////////
