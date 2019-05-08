#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#include "vm/page.h"
#include "vm/swap.h"

struct hash frame_table;
struct list frame_list;
struct list_elem *eviction_ptr;
struct lock frame_table_lock;
/*
 * Initialize frame table
 */

unsigned frame_hash_function(const struct hash_elem *e, void *aux){
	uint32_t* user = hash_entry(e, struct frame_table_entry, hash_elem)->user;
	return hash_int((int)user);
}

bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	struct thread *thread_a = hash_entry(a, struct frame_table_entry, hash_elem)->owner;
	struct thread *thread_b = hash_entry(b, struct frame_table_entry, hash_elem)->owner;
	if(thread_a != thread_b)
		return thread_a < thread_b;
	return hash_entry(a, struct frame_table_entry, hash_elem)->user < 
		   hash_entry(b, struct frame_table_entry, hash_elem)->user;
}

void 
frame_init (void)
{
	eviction_ptr = NULL;
	list_init(&frame_list);
	hash_init(&frame_table, frame_hash_function, frame_hash_less, NULL);
	lock_init(&frame_table_lock);
}

/* 
 * Make a new frame table entry for addr.
 */
uint32_t *
_allocate_frame (void *addr) // user virtual address
{	
	struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
	if(fte == NULL)
		return NULL;
	fte->kernel = palloc_get_page(PAL_USER | PAL_ZERO);
	if(fte->kernel == NULL)
		return NULL;
	fte->user = addr;
	fte->owner = thread_current();
	lock_acquire(&frame_table_lock);
	list_push_back(&frame_list, &fte->list_elem);
	hash_insert(&frame_table, &fte->hash_elem);
	lock_release(&frame_table_lock);

	struct sup_page_table_entry spte;
	spte.user_vaddr = addr;
	struct hash_elem *e;
	if((e = hash_find(thread_current()->sup_page_dir, &spte.hash_elem)) == NULL){
		fte->spte = allocate_page(addr);
	}
	else{
		fte->spte = hash_entry(e, struct sup_page_table_entry, hash_elem);
	}

	return fte->kernel;
}
uint32_t *
allocate_frame (void *_addr){
	void *addr = (void*)pg_round_down(_addr);
	uint32_t *kernel = _allocate_frame(addr);
	if(kernel == NULL) {
		if(!swap_out())
			exit(-1); // TODO panic
		kernel = _allocate_frame(addr);
		ASSERT(kernel!=NULL);
	}
	return kernel;
}

void deallocate_frame(void *addr){
	struct frame_table_entry fte;
	struct hash_elem *e;

	fte.user = addr;
	fte.owner = thread_current();
	lock_acquire(&frame_table_lock);
	e = hash_find(&frame_table, &fte.hash_elem);
	hash_delete(&frame_table,e);
	list_remove(&hash_entry(e, struct frame_table_entry, hash_elem)->list_elem);
	lock_release(&frame_table_lock);

	palloc_free_page(hash_entry(e, struct frame_table_entry, hash_elem)->kernel);
	free(hash_entry(e, struct frame_table_entry, hash_elem));
}

struct frame_table_entry* find_fte(void *addr){ //user
	struct frame_table_entry fte;
	struct hash_elem *e;
	fte.user = addr;
	fte.owner = thread_current();
	lock_acquire(&frame_table_lock);
	e = hash_find(&frame_table, &fte.hash_elem);
	lock_release(&frame_table_lock);
	if(e==NULL)
		return NULL;
	return hash_entry(e, struct frame_table_entry, hash_elem);
}