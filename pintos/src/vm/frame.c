//////////////////////////////// PJ3 EDITED /////////////////////////////////
#include "vm/frame.h"
#include <list.h>
#include "threads/synch.h"
#include "vm/swap.h"
#include <debug.h>
#include "threads/malloc.h"
#include <string.h>
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

struct list lru_list;
struct lock lru_list_lock;
struct list_elem *lru_clock;

static struct list_elem *get_next_lru_clock (void);

void
lru_list_init (void)
{
  list_init (&lru_list);
  lock_init (&lru_list_lock);
  lru_clock = NULL;
}

void
add_page_to_lru_list (struct page *page)
{
  ASSERT (page != NULL);
  lock_acquire (&lru_list_lock);
  list_push_back (&lru_list, &page->lru);
  lock_release (&lru_list_lock);
}

void
del_page_from_lru_list (struct page *page)
{
  ASSERT (page != NULL);
  lock_acquire (&lru_list_lock);
  list_remove (&page->lru);
  lock_release (&lru_list_lock);
}

struct page *
alloc_page (enum palloc_flags flags)
{
  struct page *page = (struct page *)malloc (sizeof (struct page));

  if (page == NULL)
    return NULL;
  memset (page, 0, sizeof *page);
  if ((page->kaddr = palloc_get_page (flags)) == NULL)
    while ((page->kaddr = try_to_free_pages (flags)) == NULL);
  page->thread = thread_current ();

  add_page_to_lru_list (page);

  return page;
}

void
free_page (void *kaddr)
{
  struct list_elem *e, *next;

  if (kaddr != NULL)
    return;
  for (e = list_begin (&lru_list); e != list_end (&lru_list); e = next)
  {
    struct page *page = list_entry (e, struct page, lru);

    next = list_next (e);
    if (page->kaddr == kaddr)
      __free_page (page);
  }
}

void
__free_page (struct page *page)
{
  ASSERT (page != NULL)
  del_page_from_lru_list (page);
  if (page->vme != NULL)
    pagedir_clear_page (page->thread->pagedir, page->vme->vaddr);
  palloc_free_page (page->kaddr);
  free (page);
}

static struct list_elem *
get_next_lru_clock (void)
{
  if (lru_clock == list_end (&lru_list))
    return NULL;
  else if (lru_clock == NULL)
    return list_begin (&lru_list);
  else
    return list_next (lru_clock);
}

void *
try_to_free_pages (enum palloc_flags flags)
{
  struct page *victim;
  uint32_t *pagedir = thread_current ()->pagedir;

  while (1)
  {
    struct page *cur_page;

    if (lru_clock == NULL)
      lru_clock = get_next_lru_clock ();

    cur_page = list_entry (lru_clock, struct page, lru);

    if (cur_page->vme != NULL)
    {
      if (!pagedir_is_accessed (pagedir, cur_page->vme->vaddr))
      {
        victim = cur_page;
        lru_clock = get_next_lru_clock ();
        break;
      }
      else
        pagedir_set_accessed (pagedir, cur_page->vme->vaddr, false);
    }

    lru_clock = get_next_lru_clock ();
  }

  switch (victim->vme->type)
  {
    case VM_BIN:
      if (pagedir_is_dirty (pagedir, victim->vme->vaddr))
      {
        victim->vme->swap_slot = swap_out (victim->kaddr);
        victim->vme->type = VM_ANON;
      }
      break;
    case VM_FILE:
      if (pagedir_is_dirty (pagedir, victim->vme->vaddr))
      {
        file_write_at (victim->vme->file,
                       victim->vme->vaddr,
                       victim->vme->read_bytes,
                       victim->vme->offset);
      }
      break;
    case VM_ANON:
      victim->vme->swap_slot = swap_out (victim->kaddr);
      break;
    default:
      return NULL;
  }

  victim->vme->is_loaded = false;
  __free_page (victim);

  return palloc_get_page (flags);
}
/////////////////////////////////////////////////////////////////////////////
