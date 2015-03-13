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
 * This memory management ensure that we get a contiguous raw memory
 * allocation from an allocated page we use the chunk and type cast 
 * with appropriate structure variable.
 *  
 */

#ifndef _USBDMEM_
#define _USBDMEM_

#include <minix/type.h>
#include <minix/com.h>
#include <sys/types.h>
#include <minix/const.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/com.h>
#include <minix/type.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/vm_i386.h>
#include <minix/syslib.h>
#include <stdio.h>
#include <string.h>

#undef  DPRINT_PREFIX
#define DPRINT_PREFIX "\nusbdmem: "
#define DPRINTF(FLAG,arg) if (FLAG) { printf(DPRINT_PREFIX); printf arg; }
#define OK	     0 
#define A4K          0x01
#define A64K 	     0x04
#define HC_DEV_FLAG  0x300
#define ALIGN16      0x301
#define A64K_ALIGN16 0x302
#define A4K_ALIGN16  0x303


/* Holds the list of deallocated addresses within the page*/
struct dealloc_list {
	struct dealloc_list *next;
};

struct usbd_page {
	u32_t size;		/* page size                           */
	u16_t per_object_len;	/* size of each object in page         */
	u16_t capacity;		/* capacity = (size/per_object_len)    */
	u16_t filled_cnt;	/* Number of elements allocated        */
	phys_bytes phys_start;	/* page physical base address          */
	vir_bytes *vir_start;	/* page virtual  base address          */
	/* u8_t is used instead of vir_bytes ,for the pointer arithmetic */
	u8_t *next_free;	/* next free space in the page         */
	struct dealloc_list *dealloc_start;	/* contains list of deallocated address  */
};

_PROTOTYPE( int usbd_init_page, (size_t , int, struct usbd_page *));
_PROTOTYPE(void usbd_free_page, (struct usbd_page *));
_PROTOTYPE(void *usbd_const_alloc, (struct usbd_page *));
_PROTOTYPE(void usbd_const_dealloc,(struct usbd_page *, void *));
_PROTOTYPE(phys_bytes usbd_vir_to_phys,(void *));
_PROTOTYPE(void *usbd_var_alloc, (int));
_PROTOTYPE(void usbd_var_dealloc, (void *));
_PROTOTYPE(int is_bit_set, (u8_t *, u16_t)); 
_PROTOTYPE(void clr_bit, (u8_t *, u16_t));
_PROTOTYPE(void set_bit, (u8_t *, u16_t));
_PROTOTYPE(int next_free_bit, (u8_t *));

#endif
