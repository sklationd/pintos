#include "vm/page.h"

/*
 * Initialize supplementary page table
 */

void create_sup_pd() {
	thread_current()->sup_page_dir = palloc_get_page(PAL_ZERO);
}

void 
page_init (void)
{
	create_sup_pd();
}

/*
 * Make new supplemental page table entry for addr 
 */
struct sup_page_table_entry *
allocate_page (void *addr)
{
	struct sup_page_table_entry *ent;
	uint32_t *pd = thread_current()->sup_page_dir;
	uint32_t *pde = pd + pd_no(addr);
	struct sup_page_table_entry **pte;
	if(*pde == 0){ //not mapped
		*pde = vtop(palloc_get_page(PAL_ZERO));
	}
	pte = ptov(*pde) + pt_no(addr);
	*pte = vtop(palloc_get_page(PAL_ZERO));
	ent = (struct sup_page_table_entry *)ptov(*pte);
	ent->kernel_vaddr = pte;
	ent->user_vaddr = addr;
	ent->access_time = timer_ticks();
	ent->accessed = 0;
	ent->dirty = 0;
	return ent;
}

void deallocate_page(void *addr){
	uint32_t *pd = thread_current()->sup_page_dir;
	uint32_t *pde = pd + pd_no(addr);
	struct sup_page_table_entry **pte;
	pte = ptov(*pde) + pt_no(addr);
	palloc_free_page((*pte)->kernel_vaddr);
}


