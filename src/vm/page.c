#include "vm/page.h"

/*
 * Initialize supplementary page table
 */

unsigned page_hash_function(const struct hash_elem *e, void *aux){
	uint32_t* user_vaddr = hash_entry(e, struct sup_page_table_entry, hash_elem)->user_vaddr;
	return hash_int((int)user_vaddr);
}

bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	return hash_entry(a, struct sup_page_table_entry, hash_elem)->user_vaddr < 
		   hash_entry(b, struct sup_page_table_entry, hash_elem)->user_vaddr;
}

void 
page_init (void)
{
	thread_current()->sup_page_dir = malloc(sizeof(struct hash));
	hash_init(thread_current()->sup_page_dir, page_hash_function, page_hash_less, NULL);
}

/*
 * Make new supplemental page table entry for addr 
 */
struct sup_page_table_entry *
allocate_page (void *addr)
{
	struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
//	spte->kernel_vaddr = NULL;
	spte->user_vaddr = addr;
	hash_insert(thread_current()->sup_page_dir, &spte->hash_elem);
	spte->state = SPTE_MAPPED;
	return spte;
}

void deallocate_page(void *addr){
	struct sup_page_table_entry spte;
	spte.user_vaddr = addr;
	struct hash_elem *e;

	e = hash_find(thread_current()->sup_page_dir, &spte.hash_elem);
	hash_delete(thread_current()->sup_page_dir, e);
	free(hash_entry(e, struct sup_page_table_entry, hash_elem));
}
      //file, kpage, upage, page_read_bytes, page_zero_bytes, writable

bool lazy_load(struct file *file, off_t ofs, void *kpage, void *upage, size_t page_read_bytes, 
			   size_t page_zero_bytes, bool writable){
    struct frame_table_entry *fte = find_fte(upage);
    if(fte == NULL)
    	return false;
    struct sup_page_table_entry *spte = fte->spte;
    spte->user_vaddr = upage;
    spte->file = file;
    spte->ofs = ofs;
    spte->kpage = kpage;
    spte->page_read_bytes = page_read_bytes;
    spte->page_zero_bytes = page_zero_bytes;
    spte->writable = writable;
    spte->state = SPTE_LOAD; 
    return true;
}

struct sup_page_table_entry *find_spte(void *addr){ //user page address
	struct sup_page_table_entry spte;

	struct hash_elem *e;
	spte.user_vaddr = addr;
	e = hash_find(thread_current()->sup_page_dir, &spte.hash_elem);
	if(e==NULL)
		return NULL;
	return hash_entry(e, struct sup_page_table_entry, hash_elem);
}

void destroy_sup_page_table(){
	struct hash *spt = thread_current()->sup_page_dir;
	size_t i;

    for (i = 0; i < spt->bucket_cnt; i++) 
    {
        struct list *bucket = &spt->buckets[i];
        struct list_elem *elem, *next;

        for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) 
        {
            next = list_next (elem);
            struct sup_page_table_entry *spte;
            spte = hash_entry(list_entry(elem, struct hash_elem, list_elem), 
                              struct sup_page_table_entry, hash_elem);
            if(spte->state == SPTE_EVICTED){
            	swap_free(spte->swap_offset);
            }

            free(spte);

        }
    }
    hash_destroy(spt, NULL);
}