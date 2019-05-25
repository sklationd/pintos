#include "filesys/inode.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

uint32_t find_sector(struct inode_disk *id, int index){ // index starts with 0
  if(bytes_to_sectors(id->length) < index)
    return -1;
  ASSERT(index < NUM_OF_DIRECT_BLOCK + NUM_OF_INDIRECT_BLOCK + sq(NUM_OF_INDIRECT_BLOCK));
  int remain = index;
  uint32_t sector_buf[NUM_OF_INDIRECT_BLOCK];
  if(index < NUM_OF_DIRECT_BLOCK){ // DIRECT BLOCK
    return id->direct_block[index];
  }
  else if(index < NUM_OF_DIRECT_BLOCK + NUM_OF_INDIRECT_BLOCK){
    remain -= NUM_OF_DIRECT_BLOCK;
    disk_read(filesys_disk, id->indirect_block, sector_buf);
    return sector_buf[remain];
  }
  else if(index < NUM_OF_DIRECT_BLOCK + NUM_OF_INDIRECT_BLOCK + sq(NUM_OF_INDIRECT_BLOCK)){
    memset(sector_buf,0,NUM_OF_INDIRECT_BLOCK);
    remain -= NUM_OF_DIRECT_BLOCK + NUM_OF_INDIRECT_BLOCK;
    disk_read(filesys_disk, id->db_indirect_block, sector_buf);
    int db_index = remain / NUM_OF_INDIRECT_BLOCK;
    int ofs = remain % NUM_OF_INDIRECT_BLOCK;
    uint32_t sector_number = sector_buf[db_index];
    disk_read(filesys_disk, sector_number, sector_buf);
    return sector_buf[ofs]; 
  }
  NOT_REACHED();
}

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */

static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if(pos < 0)
    return -1;
  if (pos < inode->data.length){
    return find_sector(&inode->data, pos / DISK_SECTOR_SIZE);
    //return inode->data.start + pos / DISK_SECTOR_SIZE;
  }
  else{
    return -1;
  }
}


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

bool
create_inode_disk(struct inode_disk *disk_inode, size_t sectors, size_t original_sectors)
{
    //size_t original_sectors = bytes_to_sectors(disk_inode->length);
    uint32_t buf[NUM_OF_INDIRECT_BLOCK] = {0,};
    uint32_t db_buf[NUM_OF_INDIRECT_BLOCK] = {0,};
    int i, j;
    int remain_sectors = sectors;
    int current_sector = 0;
    //printf("Sectors %d\n", sectors);
    if(sectors > 0){
      for(i = 0; i < NUM_OF_DIRECT_BLOCK; i++){
        if(current_sector++ >= original_sectors){
          if(!free_map_allocate(1, disk_inode->direct_block + i))
            goto FAIL;
        }
        if(--remain_sectors == 0)
          break;
      }
    }
    if(sectors > NUM_OF_DIRECT_BLOCK){
      if(original_sectors < NUM_OF_DIRECT_BLOCK)
        if(!free_map_allocate(1, &disk_inode->indirect_block))
          goto FAIL;
      disk_read(filesys_disk, disk_inode->indirect_block, buf);
      for(i = 0; i<NUM_OF_INDIRECT_BLOCK; i++){
        if(current_sector++ >= original_sectors){
          if(!free_map_allocate(1, buf+i))
            goto FAIL;
          if(--remain_sectors == 0)
            break;
        }
      }
      disk_write(filesys_disk, disk_inode->indirect_block, buf);
    }
    if(remain_sectors > 0){
      if(original_sectors < NUM_OF_DIRECT_BLOCK + NUM_OF_INDIRECT_BLOCK)
        if(!free_map_allocate(1, &disk_inode->db_indirect_block))
          goto FAIL;
      int db_index = 0;
      disk_read(filesys_disk, disk_inode->db_indirect_block, db_buf);
      while(remain_sectors > 0){
        if(original_sectors < NUM_OF_DIRECT_BLOCK + NUM_OF_INDIRECT_BLOCK
                              + db_index * NUM_OF_INDIRECT_BLOCK)
          if(!free_map_allocate(1,db_buf+db_index))
            goto FAIL;
        disk_read(filesys_disk, db_buf[db_index], buf);
        for(i = 0; i < NUM_OF_INDIRECT_BLOCK; i++){
          if(current_sector++ >= original_sectors)
            if(!free_map_allocate(1, buf+i))
              goto FAIL;
          if(--remain_sectors == 0)
            break;
        }
        disk_write(filesys_disk, db_buf[db_index], buf);
        db_index ++;
      }
      disk_write(filesys_disk, disk_inode->db_indirect_block ,db_buf);
    }
    return true;
FAIL:
  return false;
}

bool
release_inode_disk(struct inode_disk *disk_inode, size_t sectors){
    int i;
    int remain_sectors = sectors;
    uint32_t buf[NUM_OF_INDIRECT_BLOCK] = {0,};
    uint32_t db_buf[NUM_OF_INDIRECT_BLOCK] = {0,};

    for(i = 0; i < NUM_OF_DIRECT_BLOCK; i++){
      free_map_release(disk_inode->direct_block[i],1);
      if(--remain_sectors == 0)
        break;
    }
    if(sectors > NUM_OF_DIRECT_BLOCK){
      free_map_release(disk_inode->indirect_block, 1);
      for(i = 0; i<NUM_OF_INDIRECT_BLOCK; i++){
        disk_read(filesys_disk, disk_inode->indirect_block, buf);
        free_map_release(buf[i], 1);
        if(--remain_sectors == 0)
          break;
      }
    }
    if(remain_sectors > 0){
      int db_index = 0;
      disk_read(filesys_disk, disk_inode->db_indirect_block, db_buf);
      free_map_release(disk_inode->db_indirect_block, 1);
      while(remain_sectors > 0){
        free_map_release(db_buf[db_index],1);
        memset(buf,0,NUM_OF_INDIRECT_BLOCK);
        disk_read(filesys_disk, db_buf[db_index], buf);
        for(i=0; i<NUM_OF_INDIRECT_BLOCK; i++){
          free_map_release(buf[i],1);
          if(--remain_sectors == 0)
            break;
        }
        db_index ++;
      }
    }
    return true;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, uint32_t _isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode != NULL){
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode->isdir = _isdir;
    if(create_inode_disk(disk_inode, sectors, 0) == false)
      return success;

    disk_write (filesys_disk, sector, disk_inode);
    if(sectors > 0) {
      static char zeros[DISK_SECTOR_SIZE];
      size_t i;
      for(i=0; i < sectors; i++){
        disk_write(filesys_disk, find_sector(disk_inode, i), zeros);
      }
    }
    success = true;
    free (disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
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
  //printf("push %p\n", inode);
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  disk_read (filesys_disk, inode->sector, &inode->data);
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
disk_sector_t
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

      disk_write (filesys_disk, inode->sector, &inode->data);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          release_inode_disk(&inode->data, bytes_to_sectors(inode->data.length));
          //free_map_release (inode->data.start,
          //                  bytes_to_sectors (inode->data.length)); 
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

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset); 
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          disk_read (filesys_disk, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
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
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;
  
  if(byte_to_sector(inode, offset+size-1) == -1){ //need extension
    size_t original_bytes = inode->data.length;
    inode->data.length = offset+size;
    if(!create_inode_disk(&inode->data, bytes_to_sectors(offset+size), bytes_to_sectors(original_bytes)))
      return 0;    
  }


  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset); 
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

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
  return inode->data.length;
}

bool inode_isdir(struct inode *inode){
  return inode->data.isdir;
}