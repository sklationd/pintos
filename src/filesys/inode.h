#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "devices/disk.h"
#include "threads/malloc.h"
#include "filesys/off_t.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"

#define NUM_OF_DIRECT_BLOCK 123
#define NUM_OF_INDIRECT_BLOCK 128
#define sq(x) ((x)*(x))
#define min(x,y) ((x)<(y) ? (x) : (y))
#define max(x,y) ((x)>(y) ? (x) : (y))

struct bitmap;

struct indirect_block_sector
  {
    uint32_t indirect_block_sector[NUM_OF_INDIRECT_BLOCK];
  };

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    uint32_t isdir;
    uint32_t direct_block[NUM_OF_DIRECT_BLOCK];
    uint32_t indirect_block;
    uint32_t db_indirect_block;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

void inode_init (void);
bool inode_create (disk_sector_t, off_t, uint32_t);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

bool inode_isdir(struct inode *);

#endif /* filesys/inode.h */
