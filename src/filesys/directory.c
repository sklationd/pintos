#include "filesys/directory.h"


/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, disk_sector_t parent, size_t entry_cnt)
{
  struct inode *inode;
  if(sector == ROOT_DIR_SECTOR)
    if(!inode_create (sector, entry_cnt * sizeof (struct dir_entry), 1))
      return false;
  struct dir_entry write_entry[2];
  strlcpy(write_entry[0].name, ".", 2);
  strlcpy(write_entry[1].name, "..", 3);
  write_entry[0].in_use = true;
  write_entry[1].in_use = true;

  if(sector == ROOT_DIR_SECTOR){
    write_entry[0].inode_sector = sector;
    write_entry[1].inode_sector = sector;
  }    
  else{
    write_entry[0].inode_sector = sector;
    write_entry[1].inode_sector = parent;
  }
  inode = inode_open(sector);
  inode_write_at(inode, write_entry, 2*sizeof(struct dir_entry), 0);
  inode_close(inode);
  return true;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      size_t ofs;
      struct dir_entry e;
      int used = 0;
      for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e){
        if(e.in_use)
          used++;
      }
      dir->size = used-2;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) 
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  if(success)
    dir->size++;
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  if(inode_isdir(inode)){
    struct dir *rm_dir = dir_open(inode);
    if(rm_dir->size > 0){
      dir_close(rm_dir);
      goto done;
    }
    dir_close(rm_dir);
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  if(success)
    dir->size--;
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

char *separate_filename(char *name){
    char *ptr = name;
    if(*ptr == 0)
      return NULL;
    while(*ptr)
        ptr++;
    ptr--;
    while(*ptr != '/'){
        ptr--;
        if(ptr < name)
            return NULL;
    }
    *ptr = 0;
    return ptr + 1;
}

struct dir *path_to_dir(struct dir *dir_itr_, char *path){
    char *token, *save_ptr;
    struct dir_entry e;
    struct dir *dir_itr = dir_itr_;

    for (token = strtok_r (path, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr)){
      if(dir_itr->inode->removed){
        dir_close(dir_itr);
        return NULL;
      }
      if(!lookup(dir_itr, token, &e, NULL)){
          dir_close(dir_itr);
          return NULL;
      }
      dir_close(dir_itr);
      dir_itr = dir_open(inode_open(e.inode_sector));
    }
    return dir_itr;
}

struct dir *get_path_and_name(char *path_, char *name){
  char path[strlen(path_) + 1];
  char *abs_path = path;  
  char *filename;
  struct dir* dir_itr;

  strlcpy(path, path_, strlen(path_)+1);

  if(path[0] == '/'){
      dir_itr = dir_open_root();
      while(*abs_path == '/')
        abs_path++;
  }
  else{
    if(thread_current()->curr_dir == NULL)
      dir_itr = dir_open_root();
    else
      dir_itr = dir_reopen(thread_current()->curr_dir);
  }

  if(name != NULL){
    if((filename = separate_filename(abs_path)) == NULL){
      filename = abs_path;
    }
    else{
        dir_itr = path_to_dir(dir_itr, abs_path);
    }
    if(strlen(filename) > NAME_MAX){
      dir_close(dir_itr);
      return NULL;
    }
    strlcpy(name, filename, NAME_MAX+1);
  }
  else{
    dir_itr = path_to_dir(dir_itr, abs_path);
  }
  return dir_itr;
}

int dir_size(struct dir *dir){
  return dir->size;
}