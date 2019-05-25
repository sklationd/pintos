#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "devices/input.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "vm/frame.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

//typedef int pid_t;
#define syscall_num(f) (*(uint32_t *)((f)->esp))
#define first_arg(f) (*(uint32_t *)((f)->esp + 4))
#define second_arg(f) (*(uint32_t *)((f)->esp + 8))
#define third_arg(f) (*(uint32_t *)((f)->esp + 12))
#define MAP_FAILED ((mapid_t)-1);

typedef int mapid_t;
void syscall_init (void);

extern struct lock filesys_lock;

struct mmap_header{
    struct list_elem list_elem;
    struct file *file;
    void *user; 
    int filesize;
    mapid_t mapid;
};

#endif /* userprog/syscall.h */
