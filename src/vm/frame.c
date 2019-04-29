#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

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
	hash_init(&frame_table,hash_int,hash_less, NULL);
}


/* 
 * Make a new frame table entry for addr.
 */
bool
allocate_frame (void *addr)
{
	struct frame_table_entry *fte = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));
	fte->hash_elem = (struct hash_elem*)malloc(sizeof(struct hash_elem));
	fte->frame = addr;
	fte->owner = running_thread();
	//spte
	fte->last_used_time = timer_ticks();
	lock_acquire(&frame_table_lock);
	hash_insert(&frame_table,fte->hash_elem);
	lock_release(&frame_table_lock);
}

