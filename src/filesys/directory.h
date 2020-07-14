#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdio.h>
#include <string.h>
#include <list.h>
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/off_t.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 64

struct inode;

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
    int size;
  };

/* A single directory entry. */
struct dir_entry 
  {
    disk_sector_t inode_sector;         /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Opening and closing directories. */
bool dir_create (disk_sector_t sector, disk_sector_t parent, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, disk_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);


char *separate_filename(char *name);
struct dir *path_to_dir(struct dir *dir_itr_, char *path);
struct dir *get_path_and_name(char *path, char *name);

int dir_size(struct dir *dir);

#endif /* filesys/directory.h */
