#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#include "vm/page.h"

/*
 * Initialize frame table
 */

struct hash frame_table;
struct lock frame_table_lock;

unsigned hash_function(const struct hash_elem *e, void *aux){
	uint32_t* frame = hash_entry(e,struct frame_table_entry,hash_elem)->frame;
	return hash_int((int)frame);
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	return hash_entry(a, struct frame_table_entry, hash_elem)->frame < 
		   hash_entry(b, struct frame_table_entry, hash_elem)->frame;
}

void 
frame_init (void)
{
	lock_init(&frame_table_lock);
	hash_init(&frame_table,hash_function,hash_less, NULL);
}


/* 
 * Make a new frame table entry for addr.
 */
bool
allocate_frame (void *addr)
{	
	struct frame_table_entry *fte = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));
	if(fte == NULL)
		return false;
	fte->frame = addr;
	fte->owner = thread_current();
	//fte->sup_page_table_entry = allocate_page();
	//fte->sup_page_table_entry->access_time = timer_ticks();
	lock_acquire(&frame_table_lock);
	hash_insert(&frame_table,&fte->hash_elem);
	lock_release(&frame_table_lock);

	return true;
}

bool
deallocate_frame(void *addr){
	struct frame_table_entry fte;
	struct hash_elem *e;
	fte.frame = addr;
	lock_acquire(&frame_table_lock);
	e= hash_find(&frame_table, &fte.hash_elem);
	hash_delete(&frame_table,e);
	lock_release(&frame_table_lock);
	free(hash_entry(e, struct frame_table_entry, hash_elem));
}



