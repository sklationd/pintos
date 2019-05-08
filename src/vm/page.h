#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <stdint.h>
#include <stdbool.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "devices/timer.h"
#include <hash.h>
#include "vm/frame.h"

enum SPTE_STATE{
	SPTE_EVICTED,
	SPTE_MAPPED,
	SPTE_LOAD,
};

struct sup_page_table_entry 
{
	//uint32_t* kernel_vaddr;
	uint32_t* user_vaddr;
	struct hash_elem hash_elem;

	enum SPTE_STATE state;
	//for lazy load
	struct file *file;
	void *kpage;
	size_t page_read_bytes;
	size_t page_zero_bytes;
	bool writable;

	//for swap
	int swap_offset;
};

void page_init (void);
struct sup_page_table_entry *allocate_page (void *addr);
void deallocate_page(void *addr);
bool lazy_load(struct file *file, void *kpage, void *upage, size_t page_read_bytes, 
			   size_t page_zero_bytes, bool writable);
struct sup_page_table_entry* find_spte(void *addr); //user page address

#endif /* vm/page.h */

