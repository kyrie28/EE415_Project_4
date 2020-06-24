//////////////////////////////// PJ4 EDITED /////////////////////////////////
#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include "threads/synch.h"
#include "devices/block.h"
#include "filesys/off_t.h"

struct buffer_head
{
  bool dirty;               /* Buffer is dirty or not. */
  bool in_use;              /* Buffer is in use or not. */
  bool accessed;            /* Buffer has been accessed or not. */
  block_sector_t sector;    /* Sector number. */
  struct lock buffer_lock;  /* Lock for buffer access. */
  void *data;               /* Pointer to actual buffer cache */
};

void bc_init (void);
void bc_term (void);
struct buffer_head *bc_select_victim (void);
struct buffer_head *bc_lookup (block_sector_t sector);
void bc_flush_entry (struct buffer_head *p_flush_entry);
void bc_flush_all_entries (void);
void bc_read (block_sector_t sector_idx, void *buffer,
              off_t bytes_read, int chunk_size, int sector_ofs);
void bc_write (block_sector_t sector_idx, const void *buffer,
               off_t bytes_written, int chunk_size, int sector_ofs);

#endif /* filesys/buffer_cache.h */
/////////////////////////////////////////////////////////////////////////////
