//////////////////////////////// PJ3 EDITED /////////////////////////////////
#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"

struct bitmap *swap_bitmap;

void
swap_init (void)
{
  ASSERT ((swap_bitmap = bitmap_create (1024)) != NULL);
}

void
swap_in (size_t used_index, void *kaddr)
{
  struct block *b = block_get_role (BLOCK_SWAP);
  int i, block_per_page = PGSIZE / BLOCK_SECTOR_SIZE;

  ASSERT (bitmap_test (swap_bitmap, used_index) == true);
  for (i = 0; i < block_per_page; i++)
    block_read (b, used_index * block_per_page + i, kaddr + BLOCK_SECTOR_SIZE * i);

  bitmap_reset (swap_bitmap, used_index);
}

size_t
swap_out (void *kaddr)
{
  struct block *b = block_get_role (BLOCK_SWAP);
  int i, index, block_per_page = PGSIZE / BLOCK_SECTOR_SIZE;

  index = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
  for (i = 0; i < block_per_page; i++)
    block_write (b, index * block_per_page + i, kaddr + BLOCK_SECTOR_SIZE * i);

  return index;
}
/////////////////////////////////////////////////////////////////////////////
