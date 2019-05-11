#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#include "userprog/process.h"
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
	uint32_t* kernel = hash_entry(e, struct frame_table_entry, hash_elem)->kernel;
	return hash_int((int)kernel);
}

bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	//struct thread *thread_a = hash_entry(a, struct frame_table_entry, hash_elem)->owner;
	//struct thread *thread_b = hash_entry(b, struct frame_table_entry, hash_elem)->owner;
	//if(thread_a != thread_b)
	//	return thread_a < thread_b;
	return hash_entry(a, struct frame_table_entry, hash_elem)->kernel < 
		   hash_entry(b, struct frame_table_entry, hash_elem)->kernel;
}

void eviction_ptr_push(struct list_elem *list_elem){
	struct list_elem *current_ptr = eviction_ptr;
	if(list_elem == eviction_ptr){
		current_ptr = list_next(current_ptr);
		if(current_ptr == list_tail(&frame_table))
			current_ptr = list_begin(&frame_table);
	}
	eviction_ptr = current_ptr;
	return;
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
	lock_acquire(&frame_table_lock);
	if(fte == NULL){
		lock_release(&frame_table_lock);
		return NULL;
	}
	fte->swap_prevention = true;
	fte->kernel = palloc_get_page(PAL_USER | PAL_ZERO);
	if(fte->kernel == NULL){
		free(fte);
		lock_release(&frame_table_lock);
		return NULL;
	}
	fte->user = addr;
	fte->owner = thread_current();
	list_push_front(&frame_list, &fte->list_elem);
	hash_insert(&frame_table, &fte->hash_elem);

	struct sup_page_table_entry spte;
	spte.user_vaddr = addr;
	struct hash_elem *e;
	if((e = hash_find(thread_current()->sup_page_dir, &spte.hash_elem)) == NULL){
		fte->spte = allocate_page(addr);
    	fte->spte->writable = true;
	}
	else{
		fte->spte = hash_entry(e, struct sup_page_table_entry, hash_elem);
	}
	fte->spte->kpage = fte->kernel;
	fte->spte->user_vaddr = addr;	
	fte->spte->state = SPTE_MAPPED;
	
	fte->swap_prevention = false;
	lock_release(&frame_table_lock);

	return fte->kernel;
}

uint32_t *
allocate_frame (void *_addr){
	void *addr = (void*)pg_round_down(_addr);
	uint32_t *kernel;// = _allocate_frame(addr);
	while((kernel = _allocate_frame(addr)) == NULL) {
		if(!swap_out())
			exit(-1); // TODO panic
	}
	return kernel;
}

void deallocate_frame(void *addr){
	lock_acquire(&frame_table_lock);
	struct frame_table_entry *fte = find_fte(addr);
	hash_delete(&frame_table, &fte->hash_elem);
	eviction_ptr_push(&fte->list_elem);
	list_remove(&fte->list_elem);
	pagedir_clear_page(thread_current()->pagedir, addr);

	palloc_free_page(fte->kernel);
	free(fte);
	lock_release(&frame_table_lock);
}
void deallocate_fte(struct frame_table_entry *fte){
	lock_acquire(&frame_table_lock);
	hash_delete(&frame_table, &fte->hash_elem);
	eviction_ptr_push(&fte->list_elem);
	list_remove(&fte->list_elem);
	pagedir_clear_page(fte->owner->pagedir, fte->user);
	palloc_free_page(fte->kernel);
	free(fte);
	lock_release(&frame_table_lock);
}

void deallocate_frame_owned_by_thread(void){
	struct thread *t = thread_current();
	struct list_elem *e;
	lock_acquire(&frame_table_lock);
    for(e = list_begin(&frame_list);e!=list_end(&frame_list);e = list_next(e)){
    	struct frame_table_entry *fte = list_entry(e,struct frame_table_entry, list_elem);
    	if(fte->owner == t){
    		hash_delete(&frame_table,&fte->hash_elem);
    		ASSERT(list_head(&frame_list)!=e);
    		e=list_prev(e);
    		eviction_ptr_push(&fte->list_elem);
    		list_remove(&fte->list_elem);
    		free(fte);
    	}
    }
    lock_release(&frame_table_lock);
}

struct frame_table_entry *find_fte(void *addr){ //user
	struct sup_page_table_entry *spte = find_spte(addr);
	if(spte == NULL){
		return NULL;
	}
	struct frame_table_entry fte;
	struct hash_elem *e;
	if(!spte->kpage){ // NOT LOADED YET
		return NULL;
	}
	fte.kernel = spte->kpage;
	fte.owner = thread_current();
	e = hash_find(&frame_table, &fte.hash_elem);

	if(e == NULL){
		return NULL;
	}
	return hash_entry(e, struct frame_table_entry, hash_elem);
}

void swap_prevent_on(void *addr){
	struct sup_page_table_entry *spte = find_spte(addr);
	if(spte == NULL)
		return;
	struct frame_table_entry *fte;
	lock_acquire(&frame_table_lock);
	fte = find_fte(addr);
	lock_release(&frame_table_lock);
	if(fte == NULL){
		ASSERT(spte->state != SPTE_MAPPED);
		if(spte->state == SPTE_EVICTED)
			swap_in(addr, spte);
		else if(spte->state == SPTE_LOAD)
			lazy_load_page(spte);
	}
	lock_acquire(&frame_table_lock);
	fte = find_fte(addr);
	lock_release(&frame_table_lock);
	ASSERT(fte);
	fte->swap_prevention = true;

}

void swap_prevent_off(void *addr){
	struct sup_page_table_entry *spte = find_spte(addr);
	if(spte == NULL)
		return;
	struct frame_table_entry *fte;
	lock_acquire(&frame_table_lock);
	fte = find_fte(addr);
	lock_release(&frame_table_lock);
	if(fte == NULL){
		ASSERT(spte->state != SPTE_MAPPED);
		if(spte->state == SPTE_EVICTED)
			swap_in(addr, spte);
		else if(spte->state == SPTE_LOAD)
			lazy_load_page(spte);
	}
	lock_acquire(&frame_table_lock);
	fte = find_fte(addr);
	lock_release(&frame_table_lock);
	ASSERT(fte);
	fte->swap_prevention = false;

}

void swap_prevention_buffer(const void *buf, size_t size, bool onoff){
	void *page;
	for(page = pg_round_down(buf); page < buf + size; page += PGSIZE)
		onoff ? swap_prevent_on(page) : swap_prevent_off(page);
}