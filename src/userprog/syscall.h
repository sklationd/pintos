#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
//typedef int pid_t;
#define syscall_num(f) (*(uint32_t *)((f)->esp))
#define first_arg(f) (*(uint32_t *)((f)->esp + 4))
#define second_arg(f) (*(uint32_t *)((f)->esp + 8))
#define third_arg(f) (*(uint32_t *)((f)->esp + 12))
void syscall_init (void);
struct lock filesys_lock;
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
