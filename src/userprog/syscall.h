#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#define syscall_num(f) (*(uint32_t *)((f)->esp))
#define first_arg(f) (*(uint32_t *)((f)->esp + 4))
#define second_arg(f) (*(uint32_t *)((f)->esp + 8))
#define third_arg(f) (*(uint32_t *)((f)->esp + 12))
void syscall_init (void);

#endif /* userprog/syscall.h */
