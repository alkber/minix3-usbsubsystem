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
 * December 2009
 * 
 * (C) Copyright 2009,2010 Althaf K Backer <althafkbacker@gmail.com> 
 * (C) Copyright 1998 The NetBSD Foundation, Inc.
 * (C) Copyright 1999 Linus Torvalds <torvalds@transmeta.com>
 * 
 * UHCI HCD driver implementation.
 */

#include "../drivers.h"
#include "../libdriver/driver.h"

#include <minix/ds.h>
#include <minix/vm.h>
#include <minix/sysutil.h>
#include <ibm/pci.h>

#include <sys/mman.h>
#include <sys/shm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usbd.h"
#include "uhci.h"
#include "usbdmem.h"


#define PCI_SBRN   0x60
#define INTEL_VID  0x8086
#define INTEL_DID_82371SB_2 0x7020
#define DRIVER "uhci-hcd"
#define INTR 8

int n = 3;

extern u32_t system_hz;
extern int errno;

char dat[100] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* pci_dev has no relation to PCI subsystem  and is local */
PRIVATE struct pci_dev {
	u16_t vid;
	u16_t did;
};

/* 
 * Table of UHCI controllers  used in probing,this driver 
 * currently support only one host controller of same type 
 * the first one that is found in the active one.
 *
 * you can add the pci vendor:product id here , make sure 
 * the list end with {0,0} , and do update /etc/drivers.conf or
 * similar for uhci-hcd part.
 */
PRIVATE struct pci_dev uhci_pcitab[] = { {INTEL_VID, INTEL_DID_82371SB_2},
					 {0,0}
				       };

PRIVATE struct uhci_rh {
	int port1_status;
	int port2_status;
};

PRIVATE struct uhci_int_waitq {
	int refcnt;
	uhci_qh_t *qh;
};

#if 0
/* Meant for later use ,it encapsulate  all the details of a device
 * associated with deviceid that is provieded by usbd,details include
 * the transfers associated.
 */
PRIVATE struct wrapper_qh {
	 uhci_qh_t *qh;
	 struct wrapper_qh *next;
};
PRIVATE struct uhci_device {
    usbd_dev_id_t devid;
	struct wrapper_qh intr;
	struct wrapper_qh ls_ctl;
	struct wrapper_qh fs_ctl;
	struct uhci_device *next;
};
#endif 

PRIVATE struct uhci {
	u8_t rev;		
	u8_t irq;		
	u8_t sbrnum;		
	u8_t saved_sof;
	u16_t saved_frnum;
	port_t reg_base;	
	u32_t usbd_proc;
	int irq_hook;
	message m2uhci;

	struct uhci_rh root_hub;
	struct usbd_page fl_page;
	struct usbd_page global_td_pool;
	struct usbd_page global_qh_pool;
	uhci_td_t *td_isoc[ISOC_FL_CNT];
	uhci_qh_t *qh_intr[INTR_FL_CNT];
	uhci_qh_t *qh_ls_ctl;
	uhci_qh_t *qh_fs_ctl;
	uhci_qh_t *qh_bulk;
	struct uhci_device *dev_lst_start;
};

struct uhci hc;
struct uhci_int_waitq ctl_waitq;

/** Port IO **/
_PROTOTYPE(u8_t uhci_inb, (u16_t port));
#define UINB(port) uhci_inb(hc.reg_base + port)
_PROTOTYPE(u16_t uhci_inw, (u16_t port));
#define UINW(port) uhci_inw(hc.reg_base + port)
_PROTOTYPE(u32_t uhci_inl, (u16_t port));
#define UINL(port) uhci_inl(hc.reg_base + port)
_PROTOTYPE(void uhci_outb, (u16_t port, u8_t value));
#define UOUTB(port,value) uhci_outb(hc.reg_base + port,value)
_PROTOTYPE(void uhci_outw, (u16_t port, u16_t value));
#define UOUTW(port,value) uhci_outw(hc.reg_base + port,value)
_PROTOTYPE(void uhci_outl, (u16_t port, u32_t value));
#define UOUTL(port,value) uhci_outl(hc.reg_base + port,value)

/** Register operations **/
#define UHCI_WRITEW(reg,bitpos)     UOUTW(reg,(UINW(reg) | bitpos))
#define UHCI_READW(reg)             UINW(reg)
#define UHCI_READW_BIT(reg,bitpos) (UINW(reg) & bitpos)
#define UHCI_CLRW_BIT(reg,bitpos)   UOUTW(reg,(UINW(reg) & (0xffff^bitpos)))
#define UHCI_WRITEB(reg,val)        UOUTB(reg,val)
#define UHCI_READB(reg)             UINB(reg)
#define UHCI_FL_INIT(fl_start)      UOUTL(UHCI_FLBASEADDR,fl_start)
#define UHCI_FLBASE_GET() 	    UINL(UHCI_FLBASEADDR)
#define UHCI_FRNUM_SET(fr_num)      UOUTW(UHCI_FRNUM,fr_num)
#define UHCI_FRNUM_GET()            UINW(UHCI_FRNUM)
/**						*/

_PROTOTYPE(void uhci_control_req,(message));
_PROTOTYPE(int uhci_hw_restart, (void));
_PROTOTYPE(int uhci_msg_usbd, (message *));
_PROTOTYPE(int uhci_notify_usbd, (int));
_PROTOTYPE(int uhci_pci_probe, (void));
_PROTOTYPE(int uhci_rh_port_reset, (u16_t));
_PROTOTYPE(int uhci_transfer_from, (message,int));
_PROTOTYPE(int uhci_interrupt_req, (message));
_PROTOTYPE(void uhci_cancel_xfer, (message));
_PROTOTYPE(void uhci_ctl_xfer_dealloc, (uhci_qh_t *));
_PROTOTYPE(void uhci_ctl_xfer_dealloc, (uhci_qh_t *));
_PROTOTYPE(void uhci_ctl_xfer_poll, (int));
_PROTOTYPE(void uhci_drv_exit, (void));
_PROTOTYPE(int  uhci_drv_init, (void));
_PROTOTYPE(void uhci_ds_init, (void));
_PROTOTYPE(void uhci_dumpreg, (void));
_PROTOTYPE(void uhci_dumptd,(uhci_td_t *));
_PROTOTYPE(void uhci_fatal_abort, (char *));
_PROTOTYPE(void uhci_hw_isr, (void));
_PROTOTYPE(void uhci_hw_reset, (void));
_PROTOTYPE(void uhci_hw_start, (void));
_PROTOTYPE(void uhci_interrupt_xfer_complete, (void));
_PROTOTYPE(uhci_qh_t *uhci_qh_alloc, (struct usbd_page *));
_PROTOTYPE(uhci_td_t *uhci_td_alloc, (struct usbd_page *));
_PROTOTYPE(void uhci_append_qh_bf, (uhci_qh_t *,uhci_qh_t *));
_PROTOTYPE(void uhci_append_td_df, (uhci_qh_t *, uhci_td_t *));
_PROTOTYPE(void uhci_td_dealloc,(struct usbd_page *, uhci_td_t *));
_PROTOTYPE(void uhci_qh_dealloc,(struct usbd_page *, uhci_qh_t *));
_PROTOTYPE(void uhci_device_attached, (message m2uhci));
_PROTOTYPE(int  uhci_register_hc_usbd, (void));
_PROTOTYPE(void uhci_rh_found_csc, (u16_t));
_PROTOTYPE(void uhci_rh_scan_ports, (void));
_PROTOTYPE(void uhci_scatter_intrqh, (int));
_PROTOTYPE(void uhci_interrupt_xfer_complete, (void));
_PROTOTYPE(void uhci_intr_xfer_dealloc, (uhci_qh_t *));

/* Millisecond delay */
#define uhci_msleep(delay) usleep(delay*1000)
#undef  DPRINT_PREFIX
#define DPRINT_PREFIX "\nuhci-hcd: "

int main(void)
{
	int r;
	system_hz = sys_hz();
	
	r = uhci_drv_init();
	if (OK != r) {
		uhci_fatal_abort("uhci initialization failed");
	}
	
	uhci_register_hc_usbd();
	
	while (TRUE) {
		  if ((r = receive(ANY, &hc.m2uhci)) != OK)
			  panic("uhci-hcd:", "receive failed", r);

		  switch (hc.m2uhci.m_source) {
		  case RS_PROC_NR:
			   notify(hc.m2uhci.m_source);
		  break;
		  case HARDWARE:
			   uhci_hw_isr();
		  break;
		  case CLOCK:
			   uhci_rh_scan_ports();
			   if (sys_setalarm(system_hz * n, 0) != OK)
				   DPRINTF(1, ("failed to set synchronous alarm"));
			   if (ctl_waitq.qh)
				   uhci_ctl_xfer_poll(1);
		  break;
		  case PM_PROC_NR: {
					   sigset_t set;
					   if (OK != getsigset(&set))
						   break;
					   if (sigismember(&set, SIGTERM)) {
				    		/* Possible reason for a deadlock when minix sends SIGTERM at
					         * shutdown ,one solution is to call sendnb(),which is done here. 
				    		 */
				   		uhci_notify_usbd(HC2USBD_HC_DERGISTER);
				   		uhci_drv_exit();
					   }
				   }
		  break;
		  default:
			    DPRINTF(0, ("default %d ", hc.m2uhci.m_type));
				goto usbd2hc_msg;
		  } 
		
		continue;

 usbd2hc_msg:
		switch (hc.m2uhci.m_type) {
		case USBD2HC_HC_REGISTERED:
			 DPRINTF(1, ("registered with usbd"));
		break;
		case USBD2HC_HC_REGISTER_FAIL:
			 DPRINTF(1, ("registration failed with usbd"));
		break;
		case USBD2HC_CONTROL_REQ:
			 DPRINTF(0, ("control request received from usbd"));
			 uhci_control_req(hc.m2uhci);
		break;
		case USBD2HC_INTERRUPT_REQ:
			 DPRINTF(0, ("interrupt request received from usbd"));
			 uhci_interrupt_req(hc.m2uhci);
		break;
		case USBD2HC_BULK_REQ:
			  /* TO DO */
		break;
		case USBD2HC_ISOC_REQ:
			  /* TO DO */
		break;
		case USBD2ALL_SIGTERM:
			 DPRINTF(1,("SIGTERM received from usbd , driver crashing.."));
		break;
		case USBD2HC_CANCEL_XFER:
			 uhci_cancel_xfer(hc.m2uhci);
			 DPRINTF(0, ("cancel xfer"));
		break;
		default:
			 DPRINTF(0, ("unknown message type %d from source %d", 
						 hc.m2uhci.m_type,hc.m2uhci.m_source));
		}
	} 
	return (OK);
}

int uhci_drv_init()
{
	U32_t self_proc;
	int r;
	DPRINTF(1, ("started"));
	
	r = ds_retrieve_u32("uhci-hcd", &self_proc);
	if (OK != r) {
		DPRINTF(1,("failed to retrieve uhci-hcd"));
		return r;
	}
	
	/* Other choice could be to loop for few micro seconds
	 * and exit with time out ,at the moment we keep it simple
	 */
	r = ds_retrieve_u32("usbd", &hc.usbd_proc);
	if (OK != r) {
		DPRINTF(1,("failed to retrieve usbd proc"));
		hc.usbd_proc = 0;
		return r;
	}
	
	r = uhci_notify_usbd(HC2USBD_PING);
	if (OK != r) {
		DPRINTF(1,("usbd didn't reply to initial ping"));
		hc.usbd_proc = 0;
		return r;
	}
	
	r = uhci_pci_probe();
	if (FALSE == r) {
		DPRINTF(1,("unable to find any uhci host controllers"));
		hc.usbd_proc = 0;
		return -1;
	}

	hc.irq_hook = hc.irq;
	if ((r = sys_irqsetpolicy(hc.irq, 0, &hc.irq_hook)) != OK)
		panic("uhci-hcd", "set IRQ policy failed: %d", r);

	if ((r = sys_irqenable(&hc.irq_hook)) != OK)
		panic("uhci-hcd", "IRQ re-enable failed: %d", r);

	if (sys_setalarm(system_hz * n, 0) != OK)
		DPRINTF(1, ("failed to set synchronous alarm"));

	/* Initialize uhci data structures */
	uhci_ds_init();
	
	/* Stop the controller if at all running */
	UHCI_CLRW_BIT(UHCI_CMD, CMD_RS);
	uhci_hw_reset();
	uhci_hw_start();
	return OK;
}

void uhci_drv_exit()
{
	/* Stop the controller */
	if (hc.usbd_proc) 
		UHCI_CLRW_BIT(UHCI_CMD, CMD_RS);

	if (hc.fl_page.vir_start)
		usbd_free_page(&hc.fl_page);
		
	if (hc.global_td_pool.vir_start)
		usbd_free_page(&hc.global_td_pool);
		
	if (hc.global_qh_pool.vir_start)
		usbd_free_page(&hc.global_qh_pool);
		
	DPRINTF(1, ("terminated"));
}

void uhci_hw_start()
{
	DPRINTF(0, ("starting uhci hcd"));
	/* Enable All the Interrupts */
	UHCI_WRITEW(UHCI_INTR, INTR_TOCRCIE | INTR_RIE | INTR_IOCE | INTR_SPIE);

	if (uhci_hw_restart() == FALSE) {
		DPRINTF(1, ("failed to start controller"));
		return;
	}

	DPRINTF(0, ("frame number %d", UHCI_FRNUM_GET()));
}

void uhci_ds_init()
{
	int i = 0;
	uhci_qh_t *qh_ls;
	uhci_qh_t *qh_fs;
	uhci_qh_t *qh_intr;
	uhci_td_t *td_tmp;
	uhci_qh_t *qh_bulk;
	uhci_td_t *td_tmp2;
	uhci_qh_t *tmp_qh;
	vir_bytes *frame_lst;
	phys_bytes phys;

	ctl_waitq.qh = NULL;
	ctl_waitq.refcnt = 0;
   	/* Allocate 64K , align in 16 byte boundary*/
	usbd_init_page(sizeof(uhci_td_t), A64K_ALIGN16,&hc.global_td_pool);
	DPRINTF(0,("\n Global td pool phys 0x%08x",hc.global_td_pool.phys_start));
	usbd_init_page(sizeof(uhci_qh_t), A64K_ALIGN16,&hc.global_qh_pool);
	DPRINTF(0,("\n Global qh pool phys 0x%08x",hc.global_qh_pool.phys_start));
	usbd_init_page(sizeof(u32_t), A4K, &hc.fl_page);
	DPRINTF(0,("\n fl  pool phys 0x%08x", hc.fl_page.phys_start));

	hc.qh_ls_ctl = uhci_qh_alloc(&hc.global_qh_pool);
	hc.qh_fs_ctl = uhci_qh_alloc(&hc.global_qh_pool);
	hc.qh_bulk   = uhci_qh_alloc(&hc.global_qh_pool);
	
	/*     
	 * fl_page->full speed isochronous -> interrupt qhs -> low speed control- 
	 *          -> full speed control  -> bulk transfers
	 *
	 * fl_page      td_isoc    qh_intr   qh_intr   qh_ls_ctl  qh_fs_ctl qh_bulk    
	 * [0        ]->[0      ]->[0     ]->[0   ] -> [0   ] ->  [0   ] -> [0   ]  
	 * [1        ]->[1      ]->[1     ] . 					 .
	 * .         .  .       .  .      . . 					 .
	 * .         .  .       .  .      . . 					 .        
	 * [127      ]->[127    ]->[127   ] . 					 .
	 * [128      ]->[0      ]->[0     ] . 					 .
	 * .         .  .       .  .      . . 					 .
	 * .         .  .       .  .      . . 					 .
	 * [255      ]->[127    ]->[127   ] . 					 .
	 * [256      ]->[0      ]->[0     ] . 					 .
	 * .         .  .       .  .      . . 					 .    
	 * .         .  .       .  .      . .					 .   
	 * [1023     ]->[127    ]  [127   ] .				         .
	 *
	 */

	qh_ls = hc.qh_ls_ctl;

	DPRINTF(0, ("allocating intr qh"));
	for (i = 0; i < INTR_FL_CNT; i++) {
		hc.qh_intr[i] = uhci_qh_alloc(&hc.global_qh_pool);
		hc.qh_intr[i]->elink_ptr = LP_TER;
		/* qh_intr  qh_ls_ctl
		 * [i   ] -> [     ] 
		 */
		qh_intr = hc.qh_intr[i];
		qh_intr->h_lnext = qh_ls;
		qh_intr->link_ptr = qh_ls->phys;
		qh_intr->v_tdstart = 0;
		qh_ls->v_tdlast = 0;
	}
    	/* Currently we just scatter all the IN interrupt request at 
	 * 8ms latency
	 */
	uhci_scatter_intrqh(INTR);
	DPRINTF(0, ("allocating isoc td"));
	/* td_isoc       qh_intr
	 * [0....127] -> [0....127]  
	 */
	for (i = 0; i != ISOC_FL_CNT; i++) {

		hc.td_isoc[i] = uhci_td_alloc(&hc.global_td_pool);

		qh_intr = hc.qh_intr[i];
		td_tmp = hc.td_isoc[i];

		td_tmp->next = 0;
		td_tmp->link_ptr = qh_intr->phys;
		td_tmp->status = TD_ISO;
		td_tmp->token = 0;
		td_tmp->buffer_ptr = 0;
	}
	
	/* qh_ls_ctl   qh_fs_ctl
	 * [      ] -> [      ]   
	 */
	qh_ls = hc.qh_ls_ctl;
	qh_fs = hc.qh_fs_ctl;

	qh_ls->h_lnext = qh_fs;
	qh_ls->link_ptr = qh_fs->phys;
	qh_ls->elink_ptr = LP_TER;
	qh_ls->v_tdstart = 0;
	qh_ls->v_tdlast = 0;
	/* qh_fs_ctl  qh_fs_bulk 
	 * [      ] -> [      ] 
	 */
	qh_fs = hc.qh_fs_ctl;
	qh_bulk = hc.qh_bulk;

	qh_fs->h_lnext = qh_bulk;
	qh_fs->link_ptr = qh_bulk->phys;
	qh_fs->elink_ptr = LP_TER;
	qh_fs->v_tdstart = 0;
	qh_ls->v_tdlast = 0;

	td_tmp = uhci_td_alloc(&hc.global_td_pool);
	td_tmp->next = 0;
	td_tmp->status = 1;
	td_tmp->token = 1;
	td_tmp->buffer_ptr = 0;
	td_tmp->link_ptr = LP_TER;

	qh_bulk->h_lnext = 0;
	qh_bulk->link_ptr = LP_TER;
	qh_bulk->elink_ptr = td_tmp->phys;
	qh_bulk->v_tdstart = td_tmp;
	qh_ls->v_tdlast = 0;

	frame_lst = hc.fl_page.vir_start;

	for (i = 0; i != FL_CNT; i++) {
		frame_lst[i] = hc.td_isoc[i % ISOC_FL_CNT]->phys;
	}
	hc.saved_sof = 0x40;
	hc.saved_frnum = 0x0;
	UHCI_FRNUM_SET(hc.saved_frnum);
	UHCI_WRITEB(UHCI_SOF, hc.saved_sof);
}

int uhci_hw_restart()
{

	if (UHCI_READW_BIT(UHCI_CMD, CMD_RS) & CMD_RS) {
		DPRINTF(1, ("uhci hc already started\n"));
		return FALSE;
	}

	DPRINTF(0, ("restarting uhci hc"));

	/* Write Frame base address */
	UHCI_FL_INIT(hc.fl_page.phys_start);

	/* We assign max packet size to be 64bytes */
	UHCI_WRITEW(UHCI_CMD, CMD_MAXP | CMD_RS);

	uhci_msleep(DELAY_10MS);

	DPRINTF(0, ("frame number %d", UHCI_FRNUM_GET()));
	/*   0 started            & 1  =  0  */
	/*   1 stopped            & 1  =  1  */
	if (UHCI_READW_BIT(UHCI_STS, STS_HCH) & STS_HCH) {
		DPRINTF(1, ("uhci hc failed to restart"));
		return FALSE;
	}
	return TRUE;
}

void uhci_hw_reset()
{
	int ctr = 0;

	DPRINTF(0, ("resetting uhci hc"));

	/* Disable all the interrupts */
	UHCI_CLRW_BIT(UHCI_INTR, INTR_TOCRCIE | INTR_RIE | INTR_IOCE | INTR_SPIE);
	
	/* Global Reset */
	UHCI_WRITEW(UHCI_CMD, CMD_GRESET);
	uhci_msleep(GRESET_DELAY);
	
	/* Reset the host controller logic and terminate all transfers */
	UHCI_WRITEW(UHCI_CMD, CMD_HCRESET);
	
	/* Poll untill CMD_HCRESET is logic 0 ie reset is complete     */
	ctr = HCRESET_WAIT_CNT;
	
	while (ctr--) {
		uhci_msleep(HCRESET_DELAY);
		/* 1 not complete       & 1   -> 1 ~ 0 */
		/* 0 reset complete     & 1   -> 0 ~ 1 */
		if (!(UHCI_READW_BIT(UHCI_CMD, CMD_HCRESET) & CMD_HCRESET))
			goto hc_reset_ok;
	}
	
	DPRINTF(1, ("uhci hc did not reset"));

 hc_reset_ok:
	return;
}

void uhci_hw_isr(void)
{
	int r;
	u32_t status;
	message reply;

	DPRINTF(0, ("isr executed"));
	status = UHCI_READW(UHCI_STS) & STS_ALLINTRS;
	if (status == 0) {
		DPRINTF(1, ("the interrupt was not for us"));
		goto intr_done;
	}

	if (status & STS_RD) {
		DPRINTF(1, ("resume detect"));
	}

	if (status & STS_HSE) {
		DPRINTF(1, ("host system error"));
	}

	if (status & STS_HCPE) {
		DPRINTF(1, ("host controller process error"));
	}

	if (status & STS_HCH) {
		DPRINTF(1, ("host controller halted"));
	}

	if (status & STS_USBINT) {
		DPRINTF(0, ("transaction complete or spd"));
		if (ctl_waitq.qh)
			uhci_ctl_xfer_poll(0);	
		uhci_interrupt_xfer_complete();
	}

	/* get acknowledge bits */
	status &= (STS_USBINT | STS_USBEI | STS_RD | STS_HSE | STS_HCPE);

	if (!status) {
		/* nothing to acknowledge */
		goto intr_done;
	}
	/* acknowledge interrupts */
	UHCI_WRITEW(UHCI_STS, status);
 intr_done:
	if ((r = sys_irqenable(&hc.irq_hook)) != OK)
		panic("uhci-hcd", "IRQ renable failed: %d", r);
}

int uhci_pci_probe()
{
	int i = 0, r, devind;
	u16_t vid, did;
	char *dname;

	pci_init();

	if (!(r = pci_first_dev(&devind, &vid, &did))) 
		return FALSE;
		
	/* Probing for the table entries, don't get puzzled if 
	 * this probing logic doesn't make sense ,it entirely 
	 * depend on the entries in /etc/drivers.conf or similar 
	 */
	while (TRUE) {
		for (i = 0; (uhci_pcitab[i].vid != 0); i++) {
			if (vid == uhci_pcitab[i].vid && did == uhci_pcitab[i].did) 
				goto found_uhci;
		}
		
		if (!(r = pci_next_dev(&devind, &vid, &did))) {
			return FALSE;
		}
	}

 found_uhci:
	if (!(dname = pci_dev_name(vid, did))) {
		dname = "UHCI Compatible Controller Found";
	}

	DPRINTF(1,("%s (%x:%x) at %s", dname, vid, did, pci_slot_name(devind)));

	/*  Claim our device for privileges */
	if ((r = pci_reserve_ok(devind)) != OK) 
		panic("uhci", "failed to reserve PCI device", r);
	
	hc.rev = pci_attr_r8(devind, PCI_REV);
	hc.irq = pci_attr_r8(devind, PCI_ILR);
	hc.sbrnum = pci_attr_r8(devind, PCI_SBRN);
	hc.reg_base = (pci_attr_r32(devind, PCI_BAR_5) & 0xfffe);

#if 0
	DPRINTF(0, ("information"));
	DPRINTF(0, ("revision: %x", hc.rev));
	DPRINTF(0, ("irq: %x", hc.irq));
	DPRINTF(0, ("usb specs ver: %x", hc.sbrnum));
	DPRINTF(0, ("bar: %x", hc.reg_base));
#endif
	return TRUE;
}

/* 
 * Register with USBD 
 * 
 * Following members are used
 * 
 * m_type : contain the message type HC2USBD_REGISTER
 * m1_i1  : amount of memory for the devices that are part of hcd
 * 			A4K
 * 			A64K 
 * Note: the memory is allocated in pages ie 4K and 64K 
 * and hcd doesn't have access to this memory it is only meant 
 * for usbd to allocate device within it,alternatively and 
 * implementation of hcd could have a local list of devices ie 
 * Blocked coded 
 */
int uhci_register_hc_usbd()
{
	message m2usbd;	
	int r;
	
	m2usbd.m_type = HC2USBD_REGISTER;
	m2usbd.m1_i1 = A4K;
	/* we are blocked */
	r = send(hc.usbd_proc, &m2usbd);
	if (OK != r) 
		DPRINTF(1, ("failed to register with usbd"));
	return r;
}

uhci_td_t *uhci_td_alloc(struct usbd_page *td_page)
{
	uhci_td_t *new_td;
	phys_bytes td_phys;

	if (!(new_td = (uhci_td_t *) usbd_const_alloc(td_page))) {
		DPRINTF(1, ("td allocation failed"));
		return NULL;
	}
	
	memset(new_td, 0, sizeof(new_td));
	new_td->phys = usbd_vir_to_phys(new_td);

	if (new_td->phys == EINVAL)
	    return NULL;

	new_td->phys = LP_TD_SET(new_td->phys);
	return new_td;
}

void uhci_td_dealloc(struct usbd_page *td_page, uhci_td_t *this_td)
{
	usbd_const_dealloc(td_page, this_td);
}

uhci_qh_t *uhci_qh_alloc(struct usbd_page *qh_page)
{
	uhci_qh_t *new_qh;
	phys_bytes qh_phys;

	if (!(new_qh = (uhci_qh_t *)usbd_const_alloc(qh_page))) {
		DPRINTF(1, ("qh allocation failed"));
		return NULL;
	}
	
	memset(new_qh, 0, sizeof(new_qh));
	new_qh->phys = usbd_vir_to_phys(new_qh);
	
	if (new_qh->phys == EINVAL)
		return NULL;

	new_qh->phys = LP_QH_SET(new_qh->phys);
	return new_qh;
}

void uhci_qh_dealloc(struct usbd_page *qh_page, uhci_qh_t *this_qh)
{
	usbd_const_dealloc(qh_page, this_qh);
}

/* Append td is depth first order to a given qh 
 * 
 * intital case: qh
 *  	  	 |
 *  	  	 td
 * 		 |
 *  	 	 LP_TER
 * 
 * second case   qh 
 * 		 |
 * 		 td 
 * 		 |
 *  		 td 
 * 		 |
 * 		 LP_TER
 */
void uhci_append_td_df(uhci_qh_t *qh, uhci_td_t *src_td)
{
	unsigned long destination;
	if (qh->v_tdlast == NULL) {

		qh->elink_ptr = src_td->phys;
		LP_TER_SET(src_td->link_ptr);

		qh->v_tdlast = src_td;
		qh->v_tdstart = src_td;
		qh->v_tdstart->prev = NULL;
		qh->v_tdlast->prev = qh->v_tdstart;
		qh->v_tdlast->next = NULL;
		
		DPRINTF(0, ("pass 1"));
	} else {
		DPRINTF(0, ("\n qh->v_tdlast %x", qh->v_tdlast));
		qh->v_tdlast->link_ptr = src_td->phys;
		/* Set VF = 1 for depth */
		LP_VF_D_SET(qh->v_tdlast->link_ptr);
		LP_TER_SET(src_td->link_ptr);

		qh->v_tdlast->next = src_td;
		src_td->prev = qh->v_tdlast;
		src_td->next = NULL;
		qh->v_tdlast = src_td;
		DPRINTF(0, ("pass 2h"));
	}
}

void uhci_scatter_intrqh(int interval)
{	
	int x, i, cnt;
	uhci_qh_t *iqh;
	
	iqh = hc.qh_intr[interval];
	cnt = (128/interval);
	i = 2;
	x = interval;
	
	while (i <= cnt) {
		hc.qh_intr[x * i]->link_ptr = iqh->phys; 
#if 0
		   printf("\n i = %d x * i = %d",i,x*i);
#endif 	
		   i++;
	}
}


void uhci_rh_scan_ports()
{
	unsigned int port_status;
	/* If devices attached to both ports at the time of scanning ie device 
	 * in PORTSC1 and PORTSC2,the attempt to notify usbd within 
	 * uhci_rh_found_csc leads to a deadlock, in order to avoid that we scan the 
	 * ports in separate,thus static makes sense to switch among ports.
	 */
	static u16_t port = UHCI_PORTSC1;
	port_status = UHCI_READW(port);
	/* Checking if the 'Current Connect Status' changed */
	if (port_status & UHCI_PORTSC_CSC) {
		DPRINTF(0, ("CSC changed %d", port));
		uhci_rh_found_csc(port);
		/* Clear the CSC as this change was only meant for this period, 
		 * according to UHCI specs its upto the software to manage
		 * the state change. 
		 */
		port_status = UHCI_READW(port);
		UHCI_WRITEW(port, (port_status, UHCI_PORTSC_CSC));
	}
	port = (port == UHCI_PORTSC1) ? UHCI_PORTSC2 : UHCI_PORTSC1;
	DPRINTF(0, ("%d next_port", port));
}

void uhci_rh_found_csc(u16_t port)
{
	message m2usbd;
	unsigned int port_status = UHCI_READW(port);
	DPRINTF(0, ("port %d status %x", port, port_status));
	/* port number */
	m2usbd.m1_i1 = (port == UHCI_PORTSC1) ? 1 : 2;
	/* Check to see if a device is connected or not */
	if (!(port_status & UHCI_PORTSC_CCS)) {
		/* If we are here then mostly no device is present, we clear the PE and 
		 * set 'Port enable or disable change' bit.
		 */
		port_status =
		    (port_status & ~UHCI_PORTSC_PE) | UHCI_PORTSC_POEDC;
		UHCI_WRITEW(port, port_status);
		DPRINTF(0,("port %d status %d", port,
			 port_status & UHCI_PORTSC_CCS));
		/* device got disconnected or not present we inform usbd about it.*/
		m2usbd.m_type = HC2USBD_DEVICE_DISCONNECTED;
		uhci_msg_usbd(&m2usbd);
		return;
	}

	DPRINTF(0, ("found a device"));
	/* Low speed device ? */
	m2usbd.m1_i2 = (UHCI_READW(port) & UHCI_PORTSC_LSDA) ? 1 : 0;
	DPRINTF(0,("port %d status %x %d", port , UHCI_READW(port), m2usbd.m1_i2));
	/* Delay for completion of the insertion process as per specs */
	uhci_msleep(100);
	/* Reset the port as per USB specification 2.0, check page 243 9.1.2 point 3
	 */
	uhci_rh_port_reset(port);
	/* Inform usbd about the new device found and port */
	m2usbd.m_type = HC2USBD_NEW_DEVICE_FOUND;
	uhci_msg_usbd(&m2usbd);
}


int uhci_rh_port_reset(u16_t port)
{
	int wait_cnt = 10;
	int port_status = 0;
    	/*
     	* TO DO: check the proper delay timing 
     	*/
    
	/* Reset port */
	UHCI_WRITEW(port, UHCI_PORTSC_PR);
	/* Providing reasonable delay for RESET signaling to be active */
	uhci_msleep(100);

	DPRINTF(0, ("port %d reset,status0 = 0x%04x", port, UHCI_READW(port)));

	port_status = UHCI_READW(port);
	/* Set port to be not in suspend or reset */
	UHCI_WRITEW(port, (port_status & ~(UHCI_PORTSC_PR | UHCI_PORTSC_SUSP)));
	uhci_msleep(80);
	DPRINTF(0, ("port %d reset, status1 = 0x%04x", port, UHCI_READW(port)));

	/* Enable our port */
	UHCI_WRITEW(port, UHCI_PORTSC_PE);
	/* Loop untill the port is enabled , there can be delay in host
	 * controller in enabling the port if any transactions is currently
	 * in progress
	 */
	while (wait_cnt > 0) {
		uhci_msleep(80);

		port_status = UHCI_READW(port);

		DPRINTF(0,
			("port %d iteration %u, status = 0x%04x", port,
			 wait_cnt, port_status));
		if (!(port_status & UHCI_PORTSC_CCS)) {
			/*
			 * No device is connected (or was disconnected
			 * during reset).  Consider the port reset.
			 * The delay must be long enough to ensure on
			 * the initial iteration that the device
			 * connection will have been registered.  50ms
			 * appears to be sufficient, but 20ms is not.
			 */
			DPRINTF(1,
				("port %d loop %u, device detached", port,
				 wait_cnt));
			break;
		}

		if (port_status & (UHCI_PORTSC_POEDC | UHCI_PORTSC_CSC)) {
			/*
			 * Port enabled changed and/or connection
			 * status changed were set.  Reset either or
			 * both raised flags (by writing a 1 to that
			 * bit), and wait again for state to settle.
			 */
			UHCI_WRITEW(port, (port_status &
					   (UHCI_PORTSC_POEDC |
					    UHCI_PORTSC_CSC)));
			continue;
		}

		/* Port is enabled */
		if (port_status & UHCI_PORTSC_PE)
			break;

		UHCI_WRITEW(port, UHCI_PORTSC_PE);
		--wait_cnt;
	}
      
    port_status = UHCI_READW(port);
    UOUTW(port,(port_status & ~UHCI_PORTSC_PR));
	DPRINTF(0, ("port %d reset, status2 = 0x%04x 0x%04x", 
				port, UHCI_READW(port),(port_status & ~UHCI_PORTSC_PR)));
    
	if (wait_cnt <= 0) {
		DPRINTF(1, ("\nuhci-hcd: port %d reset timed out", port));
		return -1;
	}

	DPRINTF(0, ("port %d reset complete", port));
	return 0;
}

int uhci_msg_usbd(message * m2usbd)
{
	int r = 0;
	DPRINTF(0, ("sending message .."));
	r = send(hc.usbd_proc, m2usbd);
	if (r != OK) {
		DPRINTF(1, ("send() to usbd failed %d", r));
	}
	DPRINTF(0, ("message send.."));
	return r;

}

int uhci_notify_usbd(int event)
{
	message m2usbd;
	int r = 0;
	DPRINTF(0, ("sending event notifcation .."));
	m2usbd.m_type = event;
	r = sendnb(hc.usbd_proc, &m2usbd);
	if (r != OK) {
		DPRINTF(1, ("send() to usbd failed %d", r));
	}
	DPRINTF(0, ("event notification send.."));
	return r;
}

/*
 * One of the clean way to terminate a driver, what i understood from 
 * service utility thanks to sphere from #minix for the suggestion
 * to emulate 'service down' within the driver.
 */
void uhci_fatal_abort(char *reason)
{
	message m2rs;
	m2rs.m_type = RS_DOWN;
	m2rs.RS_CMD_ADDR = DRIVER;
	m2rs.RS_CMD_LEN = strlen(DRIVER);

	DPRINTF(1, ("%s: aborted: %s", DRIVER, reason));
	send(RS_PROC_NR, &m2rs);
}

u8_t uhci_inb(u16_t port)
{
	u32_t value;
	int r;
	if ((r = sys_inb(port, &value)) != OK)
		DPRINTF(1, ("uhci-hcd: sys_inb_failed: %d\n", r));
	return value;
}

u16_t uhci_inw(u16_t port)
{
	u32_t value;
	int r;
	if ((r = sys_inw(port, &value)) != OK)
		DPRINTF(1, ("uhci-hcd: sys_inw_failed: %d\n", r));
	return value;
}

u32_t uhci_inl(u16_t port)
{
	u32_t value;
	int r;
	if ((r = sys_inb(port, &value)) != OK)
		DPRINTF(1, ("sys_inb_failed: %d\n", r));
	return value;
}

void uhci_outb(u16_t port, u8_t value)
{
	int r;
	if ((r = sys_outw(port, value)) != OK)
		DPRINTF(1, ("sys_outb failed: %d", r));
}

void uhci_outw(u16_t port, u16_t value)
{
	int r;
	if ((r = sys_outw(port, value)) != OK)
		DPRINTF(1, ("uhci-hcd: sys_outw failed: %d", r));
}

void uhci_outl(u16_t port, u32_t value)
{
	int r;
	if ((r = sys_outl(port, value)) != OK)
		DPRINTF(1, ("sys_outl failed: %d\n", r));
}

void uhci_dump_qh(uhci_qh_t *qh)
{
	uhci_td_t *current = qh->v_tdstart;
	while (current) {
		printf("\n td :0x%x td->link :0x%x", current,
		       current->link_ptr);
		current = current->next;
	}
}

void uhci_dumptd(uhci_td_t *td)
{
	if (!td) 
		return;	
	printf("\n TD: link_ptr 0x%08x"
	       "\n TD: T  %x"
	       "\n TD: Q  %x"
	       "\n TD: VF %x"
	       "\n TD: status and control -"
	       "\n TD: Actlen %x"
	       "\n TD: status --"
	       "\n TD: Bitstuff error %x"
	       "\n TD: CRC/T OUT %x"
	       "\n TD: NAK %x"
	       "\n TD: Babble %x"
	       "\n TD: Data buffer error %x"
	       "\n TD: Stalled %x"
	       "\n TD: Active %x"
	       "\n TD: IOC %x"
	       "\n TD: ISO %08x"
	       "\n TD: LS %x"
	       "\n TD: SPD %x"
	       "\n TD: token -"
	       "\n TD: PID %x "
	       "\n TD: Device Address 0x%08x"
	       "\n TD: Endpt %x"
	       "\n TD: Data toggle %x"
	       "\n TD: Maxlen %d"
	       "\n TD: buffer_ptr 0x%08x"
	       "\n TD: sw td->next 0x%08x",
	       td->link_ptr,
	       td->link_ptr & LP_TER,
	       td->link_ptr & LP_QH,
	       td->link_ptr & LP_VF_D,
	       td->status & TD_ACTLEN,
	       td->status & TD_BITSTUFF,
	       td->status & TD_CRCTO,
	       td->status & TD_NAK,
	       td->status & TD_BABBLE,
	       td->status & TD_DBUFFER,
	       td->status & TD_STALLED,
	       td->status & TD_ACTIVE,
	       td->status & TD_IOC,
	       td->status & TD_ISO,
	       td->status & TD_LS,
	       td->status & TD_SPD,
	       td->token & TD_PID,
	       TD_DEVADDR_GET(td->token),
	       TD_ENDPT_GET(td->token),
	       TD_DT_GET(td->token),
	       TD_MAXLEN_GET(td->token), td->buffer_ptr, td->next);
}

void uhci_dumpreg()
{ 
#if 0
	 printf("\n UHCI Register Dump \n");

	   printf("UHCI_CMD:0x%04x\n CMD_RS:0x%x"
	   "\n CMD_HCRESET:0x%x\n CMD_GRESET:0x%x"
	   "\n CMD_EGSM:0x%x\n CMD_FGR:0x%x"
	   "\n CMD_SWDBG:0x%x\n CMD_CF:0x%x"
	   "\n CMD_MAXP:0x%x\nUHCI_STS:0x%x"
	   "\n STS_USBINT:0x%04x\n STS_USBEI:0x%x"
	   "\n STS_RD:0x%x\n STS_HSE:0x%x"
	   "\n STS_HCPE:0x%x\n STS_HCH:0x%x"
	   "\nUHCI_INTR:0x%04x\n INTR_TOCRECIE:0x%x"
	   "\n INTR_RIE:0x%x\n INTR_IOCE:0x%x"
	   "\n INTR_SPIE:0x%x\nUHCI_FRNUM:0x%04x"
	   "\nUHCI_FRBASEADDR:0x%08x\nUHCI_SOFMOD:0x%02x"
	   "\nUHCI_PORTSC1:0x%04x\nUHCI_PORTSC2:0x%04x\n",              
	   UHCI_INW(UHCI_CMD),
	   UHCI_CMD_GET(CMD_RS),
	   UHCI_CMD_GET(CMD_HCRESET),
	   UHCI_CMD_GET(CMD_GRESET),
	   UHCI_CMD_GET(CMD_EGSM),
	   UHCI_CMD_GET(CMD_FGR),
	   UHCI_CMD_GET(CMD_SWDBG),
	   UHCI_CMD_GET(CMD_CF),
	   UHCI_CMD_GET(CMD_MAXP),
	   UHCI_INW(UHCI_STS),
	   UHCI_STS_GET(STS_USBINT),
	   UHCI_STS_GET(STS_USBEI),
	   UHCI_STS_GET(STS_RD),
	   UHCI_STS_GET(STS_HSE),
	   UHCI_STS_GET(STS_HCPE),
	   UHCI_STS_GET(STS_HCH),
	   UHCI_INW(UHCI_INTR),
	   UHCI_INTR_GET(INTR_TOCRCIE),
	   UHCI_INTR_GET(INTR_RIE),
	   UHCI_INTR_GET(INTR_IOCE),
	   UHCI_INTR_GET(INTR_SPIE),
	   UHCI_INW(UHCI_FRNUM),
	   UHCI_INL(UHCI_FLBASEADDR),
	   UHCI_INB(UHCI_SOF),
	   UHCI_INW(UHCI_PORTSC1),
	   UHCI_INW(UHCI_PORTSC2));  
#endif
}

/*
 * UHCI Control request
 * 
 * Following message members are used for input from usbd
 * m2_l2 : pipe
 * m2_i1 : data length
 * m2_l1 : device request physical address 
 * m2_p1 : data buffer physical address  
 * 
 * Return :
 * In this implementation of usb stack  all the control request are 
 * synchronous ie who ever requested is blocked util we reply , here we
 * submit the request to the uhci skeleton and wait , when transfer is
 * complete IOC is obtained ,in case transfer doesn't complete we time
 * out in uhci_ctl_xfer_poll(1) which is called at CLOCK switch.
 * 
 *
 * Reply depends on status in uhci_ctl_xfer_poll(0) with 0 as parameter 
 * called within uhci_hw_isr() 
 */
void uhci_control_req(message m2uhci)
{
	u32_t token, status, pipe;
	i32_t data_len;
	phys_bytes dr_phys, data_phys;
	message rmessage;
	uhci_td_t *td;
	uhci_qh_t *qh_local;

	/* TD for the control request and the qh to hook with  active ctl qh */
	td = uhci_td_alloc(&hc.global_td_pool);
	qh_local = uhci_qh_alloc(&hc.global_qh_pool);
	/* Initialize them for ctl_xfer polling once submitted to uhci*/
	ctl_waitq.refcnt = 0;
	ctl_waitq.qh = qh_local;
	
	/* failed allocation */
	if (!qh_local || !td) {
		rmessage.m1_i1 = ENOMEM;
		if (td)
			uhci_td_dealloc(&hc.global_td_pool, td);
		if (qh_local)
			uhci_qh_dealloc(&hc.global_qh_pool, qh_local);
		/* usbd_control_req() is blocked until we reply */
		sendnb(m2uhci.m_source, &rmessage);
		return;
	}
	
	/* Extract details from the message */
	pipe	  = m2uhci.m2_l2;
	data_len  = m2uhci.m2_i1;
	dr_phys   = m2uhci.m2_l1;
	data_phys = (phys_bytes) m2uhci.m2_p1;

#if 0 
	DPRINTF(0, ("data phys 0x%x", data_phys));
    DPRINTF(0, ("data lenght %d", data_len));
    DPRINTF(0, ("pipe 0x%x", pipe));
    DPRINTF(0, ("dr phys 0x%x", dr_phys));
#endif 

	/* destination in bits 8--18 check usbd.h */
	token      = (pipe & 0x0007ff00) | TD_PID_SETUP;
	status     = (pipe & TD_LS) | TD_ACTIVE | TD_SPD | TD_ERRCNT_SET(3);
	td->status = status;
	/* device request is alway 8 bytes */
	td->token = token | (7 << 21);
	td->buffer_ptr = dr_phys;
	/*
	 * qh_local
	 * 	  |
	 *    td
	 */  
	uhci_append_td_df(qh_local, td);

	token ^= (TD_PID_SETUP ^ TD_PID_IN);	 /* SETUP -> IN */
	
	if (usb_pipeout(pipe)) 
		token ^= (TD_PID_OUT ^ TD_PID_IN); /* IN -> OUT   */

	/* Assemble the data tds */
	while (data_len > 0) {
		/* Build the TD for control status */
		int pktsze = data_len;
		int maxsze = usb_maxpacket(pipe);

		DPRINTF(0, ("max packet size %d", maxsze));
		if (pktsze > maxsze)
			pktsze = maxsze;

		/* data td */
		td = uhci_td_alloc(&hc.global_td_pool);
		if (!td) {
			rmessage.m1_i1 = ENOMEM;
			uhci_ctl_xfer_dealloc(qh_local);
			sendnb(m2uhci.m_source, &rmessage);
			return;
		}
		/* Alternate Data 0/1 (start with Data1) */
		token ^= TD_DATA_TOGGLE;
		
		td->status = status;
		td->token = token | ((pktsze - 1) << 21);	/* pktsze bytes of data */
		td->buffer_ptr = data_phys;
		/*
		* 	qh_local
		* 	  |
		*    	td->LP_TER
		*	  |
		*    	td->LP_TER
		* 	  |
		* 	td->LP_TER
		*/ 
		uhci_append_td_df(qh_local, td);
#if 0
		DPRINTF(0, (" %d data_len data  phys %x", data_len, data_phys));
#endif
		data_phys += maxsze;
		data_len -= maxsze;
	}

	/* control status td */
	td = uhci_td_alloc(&hc.global_td_pool);
	if (!td) {
		rmessage.m1_i1 = ENOMEM;
		uhci_ctl_xfer_dealloc(qh_local);
		sendnb(m2uhci.m_source, &rmessage);
		return;
	}

	token ^= (TD_PID_OUT ^ TD_PID_IN);	/* OUT -> IN */
	token |= TD_DATA_TOGGLE;	/* End in Data1 */

	td->status = status | TD_IOC;
	td->token = token | (0x7ff << 21); /* 0 bytes of data */
	td->buffer_ptr = 0;
	
	uhci_append_td_df(qh_local, td);

	/* Hook to our active ctl_qh 
	 * qh_ls_ctl..............qh_fs_ctl    
	 *    |        		  /
	 * qh_local->link_ptr--->/ 
	 *    |
	 *    td
	 *    .
	 *    .  
	 */
	/* TO DO: full speed control transfers are supposed to goto 
	 * hc.qh_fs_ctl since current implementation is synchronous it
	 * has no effect so we queue it to low speed qh 
	 */
	qh_local->link_ptr = hc.qh_ls_ctl->link_ptr;
	hc.qh_ls_ctl->elink_ptr = qh_local->phys | LP_QH;

	DPRINTF(0,("uhci_control_msg: out"));
}

/* Read  uhci_control_req() for info */
void uhci_ctl_xfer_poll(int clock)
{
	uhci_td_t *current;
	uhci_td_t *prev;
	unsigned int status;
	message rmessage;
	int dbg_i = 0;
	int r = 0;
	
	if (!ctl_waitq.qh)
		return;
		
	if (clock) {
		if (ctl_waitq.refcnt >= 3) {
			uhci_dumptd(ctl_waitq.qh->v_tdstart->next);
			rmessage.m1_i1 = ETIMEDOUT;
			uhci_msg_usbd(&rmessage);
			goto err;
		}
		ctl_waitq.refcnt++;
		return;
	}
	prev = current = ctl_waitq.qh->v_tdstart;
	/* This part is only executed on an IOC */
	while (current) {
		status = (current->status >> 16) & 0xff;
		if (!status) {
			/*prev = current;*/
			current = current->next;
		}
		else {
			DPRINTF(0, ("\n status %d %x", dbg_i, status));
			uhci_dumptd(current);
			rmessage.m1_i1 = EIO;
			uhci_msg_usbd(&rmessage);
			goto err;
		}
		dbg_i++;
	}
	DPRINTF(0,("control xfer complete"));
	rmessage.m1_i1 = OK;
	/*uhci_dumptd(prev);*/
	uhci_msg_usbd(&rmessage);
	/* Reset for next xfer*/
 err:
	uhci_ctl_xfer_dealloc(ctl_waitq.qh);
	ctl_waitq.qh = NULL;
	ctl_waitq.refcnt = 0;
	return;
}

/* Deallocate the control transfer after it is finished */
void uhci_ctl_xfer_dealloc(uhci_qh_t *qh)
{
	uhci_td_t *current;
	uhci_td_t *prev;

	current = qh->v_tdstart;

	while (current) {
		prev = current->next;
		uhci_td_dealloc(&hc.global_td_pool, current);
		current = prev;
	}

	hc.qh_ls_ctl->elink_ptr = LP_TER;
	uhci_qh_dealloc(&hc.global_qh_pool, qh);
	DPRINTF(0, ("xfer deallocated"));
	return;
}

/* Interrupt request    
 * 
 * m2_l1 : physical address of the data 
 * m2_l2 : pipe 
 * m2_p1 : device id from usbd
 * 
 * reply.m1_i1 = OK  if all fine 
 * reply.m1_i1 = ENOMEM no memory 
 * we are blocked ,but usbd is waiting for a status reply 
 */ 
int uhci_interrupt_req(message m2uhci)
{
	uhci_td_t *td; 
	uhci_qh_t *interrupt_qh; 
	struct uhci_device *udev;
        u32_t status, pipe, token;
	phys_bytes dr_phys, data_phys;
	message reply;
	
	td = uhci_td_alloc(&hc.global_td_pool);
	interrupt_qh = uhci_qh_alloc(&hc.global_qh_pool);
	
	if (!interrupt_qh || !td) {
		reply.m1_i1 = ENOMEM;
		if (td)
			uhci_td_dealloc(&hc.global_td_pool, td);
		if (interrupt_qh)
			uhci_qh_dealloc(&hc.global_qh_pool, interrupt_qh);
		uhci_msg_usbd(&reply);	
		return ENOMEM;
	}
	
	interrupt_qh->devid = (usbd_dev_id_t) m2uhci.m2_p1;
	pipe = m2uhci.m2_l2;
	data_phys = m2uhci.m2_l1;
	token = (pipe & 0x0007ff00) | ((pipe & 0xf0) ? TD_PID_IN : TD_PID_OUT);
	status = (pipe & TD_LS) | TD_IOC | TD_ACTIVE | TD_SPD | TD_ERRCNT_SET(0);
	td->status = status;			
	td->token = token | (7 << 21);	/* 8 bytes of data */
	td->buffer_ptr = data_phys;
	
#if 0
	DPRINTF(1, ("Intr pipe   0x%x", pipe));
	DPRINTF(1, ("Intr token  0x%x",token));	
	DPRINTF(1, ("Intr status 0x%x",status));
	DPRINTF(1, ("Intr data phys 0x%x", data_phys));
#endif
	uhci_append_td_df(interrupt_qh, td);
	uhci_append_qh_bf(hc.qh_intr[INTR], interrupt_qh);
	
	reply.m1_i1 = OK;
	uhci_msg_usbd(&reply);
	return 0;
}

void uhci_interrupt_xfer_complete()
{   
	/* TO DO: Time being we just assume all the interrupt request 
	 * need a theoretical latency of 8ms ,later on we have to 
	 * check others for xfer completing check.
	 */
	 uhci_qh_t *intrqh, *prevqh; 
	 uhci_td_t *savetd;
	 message reply;
	 
	 intrqh = hc.qh_intr[INTR]->v_lnext;
	 prevqh  = hc.qh_intr[INTR];
	 
	 while (intrqh) {
		 if (intrqh->v_tdstart) {
			 if (!(intrqh->v_tdstart->status & TD_ACTIVE)) {
				 /* td executed successfully notify usbd */
				 reply.m_type = USB_INTERRUPT_REQ_STS;
				 reply.m2_i1 = OK;
				 reply.m2_l1 = intrqh->devid;
				 uhci_msg_usbd(&reply);
					
				 savetd = intrqh->v_tdstart;
				 intrqh->v_tdlast = NULL;
				 uhci_append_td_df(intrqh,savetd);
				 /* sync with hc */
				 intrqh->v_tdstart->token ^= TD_DATA_TOGGLE;
				 intrqh->v_tdstart->status |= TD_ACTIVE;
			 }
		 }	
		 intrqh = intrqh->h_lnext;
	 }	
	 DPRINTF(0, ("out of interrupt_xfer_complete()"));
}

/*
 * Append qh in breadth first order , the inital hook would be depth 
 * first ie to a known qh in the skeleton in uhci_ds_init()
 * 
 * inital case: [dqh]     (v  means vertical ,h horizontal) 
 * 		  |
 * 		 v_link = sqh
 * 
 * second case: [dqh]-->h_link = sqh
 * 
 */  
void uhci_append_qh_bf(uhci_qh_t *dqh,uhci_qh_t *sqh)
{
	if (!dqh || !sqh)
		return;
	/* Actually initial append is df */ 	
	if (NULL == dqh->last_qh) {
		dqh->last_qh   = sqh;
		sqh->link_ptr  = dqh->link_ptr;
		dqh->elink_ptr = sqh->phys | LP_QH;
		dqh->v_lnext = sqh;
	} else {
		sqh->link_ptr = dqh->last_qh->link_ptr;
		dqh->last_qh->link_ptr = sqh->phys | LP_QH;
		dqh->last_qh->h_lnext = sqh;
		dqh->last_qh = sqh;
	}
}

/*
 * Does ,the opposite of uhci_append_qh_bf() 
 */
void uhci_remove_qh_bf(uhci_qh_t *rqh,uhci_qh_t* rprevqh)
{

	if (rqh == hc.qh_intr[INTR]->v_lnext) {
		
		if (rqh->h_lnext) {
			hc.qh_intr[INTR]->v_lnext = rqh->h_lnext;
			hc.qh_intr[INTR]->elink_ptr = rqh->h_lnext->phys | LP_QH;
			uhci_qh_dealloc(&hc.global_qh_pool,rqh);
			DPRINTF(0,("next one"));
			return;
		} 
		hc.qh_intr[INTR]->elink_ptr = LP_TER;
		hc.qh_intr[INTR]->v_lnext = NULL;
		hc.qh_intr[INTR]->last_qh = NULL;
		uhci_qh_dealloc(&hc.global_qh_pool,rqh);
		DPRINTF(0,("start"));
		return;
	}
	if (!rqh || !rprevqh)
		return;
	/*TODO: deactivate td */	
	rprevqh->h_lnext  = rqh->h_lnext;
	rprevqh->link_ptr = rqh->link_ptr;
    uhci_qh_dealloc(&hc.global_qh_pool,rqh);
}

/*
 * To keep things simple a unique deviceid is associated with the each 
 * qh, the skeleton qhs in uhci_ds_init() are not associated with 
 * any device,in case of interrupt qh when doing a xfer cancel ,
 * we would have to check each latency interval 8 ,16..etc to see
 * if any xfer is associated with device as a device could choose
 * different latency for different endpoints
 * 
 * Another way to this would be to keep a data structure with primary 
 * key as deviceid and linked list of xfers associated with it,
 *  
 * This function cancel all the xfer related to type requested ie
 * ie if a device had interrupt request at 8 and 16 both will be removed.
 * 
 * This is for time being called when a device detached and not meant to
 * cancel at run.
 */  

void uhci_cancel_xfer(message m2uhci)
{
	usbd_dev_id_t devid = m2uhci.m2_l1;
	switch(m2uhci.m2_i1) {
	case XFER_ALL:
  	     /*    */
	break;
	case XFER_ISOC:
	     /*    */
	break;
	case XFER_BULK:
	     /*    */
    break;
	case XFER_INTERRUPT: {
		 /* we just assume all the interrupt request needs  only 8ms 
		  * latency ,to keep it simple. Later on we have to check all the 
		  * latencies like 8 16 ..64 	
		  */
		 uhci_qh_t *intr = hc.qh_intr[INTR]->v_lnext;
		 uhci_qh_t *prev = NULL;
		 while (intr) {
			 if (intr->devid == devid) {
				 uhci_intr_xfer_dealloc(intr);
				 uhci_remove_qh_bf(intr, prev);		
			 }			    
			 prev = intr;
			 intr = intr->v_lnext;
		  }
		}	  
	break;
    default:
		DPRINTF(1,("unknown xfer cancel request"))
	}
	return;
}
/* Deallocate the interrupt transfer ,usually called when device
 * is detached 
 */
void uhci_intr_xfer_dealloc(uhci_qh_t *qh)
{
	uhci_td_t *current;
	uhci_td_t *next;

	current = qh->v_tdstart;

	while (current) {
		next = current->next;
		uhci_td_dealloc(&hc.global_td_pool, current);
		current = next;
	}
	
	qh->elink_ptr = LP_TER;
	qh->v_tdlast = NULL;
	qh->v_tdstart = NULL;
	DPRINTF(0,("intr xfer deallocated"));
	return;
}

