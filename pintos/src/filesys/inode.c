#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
//////////////////////////////// PJ4 EDITED /////////////////////////////////
#include "filesys/buffer_cache.h"
/////////////////////////////////////////////////////////////////////////////

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

//////////////////////////////// PJ4 EDITED /////////////////////////////////
#define INDIRECT_BLOCK_ENTRIES 128  /* A block can contain 128 sector numbers */
#define DIRECT_BLOCK_ENTRIES 124    /* Make inode_disk fit in BLOCK_SECTOR_SIZE */

enum direct_t
  {
    NORMAL_DIRECT,                  /* Saved in inode directly. */
    INDIRECT,                       /* Saved indirectly. */
    DOUBLE_INDIRECT,                /* Saved double-indirectly. */
    OUT_LIMIT                       /* Wrong file offset. */
  };

struct sector_location
  {
    enum direct_t directness;       /* Direct, indirect, or double indirect. */
    off_t index1;                   /* Offset for lower index table. */
    off_t index2;                   /* Offset for higher index table. */
  };

struct inode_indirect_block
  {
    /* Index block structure. */
    block_sector_t map_table[INDIRECT_BLOCK_ENTRIES];
  };
/////////////////////////////////////////////////////////////////////////////

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
//////////////////////////////// PJ4 EDITED /////////////////////////////////
    // block_sector_t start;            /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    // uint32_t unused[125];            /* Not used. */
    block_sector_t direct_map_table[DIRECT_BLOCK_ENTRIES]; /* Direct blocks. */
    block_sector_t indirect_block_sec;                   /* Indirect blocks. */
    block_sector_t double_indirect_block_sec;     /* Double indirect blocks. */
/////////////////////////////////////////////////////////////////////////////
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
//////////////////////////////// PJ4 EDITED /////////////////////////////////
    // struct inode_disk data;          /* Inode content. */
    struct lock extend_lock;            /* Lock for file extension. */
/////////////////////////////////////////////////////////////////////////////
  };

//////////////////////////////// PJ4 EDITED /////////////////////////////////
/* Read on-disk inode from buffer cache and save it to inode_disk. */
static void
get_disk_inode (const struct inode *inode, struct inode_disk *inode_disk)
{
  bc_read (inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
}

/* Check the access type of disk block and calculate index of blocks.
   Results are saved in sec_loc. */
static void
locate_byte (off_t pos, struct sector_location *sec_loc)
{
  off_t pos_sector = pos / BLOCK_SECTOR_SIZE;

  if (pos_sector < DIRECT_BLOCK_ENTRIES)
  {
    /* Direct */
    sec_loc->directness = NORMAL_DIRECT;
    sec_loc->index1 = pos_sector;
  }
  else if (pos_sector < (off_t)(DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES))
  {
    /* Indirect */
    sec_loc->directness = INDIRECT;
    sec_loc->index1 = pos_sector - DIRECT_BLOCK_ENTRIES;
  }
  else if (pos_sector < (off_t)(DIRECT_BLOCK_ENTRIES + \
    INDIRECT_BLOCK_ENTRIES * (INDIRECT_BLOCK_ENTRIES + 1)))
  {
    /* Double Indirect */
    sec_loc->directness = DOUBLE_INDIRECT;
    pos_sector -= DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES;
    sec_loc->index1 = pos_sector % INDIRECT_BLOCK_ENTRIES;
    sec_loc->index2 = pos_sector / INDIRECT_BLOCK_ENTRIES;
  }
  else
    sec_loc->directness = OUT_LIMIT;
}

/* Return byte offset of map_table */
static inline off_t
map_table_offset (int index)
{
  return index * sizeof (block_sector_t);
}

/* Save sector number of new_sector to inode_disk. */
static bool
register_sector (struct inode_disk *inode_disk, block_sector_t new_sector,
                 struct sector_location sec_loc)
{
  struct inode_indirect_block temp_block;
  block_sector_t lower_table_sector;
  uint8_t zeros[BLOCK_SECTOR_SIZE] = { 0, };

  switch (sec_loc.directness)
  {
    case NORMAL_DIRECT:
      inode_disk->direct_map_table[sec_loc.index1] = new_sector;
      break;
    case INDIRECT:
      if (inode_disk->indirect_block_sec <= 0)
      {
        if (!free_map_allocate (1, &inode_disk->indirect_block_sec))
          return false;
        bc_write (inode_disk->indirect_block_sec, zeros,
                  0, BLOCK_SECTOR_SIZE, 0);
      }
      bc_write (inode_disk->indirect_block_sec, &new_sector,
                0, sizeof (block_sector_t), map_table_offset (sec_loc.index1));
      break;
    case DOUBLE_INDIRECT:
      if (inode_disk->double_indirect_block_sec <= 0)
      {
        if (!free_map_allocate (1, &inode_disk->double_indirect_block_sec))
          return false;
        memset (&temp_block, 0, sizeof temp_block);
      }
      else
        bc_read (inode_disk->double_indirect_block_sec, &temp_block,
                 0, BLOCK_SECTOR_SIZE, 0);

      if (temp_block.map_table[sec_loc.index2] <= 0)
      {
        if (!free_map_allocate (1, &temp_block.map_table[sec_loc.index2]))
          return false;
        bc_write (temp_block.map_table[sec_loc.index2], zeros,
                  0, BLOCK_SECTOR_SIZE, 0);
        bc_write (inode_disk->double_indirect_block_sec, &temp_block.map_table,
                  0, BLOCK_SECTOR_SIZE, 0);
      }

      bc_write (temp_block.map_table[sec_loc.index2], &new_sector,
                0, sizeof (block_sector_t), map_table_offset (sec_loc.index1));
      break;
    default:
      return false;
  }
  return true;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
/* old codes */
// static block_sector_t
// byte_to_sector (const struct inode *inode, off_t pos)
// {
  // ASSERT (inode != NULL);
  // if (pos < inode->data.length)
    // return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  // else
    // return -1;
// }
static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos)
{
  block_sector_t result_sec;

  if (pos < inode_disk->length)
  {
    block_sector_t lower_table_sector;
    struct sector_location sec_loc;
    locate_byte(pos, &sec_loc);

    switch (sec_loc.directness)
    {
      case NORMAL_DIRECT:
        result_sec = inode_disk->direct_map_table[sec_loc.index1];
        break;
      case INDIRECT:
        bc_read (inode_disk->indirect_block_sec, &result_sec,
                 0, sizeof (block_sector_t), map_table_offset (sec_loc.index1));
        break;
      case DOUBLE_INDIRECT:
        bc_read (inode_disk->double_indirect_block_sec, &lower_table_sector,
                 0, sizeof (block_sector_t), map_table_offset (sec_loc.index2));
        bc_read (lower_table_sector, &result_sec,
                 0, sizeof (block_sector_t), map_table_offset (sec_loc.index1));
        break;
      default:
        result_sec = -1;
    }
  }
  else
    result_sec = -1;
    
  return result_sec;
}


/* Deallocate all disk block for inode_disk. */
static void
free_inode_sectors (struct inode_disk *inode_disk)
{
  int i, j;
  struct inode_indirect_block ind_block_1;
  struct inode_indirect_block ind_block_2;

  if (inode_disk->double_indirect_block_sec > 0)
  {
    bc_read (inode_disk->double_indirect_block_sec, &ind_block_1,
             0, BLOCK_SECTOR_SIZE, 0);
    for (i = 0; i < INDIRECT_BLOCK_ENTRIES && ind_block_1.map_table[i] > 0; i++)
    {
      bc_read (ind_block_1.map_table[i], &ind_block_2,
               0, BLOCK_SECTOR_SIZE, 0);
      for (j = 0; i < INDIRECT_BLOCK_ENTRIES && ind_block_2.map_table[j] > 0; j++)
        free_map_release (ind_block_2.map_table[j], 1);
      free_map_release (ind_block_1.map_table[i], 1);
    }
    free_map_release (inode_disk->double_indirect_block_sec, 1);
  }

  if (inode_disk->indirect_block_sec > 0){
    bc_read (inode_disk->indirect_block_sec, &ind_block_1,
             0, BLOCK_SECTOR_SIZE, 0);
    for (i = 0; i < INDIRECT_BLOCK_ENTRIES && ind_block_1.map_table[i] > 0; i++)
      free_map_release (ind_block_1.map_table[i], 1);
    free_map_release (inode_disk->indirect_block_sec, 1);
  }

  for (i = 0; i < DIRECT_BLOCK_ENTRIES && inode_disk->direct_map_table[i] > 0; i++)
    free_map_release (inode_disk->direct_map_table[i], 1);
}

/* If start_pos < end_pos, then allocate
   new disk block and update inode info. */
bool
inode_update_file_length (struct inode_disk *inode_disk,
                          off_t start_pos, off_t end_pos)
{
  off_t size = end_pos - start_pos;
  uint8_t zeroes[BLOCK_SECTOR_SIZE] = { 0, };

  while (size > 0)
  {
    int sector_ofs = start_pos % BLOCK_SECTOR_SIZE;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int chunk_size = size < sector_left ? size : sector_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0)
    {
      block_sector_t sector_idx;
      struct sector_location sec_loc;
      locate_byte (start_pos, &sec_loc);

      if (!free_map_allocate (1, &sector_idx))
        return false;
      if (!register_sector (inode_disk, sector_idx, sec_loc))
        return false;
      bc_write (sector_idx, zeroes, 0, BLOCK_SECTOR_SIZE, 0);
    }

    /* Advance. */
    size -= chunk_size;
    start_pos += chunk_size;
  }

  inode_disk->length = end_pos;
  return true;
}
/////////////////////////////////////////////////////////////////////////////

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
//////////////////////////////// PJ4 EDITED /////////////////////////////////
      /* old codes */
      // size_t sectors = bytes_to_sectors (length);
      // disk_inode->length = length;
      // disk_inode->magic = INODE_MAGIC;
      // if (free_map_allocate (sectors, &disk_inode->start))
        // {
          // block_write (fs_device, sector, disk_inode);
          // if (sectors > 0)
            // {
              // static char zeros[BLOCK_SECTOR_SIZE];
              // size_t i;

              // for (i = 0; i < sectors; i++)
                // block_write (fs_device, disk_inode->start + i, zeros);
            // }
          // success = true;
        // }
      // free (disk_inode);

      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      if (length > 0)
        inode_update_file_length (disk_inode, 0, length);
      bc_write (sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
      free (disk_inode);
      success = true;
/////////////////////////////////////////////////////////////////////////////
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
//////////////////////////////// PJ4 EDITED /////////////////////////////////
  /* old code */
  // block_read (fs_device, inode->sector, &inode->data);
  lock_init (&inode->extend_lock);
/////////////////////////////////////////////////////////////////////////////
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
//////////////////////////////// PJ4 EDITED /////////////////////////////////
          /* old codes */
          // free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
                            // bytes_to_sectors (inode->data.length));

          struct inode_disk disk_inode;
          get_disk_inode (inode, &disk_inode);
          free_inode_sectors (&disk_inode);
          free_map_release (inode->sector, 1);
/////////////////////////////////////////////////////////////////////////////
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
//////////////////////////////// PJ4 EDITED /////////////////////////////////
  struct inode_disk disk_inode;
  get_disk_inode (inode, &disk_inode);

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&disk_inode, offset);
/////////////////////////////////////////////////////////////////////////////
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

//////////////////////////////// PJ4 EDITED /////////////////////////////////
      /* old codes */
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        // {
          /* Read full sector directly into caller's buffer. */
          // block_read (fs_device, sector_idx, buffer + bytes_read);
        // }
      // else
        // {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          // if (bounce == NULL)
            // {
              // bounce = malloc (BLOCK_SECTOR_SIZE);
              // if (bounce == NULL)
                // break;
            // }
          // block_read (fs_device, sector_idx, bounce);
          // memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        // }
      bc_read (sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
/////////////////////////////////////////////////////////////////////////////


      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
//////////////////////////////// PJ4 EDITED /////////////////////////////////
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  // uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  struct inode_disk disk_inode;
  get_disk_inode (inode, &disk_inode);

  lock_acquire (&inode->extend_lock);
  int old_length = disk_inode.length;
  int write_end = offset + size - 1;
  if (write_end > old_length - 1)
    inode_update_file_length (&disk_inode, old_length, write_end);
  lock_release (&inode->extend_lock);

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&disk_inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* old codes */
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        // {
          /* Write full sector directly to disk. */
          // block_write (fs_device, sector_idx, buffer + bytes_written);
        // }
      // else
        // {
          /* We need a bounce buffer. */
          // if (bounce == NULL)
            // {
              // bounce = malloc (BLOCK_SECTOR_SIZE);
              // if (bounce == NULL)
                // break;
            // }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          // if (sector_ofs > 0 || chunk_size < sector_left)
            // block_read (fs_device, sector_idx, bounce);
          // else
            // memset (bounce, 0, BLOCK_SECTOR_SIZE);
          // memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // block_write (fs_device, sector_idx, bounce);
        // }
      bc_write (sector_idx, buffer, bytes_written, chunk_size, sector_ofs);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  // free (bounce);
  bc_write (inode->sector, &disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
/////////////////////////////////////////////////////////////////////////////
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
//////////////////////////////// PJ4 EDITED /////////////////////////////////
  // return inode->data.length;
  struct inode_disk disk_inode;
  get_disk_inode (inode, &disk_inode);
  return disk_inode.length;
/////////////////////////////////////////////////////////////////////////////
}
