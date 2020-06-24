//////////////////////////////// PJ4 EDITED /////////////////////////////////
#include "filesys/buffer_cache.h"
#include <debug.h>
#include <string.h>
#include "filesys/filesys.h"

#define BUFFER_CACHE_ENTRY_NB 64
uint8_t p_buffer_cache[BUFFER_CACHE_ENTRY_NB][BLOCK_SECTOR_SIZE]; /* entire buffer cache (32KByte). */
struct buffer_head buffer_head[BUFFER_CACHE_ENTRY_NB]; /* Array of buffer head. */
int clock_hand = 0; /* Clock hand for clock algorithm. */

/* Initialize buffer cache. */
void
bc_init (void)
{
  int i;

  memset (buffer_head, 0, sizeof (struct buffer_head) * BUFFER_CACHE_ENTRY_NB);
  memset (p_buffer_cache, -1, BUFFER_CACHE_ENTRY_NB * BLOCK_SECTOR_SIZE);
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    lock_init (&buffer_head[i].buffer_lock);
    buffer_head[i].data = p_buffer_cache[i];
  }
}

/* Remove buffer cache. Just flush all entries. */
void
bc_term (void)
{
  bc_flush_all_entries ();
}

/* Select victim entry by clock algorithm. Return victim. */
struct buffer_head *
bc_select_victim (void)
{
  struct buffer_head *victim;

  while (true)
  {
    struct buffer_head *cp = &buffer_head[clock_hand % BUFFER_CACHE_ENTRY_NB];

    lock_acquire (&cp->buffer_lock);
    ASSERT (cp->in_use);
    if (cp->accessed)
      cp->accessed = false;
    else
    {
      victim = cp;
      clock_hand++;
      if (victim->dirty)
      {
        lock_release (&victim->buffer_lock);
        bc_flush_entry (victim);
        lock_acquire (&victim->buffer_lock);
      }
      lock_release (&victim->buffer_lock);
      break;
    }
    lock_release (&cp->buffer_lock);
    clock_hand++;
  }

  return victim;
}

/* Lookup buffer head array and compare sector number of cache
   to given sector. Return pointer if found, return NULL otherwise. */
struct buffer_head *
bc_lookup (block_sector_t sector)
{
  int i;

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
    if (buffer_head[i].in_use && buffer_head[i].sector == sector)
      return &buffer_head[i];

  return NULL;
}

/* Flush given buffer cache entry by calling block_write() */
void
bc_flush_entry (struct buffer_head *p_flush_entry)
{
  lock_acquire (&p_flush_entry->buffer_lock);
  ASSERT (p_flush_entry->in_use && p_flush_entry->dirty);
  block_write (fs_device, p_flush_entry->sector, p_flush_entry->data);
  p_flush_entry->dirty = false;
  lock_release (&p_flush_entry->buffer_lock);
}

/* Flush entire buffer cache by calling bc_flush_entry() repeatedly. */
void
bc_flush_all_entries (void)
{
  int i;

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
    if (buffer_head[i].in_use && buffer_head[i].dirty)
      bc_flush_entry (&buffer_head[i]);
}

/* Read data from buffer cache. If buffer cache of sector_idx
   doesn't exist, then select victim and write data from disk
   to victim.*/
void
bc_read (block_sector_t sector_idx, void *buffer,
         off_t bytes_read, int chunk_size, int sector_ofs)
{
  struct buffer_head *in_cache = bc_lookup (sector_idx);

  if (in_cache != NULL) /* Cache hit! */
    lock_acquire (&in_cache->buffer_lock);
  else /* Cache miss! */
  {
    int idx;

    for (idx = 0; idx < BUFFER_CACHE_ENTRY_NB; idx++)
      if (!buffer_head[idx].in_use)
        break;

    if (idx < BUFFER_CACHE_ENTRY_NB) /* Buffer cache is not full! */
      in_cache = &buffer_head[idx]; /* Select empty entry. */
    else /* Buffer cache is full! */
      in_cache = bc_select_victim (); /* Select victim. */

    lock_acquire (&in_cache->buffer_lock);
    block_read (fs_device, sector_idx, in_cache->data);
  }
  memcpy (buffer + bytes_read, in_cache->data + sector_ofs, chunk_size);
  in_cache->in_use = true;
  in_cache->accessed = true;
  in_cache->sector = sector_idx;
  lock_release (&in_cache->buffer_lock);
}

/* Write data to buffer cache. If buffer cache of sector_idx
   doesn't exist, then select victim, read from disk, and
   write data to it.*/
void
bc_write (block_sector_t sector_idx, const void *buffer,
          off_t bytes_written, int chunk_size, int sector_ofs)
{
  struct buffer_head *in_cache = bc_lookup (sector_idx);

  if (in_cache != NULL) /* Cache hit! */
    lock_acquire (&in_cache->buffer_lock);
  else /* Cache miss! */
  {
    int idx;

    for (idx = 0; idx < BUFFER_CACHE_ENTRY_NB; idx++)
      if (!buffer_head[idx].in_use)
        break;

    if (idx < BUFFER_CACHE_ENTRY_NB) /* Buffer cache is not full! */
      in_cache = &buffer_head[idx]; /* Select empty entry. */
    else /* Buffer cache is full! */
      in_cache = bc_select_victim (); /* Select victim. */

    lock_acquire (&in_cache->buffer_lock);
    block_read (fs_device, sector_idx, in_cache->data);
  }
  memcpy (in_cache->data + sector_ofs, buffer + bytes_written, chunk_size);
  in_cache->dirty = true;
  in_cache->in_use = true;
  in_cache->accessed = true;
  in_cache->sector = sector_idx;
  lock_release (&in_cache->buffer_lock);
}
/////////////////////////////////////////////////////////////////////////////
