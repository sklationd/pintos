#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <stdint.h>
#include <stdbool.h>

extern struct hash frame_table;

struct frame_table_entry
{
	struct hash_elem hash_elem;
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_table_entry* spte;
};

void frame_init (void);
bool allocate_frame (void *addr);
bool deallocate_frame(void *addr);
#endif /* vm/frame.h */
