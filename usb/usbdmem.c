/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * January 2010
 * 
 * (C) Copyright 2009,2010 Althaf K Backer <althafkbacker@gmail.com> 
 * 
 * USB driver memory management routines.
 * 
 * These could be used instead of the standard memory management routines,
 * by the drivers that is suppose to communicate with USBD and layer below,
 * these guarantee that we get a contiguous memory chunks , of initialized 
 * size. 
 *
 * Main reason for this particular implementation of memory management is 
 * UHCI, according to the design guide transfer descriptors and queue head
 * should have an address that is aligned to 16byte boundary, this tiny slab 
 * allocator guarantee that for each instance of qh and tds,next minix 
 * malloc() couldn't guarantee that we always get address of such nature.
 *
 * usbdmem.o could be linked to other drivers for the purpose. 
 *    
 */
#include "usbdmem.h"

/* Allocates 4K / 64K page and fill up usbd_page information  */
int usbd_init_page(size_t struct_size, int pageflag, struct usbd_page *page)
{
	int cnt;

	if (page == NULL) {
		DPRINTF(1, ("NULL reference caught"));
		return EINVAL;
	}

	switch (pageflag) {
	case A4K:
		page->size = I386_PAGE_SIZE;
		break;
	case A64K:
		page->size = 64 * 1024;
		break;
	case A4K_ALIGN16:
		page->per_object_len = ALIGN16;
		page->size = I386_PAGE_SIZE;
		pageflag = A4K;
		break;
	case A64K_ALIGN16:
		page->per_object_len = ALIGN16;
		page->size = 64 * 1024;
		pageflag = A64K;
		break;
	default:
		DPRINTF(1, ("unknown page flag"));
		return ENOMEM;
	}

	if (page->per_object_len != HC_DEV_FLAG)
		if (!
		    (page->vir_start =
		     (vir_bytes *) alloc_contig(page->size, pageflag,
						&page->phys_start)))
			return ENOMEM;

	if (page->per_object_len == ALIGN16) {
		cnt = (struct_size / 16);
		struct_size = (cnt * 16) >= struct_size ? (cnt * 16) : ((cnt + 1) * 16);
	}

	DPRINTF(0, ("page->per_object_len %d", struct_size));
	page->per_object_len = struct_size;
	page->capacity = (page->size / page->per_object_len);
	page->filled_cnt = 0;
	page->next_free = (u8_t *) page->vir_start;
	page->dealloc_start = NULL;
#if 0
	DPRINTF(1, ("\n Page info\n Page size:%d\n Object len: %d\n"
		    "\n Capacity:%d\n Phys: 0x%08x\n Vir: 0x%08x\n",
		    page->size, page->per_object_len, page->capacity,
		    page->phys_start, page->vir_start));
#endif 
	return OK;
}

/* Free the page allocated by the usbd_init_page() */
void usbd_free_page(struct usbd_page *this_page)
{
	int r = 0;
	if (this_page == NULL) {
		DPRINTF(1, ("NULL reference caught"));
		return;
	}

	if ((r = munmap(this_page->vir_start, this_page->size)) != OK)
		DPRINTF(1, ("usbdmem: munmap failed %d\n", r));

	DPRINTF(0,
		("freed page vir %08x phys %08x", this_page->vir_start,
		 this_page->phys_start));
}

/* Once we have the page allocated we virtually allocate
 * within the page with usbd_page->per_object_len chunk,
 * as per request ,its called const as it allocate predefined 
 * size instead of variable size.
 */
void *usbd_const_alloc(struct usbd_page *this_page)
{
	vir_bytes *allocated;
	u8_t *phys_addr;

	if (this_page == NULL) {
		DPRINTF(1, ("NULL reference caught"));
		return NULL;
	}

	/* we use u8_t instead of phys_bytes for the pointer arithmetic. */
	phys_addr = (u8_t *) this_page->phys_start;
	
	DPRINTF(0, ("usbd_const_alloc()"))
        DPRINTF(0, ("Page index %d", this_page->filled_cnt));

	if (this_page->capacity == this_page->filled_cnt) {
		DPRINTF(1, ("usbd_const_alloc() NOMEM"));
		return NULL;
	}

	/* if we have any deallocated addresses then use them */
	if (this_page->dealloc_start != NULL) {
		allocated = (vir_bytes *) this_page->dealloc_start;
		this_page->dealloc_start = this_page->dealloc_start->next;

	} else {
		allocated = (vir_bytes *) this_page->next_free;
		this_page->next_free += this_page->per_object_len;
	}

	this_page->filled_cnt++;
#if 0
	DPRINTF(1, ("allocated  0x%08x", allocated));
	DPRINTF(1, ("next free 0x%08x", this_page->next_free));
	DPRINTF(1, ("Space left %d", this_page->capacity - this_page->filled_cnt));
#endif 
	memset(allocated, 0, this_page->per_object_len);

	return allocated;
}

/* This procedure is tricky it virtually deallocates the instances allocated 
 * by usbd_const_alloc() keep the list of the deallocated addresses from the 
 * page,trick part is the each instance of dealloc_list use the currently 
 * deallocated space for its own data structure. 
 */
void usbd_const_dealloc(struct usbd_page *this_page, void *page_chunk)
{
	struct dealloc_list *start = this_page->dealloc_start;
	struct dealloc_list *dealloc_new = page_chunk;

	DPRINTF(0, ("usbd_const_dealloc"));

	if (this_page == NULL || page_chunk == NULL) {
		DPRINTF(1, ("NULL reference caught"));
		return;
	}
	this_page->filled_cnt--;
	if (this_page->dealloc_start == NULL) {
		this_page->dealloc_start = dealloc_new;
		this_page->dealloc_start->next = NULL;
	} else {
		dealloc_new->next = this_page->dealloc_start;
		this_page->dealloc_start = dealloc_new;
	}

	DPRINTF(0, ("deallocated 0x%08x", page_chunk));
}

/*
 * Allocate arbitrary size chunks to a 4K page this is sort
 * of improper use of resource so this must not be used extensively 
 */
 void *usbd_var_alloc(int size)
{
	vir_bytes *allocated;
	phys_bytes null;

	if (size > I386_PAGE_SIZE)
		return NULL;
	allocated = (vir_bytes *) alloc_contig(size, AC_ALIGN4K, &null);
	if (allocated == NULL)
		return NULL;
	DPRINTF(0, ("allocated vir %08x", allocated));
	return allocated;
}

void usbd_var_dealloc(void *addr)
{
	int r = 0;
	vir_bytes *vaddr;

	if (addr == NULL) {
		DPRINTF(1, ("NULL reference caught"));
		return;
	}

	vaddr = (vir_bytes *) addr;
	if ((r = munmap(vaddr, I386_PAGE_SIZE)) != OK) {
		DPRINTF(1, ("munmap failed %d\n", r));
		return;
	}
	DPRINTF(0, ("deallocated vir %08x", vaddr));
}

/* Returns the physical address of given virtual address */
phys_bytes usbd_vir_to_phys(void *vir_addr)
{
	phys_bytes phys_addr;
	int r;
	if (vir_addr == NULL) {
		DPRINTF(1, ("NULL reference caught"));
		return EINVAL;
	}
	/* Find the physical address from the virtual one */
	r = sys_umap(SELF, VM_D, (vir_bytes) vir_addr, sizeof(phys_bytes),
		     &phys_addr);
	if (OK != r) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,vir_addr));
		return EINVAL;
	}
	return phys_addr;
}

/* Meant for bitmap operation ,used to maintain 128 bit 
 * bit map for the device numbers
 */
void set_bit(u8_t *bitmap, u16_t bit)
{
    bitmap[bit >> 3] |= (1 << (bit % 8));
}

void clr_bit(u8_t *bitmap, u16_t bit)
{
    bitmap[bit >> 3]  &= (0xff & ~(1 << (bit %8)));
}

int is_bit_set(u8_t *bitmap, u16_t bit)
{
    return ( (bitmap[bit >> 3] & (1 << (bit % 8))) ? 1 : 0 );
}

int next_free_bit(unsigned char *bitmap)
{
    register int i = 0;
    
    for (;i < 128; i++) {
        if (0 == is_bit_set(bitmap, i))
          return i;  
    }
    return i;
}
