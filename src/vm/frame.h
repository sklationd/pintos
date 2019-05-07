#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include <stdint.h>
#include <stdbool.h>
#include "threads/synch.h"

extern struct hash frame_table;
extern struct list frame_list;
extern struct lock frame_table_lock;
extern struct list_elem *eviction_ptr;
struct frame_table_entry
{
	struct hash_elem hash_elem;
	struct list_elem list_elem;
	uint32_t *user;
	uint32_t *kernel;
	struct thread *owner;
	struct sup_page_table_entry *spte;
};

void frame_init (void);
uint32_t* allocate_frame (void *addr);
void deallocate_frame(void *addr);
void free_frame(void *addr);
struct frame_table_entry* find_fte(void *addr); //user

#endif /* vm/frame.h */
