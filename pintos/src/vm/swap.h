//////////////////////////////// PJ3 EDITED /////////////////////////////////
#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdio.h>

void swap_init (void);
void swap_in (size_t used_index, void *kaddr);
size_t swap_out (void *kaddr);

#endif /* vm/swap.h */
/////////////////////////////////////////////////////////////////////////////
