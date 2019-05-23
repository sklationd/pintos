#include "vm/swap.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <hash.h>

/* The swap device */
static struct disk *swap_device;

/* Tracks in-use and free swap slots */
static struct bitmap *swap_table;

/* Protects swap_table */
struct lock swap_lock;

/* 
 * Initialize swap_device, swap_table, and swap_lock.
 */
void 
swap_init (void)
{
	swap_device = disk_get(1,1);
	ASSERT(swap_device != NULL);
	swap_table = bitmap_create(disk_size(swap_device) * DISK_SECTOR_SIZE / PGSIZE);
	lock_init(&swap_lock);
}

/*
 * Reclaim a frame from swap device.
 * 1. Check that the page has been already evicted. 
 * 2. You will want to evict an already existing frame
 * to make space to read from the disk to cache. 
 * 3. Re-link the new frame with the corresponding supplemental
 * page table entry. 
 * 4. Do NOT create a new supplemental page table entry. Use the 
 * already existing one. 
 * 5. Use helper function read_from_disk in order to read the contents
 * of the disk into the frame. 
 */ 
bool 
swap_in (void *addr, struct sup_page_table_entry *spte)
{	
	//printf("swap in enter %p\n", addr);
	//if(!lock_held_by_current_thread(&frame_table_lock))
	//lock_acquire(&frame_table_lock);

	ASSERT(addr < PHYS_BASE);
	const int swap_table_size = disk_size(swap_device) * DISK_SECTOR_SIZE / PGSIZE;
	struct hash_elem *e;

	ASSERT(spte->state == SPTE_EVICTED);

	// 2, 3
	uint8_t *kpage = allocate_frame(addr);
	ASSERT(kpage);
	// 5
	struct frame_table_entry *fte;
	fte = find_fte(addr);

	read_from_disk(fte->kernel, spte->swap_offset);
	bitmap_set(swap_table,spte->swap_offset,0);
	//lock_release(&frame_table_lock);
	swap_prevent_off(addr);
}

/* 
 * Evict a frame to swap device. 
 * 1. Choose the frame you want to evict. 
 * (Ex. Least Recently Used policy -> Compare the timestamps when each 
 * frame is last accessed)
 * 2. Evict the frame. Unlink the frame from the supplementary page table entry
 * Remove the frame from the frame table after freeing the frame with
 * pagedir_clear_page. 
 * 3. Do NOT delete the supplementary page table entry. The process
 * should have the illusion that they still have the page allocated to
 * them. 
 * 4. Find a free block to write you data. Use swap table to get track
 * of in-use and free swap slots.
 */
bool
swap_out (void)
{	
	//printf("out enter\n");
	struct thread *t;
	const int swap_table_size = disk_size(swap_device) * DISK_SECTOR_SIZE / PGSIZE;
	if(bitmap_all(swap_table,0,swap_table_size)){
		exit(-1);
	}
	if(eviction_ptr == NULL){
		eviction_ptr = list_begin(&frame_list);
	}
	struct frame_table_entry *fte; 

	while(1){
		fte = list_entry(eviction_ptr, struct frame_table_entry, list_elem);
		t = fte->owner;
		if(!fte->swap_prevention){
			if(pagedir_is_accessed(t->pagedir,fte->user)){
				pagedir_set_accessed(t->pagedir,fte->user, 0);
			}
			else 
				break;
		}
		eviction_ptr_push(&(fte->list_elem));
	}
	size_t index = bitmap_scan(swap_table, 0, 1, 0);
	if(index == BITMAP_ERROR)
		PANIC("swap full\n");
	bitmap_set(swap_table, index,1);
	ASSERT(fte);
	pagedir_clear_page(t->pagedir, fte->user);
	fte->spte->state = SPTE_EVICTED;
	fte->spte->swap_offset = index;
	fte->spte->dirty = fte->spte->dirty || pagedir_is_dirty(t->pagedir, fte->user) /* || pagedir_is_dirty(t->pagedir, fte->kernel)*/;
	struct sup_page_table_entry *spte = fte->spte;
	write_to_disk(fte->kernel, index);
	eviction_ptr_push(&(fte->list_elem));
	deallocate_fte(fte);
	spte->kpage = NULL;
	return true;
}

/* 
 * Read data from swap device to frame. 
 * Look at device/disk.c
 */
void read_from_disk (uint8_t *frame, int index)
{
	int i;
	for(i = 0; i < (PGSIZE / DISK_SECTOR_SIZE); ++i) // i < 8
		disk_read(swap_device, index * (PGSIZE / DISK_SECTOR_SIZE) + i, frame + (DISK_SECTOR_SIZE*i));
}

/* Write data to swap device from frame */
void write_to_disk (uint8_t *frame, int index)
{
	int i;
	for(i = 0; i < (PGSIZE / DISK_SECTOR_SIZE); ++i) // i < 8
		disk_write(swap_device, index * (PGSIZE / DISK_SECTOR_SIZE) + i, frame + (DISK_SECTOR_SIZE*i));
}

void swap_free(int ofs){
	ASSERT(bitmap_test(swap_table,ofs));
	bitmap_set(swap_table,ofs,0);
}
