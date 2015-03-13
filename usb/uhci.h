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
 * 
 * Hardware registers,flags and data structures for Intel UHCI.
 * For more information http://download.intel.com/technology/usb/UHCI11D.pdf
 */

#ifndef _UHCI_H_
#define	_UHCI_H_

/* Command Register 		*/
#define	UHCI_CMD		0x00
#define	CMD_RS			0x0001
#define	CMD_HCRESET		0x0002
#define	CMD_GRESET		0x0004
#define	CMD_EGSM		0x0008
#define	CMD_FGR			0x0010
#define	CMD_SWDBG		0x0020
#define	CMD_CF			0x0040
#define	CMD_MAXP		0x0080

#define DELAY_1MS                1
#define DELAY_10MS              10
#define DELAY_20MS              20
#define GRESET_DELAY            DELAY_10MS
#define HCRESET_DELAY  	        DELAY_1MS
#define HCRESET_WAIT_CNT	100

/* Status Register 		  */
#define	UHCI_STS		0x02
#define	STS_USBINT		0x0001
#define	STS_USBEI		0x0002
#define	STS_RD			0x0004
#define	STS_HSE			0x0008
#define	STS_HCPE		0x0010
#define	STS_HCH			0x0020
#define	STS_ALLINTRS		0x003f

/* Interrupt register 		  */
#define	UHCI_INTR		0x04
#define	INTR_TOCRCIE		0x0001
#define	INTR_RIE		0x0002
#define	INTR_IOCE		0x0004
#define	INTR_SPIE		0x0008

/* Frame Number 		  */
#define	UHCI_FRNUM		0x06
#define	UHCI_FRNUM_MASK		0x03ff

/* Frame list base address 	  */
#define	UHCI_FLBASEADDR		0x08

/* Start of frame modify register */
#define	UHCI_SOF		0x0c
#define	UHCI_SOF_MASK		0x7f

/* Port Status & Control register */
#define	UHCI_PORTSC1      	0x010
#define	UHCI_PORTSC2      	0x012
#define	UHCI_PORTSC_CCS		0x0001
#define	UHCI_PORTSC_CSC		0x0002
#define	UHCI_PORTSC_PE		0x0004
#define	UHCI_PORTSC_POEDC	0x0008
#define	UHCI_PORTSC_LS		0x0030
#define	UHCI_PORTSC_LS_SHIFT	4
#define	UHCI_PORTSC_RD		0x0040
#define	UHCI_PORTSC_LSDA	0x0100
#define	UHCI_PORTSC_PR		0x0200
#define	UHCI_PORTSC_OCI		0x0400
#define	UHCI_PORTSC_OCIC	0x0800
#define	UHCI_PORTSC_SUSP	0x1000

/* Bitmasks and operations of td status */
#define	TD_ACTLEN   0x000003ff
#define	TD_BITSTUFF	0x00020000
#define	TD_CRCTO	0x00040000
#define	TD_NAK		0x00080000
#define	TD_BABBLE	0x00100000
#define	TD_DBUFFER	0x00200000
#define	TD_STALLED	0x00400000
#define	TD_ACTIVE	0x00800000
#define	TD_IOC		0x01000000
#define	TD_ISO		0x02000000
#define	TD_LS		0x04000000
#define	TD_ERRCNT_GET(s) (((s) >> 27) & 3)
#define	TD_ERRCNT_SET(n) ((n) << 27)
#define	TD_SPD		0x20000000
#define TD_STS_GET(td_status,bitpos) (td_status & bitpos)
#define TD_ACTIVE_SET(td) (td |= TD_ACTIVE)

/* Bitmasks and operations on td token */
#define	TD_PID	 	0x000000ff
#define	TD_PID_IN	0x69
#define	TD_PID_OUT	0xe1
#define	TD_PID_SETUP 	0x2d
#define TD_DATA_TOGGLE  1 << 19
/*
#define	TD_PID_GET(s) ((s) & 0xff)
*/
#define	TD_DEVADDR_SET(a) ((a) << 8)
#define	TD_DEVADDR_GET(s) (((s) >> 8) & 0x7f)
#define	TD_ENDPT_SET(e)   (((e) & 0xf) << 15)
#define	TD_ENDPT_GET(s)   (((s) >> 15) & 0xf)
#define	TD_DT_SET(t)      ((t) << 19)
#define	TD_DT_GET(s)      (((s) >> 19) & 1)
#define	TD_MAXLEN_SET(l)  (((l)-1) << 21)
#define	TD_MAXLEN_GET(s)  ((((s) >> 21) + 1) & 0x7ff)
#define	TD_MAXLEN_MASK	0xffe00000


#define LP_TER 	    0x00000001
#define LP_QH	    0x00000002
#define LP_VF_D     0x00000004

#define LP_TER_SET(lp)	(lp |= LP_TER)
#define LP_QH_SET(lp)   (lp |= LP_QH )
#define LP_TD_SET(lp)   (lp &= (~LP_QH))
#define LP_VF_D_SET(lp) (lp |= LP_VF_D)
#define LP_VF_B_SET(lp) (lp |= (~LP_VF_D))

#define FL_CNT      1024
#define ISOC_FL_CNT 128
#define INTR_FL_CNT 128

typedef struct uhci_td {
	/* Hardware Part */
	volatile u32_t link_ptr;
	volatile u32_t status;
	volatile u32_t token;
	volatile u32_t buffer_ptr;
	/*               */
	u32_t phys;
	struct uhci_td *next;
	struct uhci_td *prev;
	struct uhci_td *last_td;
} uhci_td_t;

typedef struct uhci_qh {
	/* Hardware Part */
	volatile u32_t link_ptr;
	volatile u32_t elink_ptr;
	/*               */
	u32_t phys;
	usbd_dev_id_t devid;
	/* Horizontal next and previous */
	struct uhci_qh *h_lnext;
	struct uhci_qh *v_lnext;
	struct uhci_qh *last_qh;
	/* Next element vertical direction */
	struct uhci_td *v_tdstart;
	struct uhci_td *v_tdlast;
} uhci_qh_t;

#endif				
 
