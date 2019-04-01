#include "userprog/syscall.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
static void syscall_handler (struct intr_frame *);


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
    if(file == NULL)
        exit(-1);

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
            lock_release(&filesys_lock);
            return -1;
        }
    int i;
    for(i=0;i<128;i++){
        if(thread_current()->fd[i] == NULL){
            thread_current()->fd[i] = filesys_open(file);
            //thread_current()->cl[i] = 0;
            lock_release(&filesys_lock);
            return i+3;
        }
    }
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
	if(thread_current()->fd[fd-3] == NULL)
		exit(-1);
	if(fd == 0)
		;
	else if(fd == 1 || fd ==2)
		exit(-1);
    lock_acquire(&filesys_lock);
	int len = file_read(thread_current()->fd[fd-3],buffer,size);
    lock_release(&filesys_lock);
    return len;
}

int write(int fd, const void *buffer, unsigned size){
	if(fd > 130)
		exit(-1);
	if(buffer == NULL)
		exit(-1);

	if(fd == 1){ // console write
		putbuf(buffer,size);
		return size;
	}
	else if (fd == 0) //std input
		return -1;

	else if(thread_current()->fd[fd-3] == NULL)
		exit(-1);

	else{
        lock_acquire(&filesys_lock);
        int len = file_write(thread_current()->fd[fd-3],buffer,size);
        lock_release(&filesys_lock);
        return len;
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
    thread_current()->fd[fd-3] = NULL;
    lock_release(&filesys_lock);
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
  if(is_kernel_vaddr(f->esp))
    exit(-1);
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
    	tell((int)first_arg(f));
    	break;	               
    case SYS_CLOSE 		:
    	if(is_kernel_vaddr(f->esp + 4))
    		exit(-1);
    	close((int)first_arg(f));
    	break;     
    default				:
    	printf("unknown system call! \n");   
  }
}
