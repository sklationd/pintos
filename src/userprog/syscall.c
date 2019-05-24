#include "userprog/syscall.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include <list.h>
#include <string.h>
#include "threads/init.h"
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);
struct lock filesys_lock;

void exit (int status);
int exec(const char *cmd_line);
int wait(int pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
mapid_t mmap(int fd, void *addr);
void munmap(mapid_t mapping);

bool chdir(const char *dir);
bool mkdir(const char *dir);
bool readdir(int fd, char *name);
bool isdir(int fd);
int inumber(int fd);

void exit(int status){
	printf("%s: exit(%d)\n",thread_name(),status);
    thread_current()->exit_status = status;
    thread_exit();
}

int exec(const char *cmd_line){ // return pid

    return process_execute(cmd_line);  
}

int wait(int pid){
    return process_wait(pid);
}

bool create(const char *file, unsigned initial_size){
    lock_acquire(&filesys_lock);
	struct dir *dir = dir_open_root ();
    struct inode *inode;
    bool b;

    /* if file is null, exit */
    if(file == NULL){
        lock_release(&filesys_lock);
        exit(-1);
    }

    /* if file already exists, fail */
    if (dir != NULL)
        if(dir_lookup (dir, file, &inode)){
            lock_release(&filesys_lock);
            return false;
        }

    b = filesys_create(file,initial_size);
    lock_release(&filesys_lock);
    return b;
}

bool remove(const char *file){
	/* if file is null, exit */
	if(file == NULL)
		exit(-1);
    lock_acquire(&filesys_lock);
    bool b = filesys_remove(file);
    lock_release(&filesys_lock);
    return b;
}

int open(const char *file){
	/* if file is null, exit */
	if(file==NULL)
		exit(-1);
    lock_acquire(&filesys_lock);

    
    /* is file exist? */
    struct dir *dir = dir_open_root ();
    struct inode *inode = NULL;
    if (dir != NULL)
        if(!dir_lookup (dir, file, &inode)){
            dir_close(dir);
            lock_release(&filesys_lock);
            return -1;
        }
    dir_close(dir);
    int i;
    for(i=0;i<128;i++){
        if(thread_current()->fd[i] == NULL){
            thread_current()->fd[i] = filesys_open(file);
            lock_release(&filesys_lock);
            return i+3;
        }
    }
    lock_release(&filesys_lock);
    return -1;
}

int filesize(int fd){
	if(fd > 130 || fd < 3)
		exit(-1);

	if(thread_current()->fd[fd-3]==NULL)
		return -1;

    lock_acquire(&filesys_lock);
    int len = file_length(thread_current()->fd[fd-3]);
    lock_release(&filesys_lock);

    return len;
}

int read(int fd, void *buffer, unsigned size){
    if(fd > 130)
        exit(-1);
    else if(thread_current()->fd[fd-3] == NULL)
        exit(-1);
    else if(fd == 0){
        unsigned i;
        for(i=0;i<size;i++){
            memset(buffer+i,input_getc(),1);
        }
        return size;
    }
    else if(fd == 1 || fd ==2)
        exit(-1);
    else{
        swap_prevention_buffer(buffer, size, true);
        lock_acquire(&filesys_lock);
	    int len = file_read(thread_current()->fd[fd-3],buffer,size);
        lock_release(&filesys_lock);
        swap_prevention_buffer(buffer, size, false);
        return len;
    }   
}

int write(int fd, const void *buffer, unsigned size){
    if(fd > 130)
        exit(-1);
    if(buffer == NULL)
        exit(-1);
    if(fd == 1){ // console write
        lock_acquire(&filesys_lock);    
        putbuf(buffer, size);
        lock_release(&filesys_lock);
        return size;
    }
    else if (fd == 0){ //std input
        return -1;
    }
    else if(thread_current()->fd[fd-3] == NULL){
        exit(-1);
    }

    else{
        if(!thread_current()->fd[fd-3]->deny_write){
            swap_prevention_buffer(buffer, size, true);
            lock_acquire(&filesys_lock);
            int len = file_write(thread_current()->fd[fd-3],buffer,size);
            lock_release(&filesys_lock);
            swap_prevention_buffer(buffer, size, false);
            return len;
        }
        lock_release(&filesys_lock);
        return 0;
    }
}

void seek(int fd, unsigned position){
	if(fd > 130)
		exit(-1);
	if(thread_current()->fd[fd-3] == NULL)
		exit(-1);
    lock_acquire(&filesys_lock);
    file_seek(thread_current()->fd[fd-3],position);
    lock_release(&filesys_lock);
}

unsigned tell(int fd){
	if(fd > 130)
		exit(-1);
	if(thread_current()->fd[fd-3] == NULL)
		exit(-1);
    lock_acquire(&filesys_lock);
    unsigned pos = file_tell(thread_current()->fd[fd-3]);
    lock_release(&filesys_lock);
    return pos;
}

void close(int fd){
	if(fd > 130 || fd < 3)
		exit(-1);
	if(thread_current()->fd[fd-3] == NULL)
		exit(-1);
    lock_acquire(&filesys_lock);
    file_close(thread_current()->fd[fd-3]);
    lock_release(&filesys_lock);
    thread_current()->fd[fd-3] = NULL;
}

mapid_t mmap(int fd, void *addr){
    if(fd==0 || fd==1)
        return MAP_FAILED;
    if(addr == NULL)
        return MAP_FAILED;
    if(pg_ofs(addr))
        return MAP_FAILED;
    lock_acquire(&filesys_lock);
    void *tmp;
    struct thread *t = thread_current();
    struct mmap_header *mh = (struct mmap_header *)malloc(sizeof(struct mmap_header)); //TODO: freeeeeeeeeeeeeee
    mh->file = file_reopen(t->fd[fd-3]);
    if(mh->file == NULL){
        goto FAIL;
    }
    size_t filesize = file_length(mh->file);

    if(filesize == 0)
        goto FAIL;

    for(tmp = addr; tmp < addr + filesize; tmp += PGSIZE){
        if(find_spte(tmp)){
            goto FAIL;
        }
    }

    for(tmp = addr; tmp < addr + filesize; tmp += PGSIZE){
        if(tmp == pg_round_down(addr+filesize))
            lazy_load(mh->file,tmp-addr, tmp, filesize+addr-tmp,
                      PGSIZE-(filesize+addr-tmp), true, allocate_page(tmp));
        else
            lazy_load(mh->file,tmp-addr,tmp, PGSIZE, 0, true, allocate_page(tmp));
    }
    
    //mh->fd = fd;
    mh->filesize = filesize;
    mh->user = addr;
    mh->mapid = (int)addr>>3;
    list_push_back(&t->mmap_list, &mh->list_elem);
    lock_release(&filesys_lock);
    return mh->mapid;

FAIL:
    file_close(mh->file);
    free(mh);
    lock_release(&filesys_lock);
    return MAP_FAILED;
}

void munmap(mapid_t mapping){
    struct thread *t = thread_current();
    struct list_elem *e;
    struct list *mmap_list = &(t->mmap_list);
    lock_acquire(&filesys_lock);
    //printf("lock acquire\n");
    for(e=list_begin(mmap_list);e!=list_end(mmap_list);e=list_next(e)){
        struct mmap_header *mh = list_entry(e, struct mmap_header, list_elem);
        if(mh->mapid == mapping){
            void *tmp;
            for(tmp = mh->user; tmp < mh->user + mh->filesize; tmp += PGSIZE){
                struct sup_page_table_entry *spte = find_spte(tmp);
                if(spte->state == SPTE_LOAD){
                    //printf("load\n");
                    deallocate_page(tmp);
                }
                else if(spte->state == SPTE_EVICTED){
                    //printf("evicted\n");
                    if(spte->dirty || pagedir_is_dirty(t->pagedir, spte->user_vaddr) || pagedir_is_dirty(t->pagedir, spte->kpage)) {
                        file_write_at(mh->file, tmp, spte->page_read_bytes,spte->ofs);
                        deallocate_fte(find_fte(tmp));
                        pagedir_clear_page(t->pagedir, tmp);
                    }
                    deallocate_page(tmp);
                }
                else if(spte->state == SPTE_MAPPED){
                    //printf("mapped\n");
                    swap_prevent_on(tmp);
                    if(spte->dirty || pagedir_is_dirty(t->pagedir, spte->user_vaddr) /* pagedir_is_dirty(t->pagedir, spte->kpage)*/){
                        file_write_at(mh->file, tmp, spte->page_read_bytes,spte->ofs);
                    }
                    deallocate_fte(find_fte(tmp));
                    deallocate_page(tmp);
                    pagedir_clear_page(t->pagedir,tmp);
                }
            }
            list_remove(&mh->list_elem);
            file_close(mh->file);
            free(mh);
            break;
        }
    }
    lock_release(&filesys_lock);
    //printf("lock release\n");
    return;
}

char *separate_filename(char *name){
    char *ptr = name;
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
        if(!lookup(dir_itr, token, &e, NULL)){
            if(dir_itr != dir_itr_)
                dir_close(dir_itr);
            return NULL;
        }
        if(dir_itr != dir_itr_)
            dir_close(dir_itr);
        dir_itr = dir_open(inode_open(e.inode_sector));
    }
    return dir_itr;
}

bool mkdir(const char *path_){
    char path[strlen(path_) + 1];
    struct dir *dir_itr;
    char *filename;
    if(path_[0] == 0)
        return false;
    strlcpy(path, path_, strlen(path_)+1);

    if(path[0] == '/'){
        dir_itr = dir_open_root();
    }
    else{
        dir_itr = thread_current()->curr_dir;
    }

    if((filename = separate_filename(path+1)) == NULL){
        filename = path+1;
    }
    else{
        dir_itr = path_to_dir(dir_itr, path+1);
    }
    if(dir_itr == NULL)
        return false;
    uint32_t inode_sector;
    if(!free_map_allocate(1, &inode_sector))
        return false;
    if(!dir_create(inode_sector, dir_itr->inode->sector, 16))
        return false;
    dir_add(dir_itr, filename, inode_sector);
    return true;
}


void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void
syscall_handler (struct intr_frame *f) 
{   
  /*  
  if(is_kernel_vaddr(f->esp))
    exit(-1);
  */
  thread_current()->esp = f->esp;
  switch(syscall_num(f)){
  	case SYS_HALT  		:
		power_off();
		break;
    case SYS_EXIT  		:
    	if(is_kernel_vaddr(f->esp + 4))
    		exit(-1);
    	exit(first_arg(f)); // status
    	break;
    case SYS_EXEC  		:
        if(is_kernel_vaddr(f->esp + 4) ||
           is_kernel_vaddr((void *)first_arg(f)))
            exit(-1);
    	f->eax = exec((const char *)first_arg(f));
    	break;
    case SYS_WAIT  		:
        if(is_kernel_vaddr(f->esp + 4))
            exit(-1);
        f->eax = wait((int)first_arg(f));
    	break;
    case SYS_CREATE		:
    	if(is_kernel_vaddr(f->esp + 4) || 
    	   is_kernel_vaddr(f->esp + 8) ||
    	   is_kernel_vaddr((void *)first_arg(f)))
    		exit(-1);
    	f->eax = create((const char *)first_arg(f),(unsigned)second_arg(f));
    	break;
    case SYS_REMOVE		:
    	if(is_kernel_vaddr(f->esp + 4) || 
    	   is_kernel_vaddr((void *)first_arg(f)))
    		exit(-1);
    	f->eax = remove((const char *)first_arg(f));
    	break;
    case SYS_OPEN  		:
    	if(is_kernel_vaddr(f->esp + 4)||
    	   is_kernel_vaddr((void *)first_arg(f)))
    		exit(-1);
    	f->eax = open((const char *)first_arg(f));
    	break;
    case SYS_FILESIZE 	:
        if(is_kernel_vaddr(f->esp + 4))
    		exit(-1);
    	f->eax = filesize((int)first_arg(f));
    	break;
    case SYS_READ  		:
    	if(is_kernel_vaddr(f->esp + 4) || 
    	   is_kernel_vaddr(f->esp + 8) ||
    	   is_kernel_vaddr(f->esp + 12)||
    	   is_kernel_vaddr((void *)second_arg(f)))
    		exit(-1);
    	f->eax = read((int)first_arg(f),(void *)second_arg(f),(unsigned)third_arg(f));
    	break;      
    case SYS_WRITE 		:
    	if(is_kernel_vaddr(f->esp + 4) || 
    	   is_kernel_vaddr(f->esp + 8) ||
    	   is_kernel_vaddr(f->esp + 12)||
    	   is_kernel_vaddr((void *)second_arg(f)))
    		exit(-1);
    	f->eax = write((int)first_arg(f),(void *)second_arg(f),(unsigned)third_arg(f));
    	break;  
    case SYS_SEEK  		:
    	if(is_kernel_vaddr(f->esp + 4) || 
    	   is_kernel_vaddr(f->esp + 8))
    		exit(-1);
    	seek((int)first_arg(f),(unsigned)second_arg(f));
    	break;                   
    case SYS_TELL  		:
   		if(is_kernel_vaddr(f->esp + 4))
    		exit(-1);
    	f->eax = tell((int)first_arg(f));
    	break;	               
    case SYS_CLOSE 		:
    	if(is_kernel_vaddr(f->esp + 4))
    		exit(-1);
    	close((int)first_arg(f));
    	break;     
    case SYS_MMAP           :
        if(is_kernel_vaddr(f->esp + 4) ||
           is_kernel_vaddr(f->esp + 8) ||
           is_kernel_vaddr((void *) second_arg(f)))
           exit(-1); 
        f->eax = mmap((int)first_arg(f), (void *)second_arg(f));
        break;
    case SYS_MUNMAP         :
        if(is_kernel_vaddr(f->esp + 4))
            exit(-1);
        munmap((mapid_t)first_arg(f));
        break;
    case SYS_CHDIR      :
        break;
    case SYS_MKDIR      :
        if(is_kernel_vaddr(f->esp + 4)||
           is_kernel_vaddr((void *)first_arg(f)))
            exit(-1);
        mkdir((char *)first_arg(f));
        break;
    case SYS_READDIR    :
        break;
    case SYS_ISDIR      :
        break;
    case SYS_INUMBER    :
        break;
    default				:
    	printf("unknown system call! \n");   
  }
}
