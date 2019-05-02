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
	uint32_t *pd = thread_current()->sup_page_dir;
	uint32_t *pde = pd + pd_no(addr);
	struct sup_page_table_entry **pte;
	if(*pde == 0){ //not mapped
		*pde = palloc_get_page(PAL_ZERO);
		pte = *pde + pt_no(addr);
		*pte = palloc_get_page(PAL_ZERO);
	}
	(*pte)->user_vaddr = addr;
	(*pte)->access_time = timer_ticks();
	return *pte;
}

