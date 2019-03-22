#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("syscall number: %d\n",*(uint32_t *)(f->esp));
  //printf("esp: %x\n", f->esp);
  //hex_dump(f->esp, f->esp, 100, 1);
  //printf("syscall_num: %d\n",syscall_num(f));

  switch(syscall_num(f)){
  	case SYS_HALT  		:
		power_off();
		break;
    case SYS_EXIT  		:
    	exit(first_arg(f)); // status
    	break;
    case SYS_EXEC  		:
    	break;
    case SYS_WAIT  		:
    	break;
    case SYS_CREATE		:
    	break;
    case SYS_REMOVE		:
    	break;
    case SYS_OPEN  		:
    	break;
    case SYS_FILESIZE 	:
    	break;
    case SYS_READ  		:
    	break;      
    case SYS_WRITE 		:
    	//if(is_kernel_vaddr((void *)second_arg(f)))
    	//	exit(-1);
    	write((int)first_arg(f),(void *)second_arg(f),(unsigned)third_arg(f));
    	break;  
    case SYS_SEEK  		:
    	break;                   
    case SYS_TELL  		:
    	break;	               
    case SYS_CLOSE 		:
    	break;     
    default				:
    	printf("unknown system call! \n");   
  }
}

void exit(int status){
	printf("%s: exit(%d)\n",thread_name(),status);
    thread_exit();
}

int write(int fd, const void *buffer, unsigned size){
	if(fd == 1){ // console write
		putbuf(buffer,size);
		return size;
	}
	else
		return -1;
}