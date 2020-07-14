#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <stdint.h>
#include <stdbool.h>
#include "vm/page.h"

extern struct lock swap_lock;

void swap_init (void);
bool swap_in (void *addr, struct sup_page_table_entry *spte);
bool swap_out (void);
void read_from_disk (uint8_t *frame, int index);
void write_to_disk (uint8_t *frame, int index);
void swap_free (int ofs);
#endif /* vm/swap.h */
