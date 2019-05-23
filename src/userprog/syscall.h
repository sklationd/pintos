#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "filesys/inode.h"
#include "filesys/directory.h"
//typedef int pid_t;
#define syscall_num(f) (*(uint32_t *)((f)->esp))
#define first_arg(f) (*(uint32_t *)((f)->esp + 4))
#define second_arg(f) (*(uint32_t *)((f)->esp + 8))
#define third_arg(f) (*(uint32_t *)((f)->esp + 12))
typedef int mapid_t;
#define MAP_FAILED ((mapid_t)-1);
void syscall_init (void);
extern struct lock filesys_lock;

struct file {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
};

struct mmap_header{
    struct list_elem list_elem;
    struct file *file;
    void *user; 
    //int fd;
    int filesize;
    mapid_t mapid;
};

struct inode_disk
  {
    uint32_t isdir;
    uint32_t direct_block[NUM_OF_DIRECT_BLOCK];
    uint32_t indirect_block;
    uint32_t db_indirect_block;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    disk_sector_t inode_sector;         /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/*
void exit(int status);
int exec(const char *cmd_line);
int create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char* file);
int filesize(int fd);
int write(int fd, const void *buffer, unsigned size);
*/
#endif /* userprog/syscall.h */
