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

	bool swap_prevention;
};

void frame_init (void);
uint32_t* allocate_frame (void *addr);
void deallocate_frame(void *addr);
void deallocate_fte(struct frame_table_entry *fte);
struct frame_table_entry* find_fte(void *addr); //user
void deallocate_frame_owned_by_thread(void);
void swap_prevention_buffer(const void *buf, size_t size, bool onoff);

#endif /* vm/frame.h */
