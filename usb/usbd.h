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
 * Janunary 2010
 * 
 * (C) Copyright 2009,2010 Althaf K Backer <althafkbacker@gmail.com> 
 * (C) Copyright 1999 Linus Torvalds <torvalds@transmeta.com>
 *     for the pipe concept implementation 
 * 
 * USB driver data structures (USBD)
 */
#ifndef _USBD_H_
#define _USBD_H_

#include "usb.h"
#include "usbdmem.h"
#include "../drivers.h"
#include <sys/mman.h>

#undef  DPRINT_PREFIX
#define DPRINT_PREFIX "\nusbd: "
#define USBD_MSLEEP(delay) usleep(delay*1000)

typedef phys_bytes usbd_dev_id_t;

typedef struct usbd_hc {
	u32_t proc;		
	struct usbd_page device_buffer;
}	usbd_hc_t;

typedef struct usbd_bus {
	int usbrev;
	struct usbd_hc hc; /* Host controller details for ipc */
	unsigned char devmap[16]; /* 0-127 bit maps for device */
	struct usbd_device *dev_list_start; /* device part of this bus/controller */
	struct usbd_device *root_hub; /* Root hub of host controller */
	struct usbd_bus *next_bus; /* purpose of internal list within USBD */
} usbd_bus_t;

typedef struct usbd_port {
	u16_t status;
	u8_t portno;
} usbd_port_t;

typedef struct usbd_device {
	unsigned int devnum;    /* Device number on USB bus    */
	struct usbd_bus *bus; 
	u32_t speed;		/* low/full */
	struct usbd_port port;	/* upstream hub port, or 0 	   */
	u32_t dd_procnr;	/* device driver of the device */
	usb_device_descriptor_t ddesc; /* Device Descriptor    */
	struct usbd_config_descriptor *usbd_cdesc; /* Configuration Descriptor */
	usb_config_descriptor_t *active_cdesc; 
	u8_t active_ifnum;
	usb_device_request_t dr; /* Device request associated with device  */
	struct usbd_device_driver *dd;
	struct usbd_device *next_dev;	/* device list */
} usbd_device_t;

typedef struct usbd_config_descriptor {
	u8_t index; /*REM:*/
	usb_config_descriptor_t *cdesc;
	struct usbd_interface_descriptor *usbd_idesc;
	struct usbd_config_descriptor *next;
} usbd_Config_descriptor_t;

typedef struct usbd_interface_descriptor {
	u8_t index; /*REM:*/
	usb_interface_descriptor_t *idesc;
	struct usbd_endpoint_descriptor *usbd_edesc;
	struct usbd_interface_descriptor *next;
} usbd_Interface_descriptor_t;

typedef struct usbd_endpoint_descriptor {
	u8_t index; /*REM:*/
	usb_endpoint_descriptor_t *edesc;
	struct usbd_endpoint_descriptor *next;
} usbd_Endpoint_descriptor_t;

typedef struct usbd_device_driver {
	u32_t dd_procnr;
	u16_t idVendor;
	u16_t idProduct;
	struct usbd_device *ddevice;
	struct usbd_device_driver *next;
} usbd_device_driver_t;

/*TO DO: Reorder the constants*/

#define USBD_IPC_BASE 0xE00
/* USBD -> HCD IPC  */
#define  USBD2HC_CONTROL_REQ       USBD_IPC_BASE+0x01
#define  USBD2HC_INTERRUPT_REQ     USBD_IPC_BASE+0x02
#define  USBD2HC_BULK_REQ  	   USBD_IPC_BASE+0x03	/* TODO */
#define  USBD2HC_ISOC_REQ          USBD_IPC_BASE+0x04	/* TODO */
#define  USBD2HC_HC_REGISTERED     USBD_IPC_BASE+0x05
#define  USBD2HC_HC_REGISTER_FAIL  USBD_IPC_BASE+0x06
#define  USBD2ALL_SIGTERM      	   USBD_IPC_BASE+0x07
#define  USBD2HC_CANCEL_XFER       USBD_IPC_BASE+0x08
#define 	 XFER_INTERRUPT    0x01
#define		 XFER_BULK	   0x02
#define 	 XFER_ISOC	   0x03
#define 	 XFER_ALL	   0x04

/* HC -> USBD IPC  */
#define  HC2USBD_REGISTER	   	USBD_IPC_BASE+0x09
#define  HC2USBD_NEW_DEVICE_FOUND  	USBD_IPC_BASE+0x0A
#define  HC2USBD_DEVICE_DISCONNECTED  	USBD_IPC_BASE+0x0B
#define  HC2USBD_PING 			USBD_IPC_BASE+0x0C
#define  HC2USBD_HC_DERGISTER          	USBD_IPC_BASE+0x0D
#define  USB_INTERRUPT_REQ_STS	       	USBD_IPC_BASE+0x0E
#define  USB_BULK_REQ_STS		0x00 /* TODO */
#define  USB_ISOC_REQ_STS		0x00 /* TODO */

/* USBD -> USBDI IPC */
#define  USBD2USBDI_DD_REGISTERED      USBD_IPC_BASE+0x10
#define  USBD2USBDI_DD_REGISTER_FAIL   USBD_IPC_BASE+0x11
#define  USBD2USBDI_DD_PROBE	       USBD_IPC_BASE+0x12
#define  USBD2USBDI_DEVICE_DISCONNECT  USBD_IPC_BASE+0x13

/* USBDI -> USBD IPC */
#define  USBDI2USBD_REGISTER_DD    	   USBD_IPC_BASE+0x14
#define  USBDI2USBD_DERGISTER_DD	   USBD_IPC_BASE+0x15
#define  USBDI2USBD_DD_PROBE_STS	   USBD_IPC_BASE+0x16
#define  	    DD_DEV_ACCEPT     0x01
#define  	    DD_DEV_REJECT     0x02
#define  USBDI2USBD_REQ  		   USBD_IPC_BASE+0x17
#define		    GET_UDESC_DEVICE    0x01  
#define  	    GET_UDESC_CONFIG    0x02	 
#define  	    GET_UDESC_INTERFACE 0x03  
#define 	    GET_UDESC_ENDPOINT	0x04
#define 	    GET_STATUS 	        0x05 
#define 	    SET_CONFIG		0x06
#define 	    SET_INTERFACE	0x07
#define		    SET_IDLE		0x08
#define		    GET_IDLE		0x09
#define 	    SET_PROTOCOL	0x0A
#define 	    GET_PROTOCOL	0x0B
#define 	    SET_REPORT		0x0C
#define 	    GET_REPORT		0x0D
#define USBDI2USBD_INTERRUPT_REQ 	 USBD_IPC_BASE+0x18
#define USBDI2USBD_BULK_REQ	         USBD_IPC_BASE+0x19 /* TODO */
#define USBDI2USBD_ISOC_REQ		 USBD_IPC_BASE+0x20 /* TODO */

/* A USB pipe  basically consists of the following information:
 *  - device number (7 bits)
 *  - endpoint number (4 bits)
 *  - current Data0/1 state (1 bit)
 *  - direction (1 bit)
 *  - speed (1 bit)
 *  - max packet size (2 bits: 8, 16, 32 or 64)
 *  - pipe type (2 bits: control, interrupt, bulk, isochronous)
 *    That's 18 bits.
 *    
 *  The encoding is:
 *
 *  - device:		bits 8-14
 *  - endpoint:		bits 15-18
 *  - Data0/1:		bit 19
 *  - direction:	bit 7		(0 = Host-to-Device, 1 = Device-to-Host)
 *  - speed:		bit 26		(0 = High, 1 = Low Speed)
 *  - max size:		bits 0-1	(00 = 8, 01 = 16, 10 = 32, 11 = 64)
 *  - pipe type:	bits 30-31	(00 = isochronous, 01 = interrupt, 10 = control, 11 = bulk)
 */

#define usb_maxpacket(pipe)		(8 << ((pipe) & 3))
#define usb_packetid(pipe)		(((pipe) & 0x80) ? USB_PID_IN : USB_PID_OUT)
#define usb_pipedevice(pipe)		(((pipe) >> 8) & 0x7f)
#define usb_pipeendpoint(pipe)		(((pipe) >> 15) & 0xf)
#define usb_pipedata(pipe)		(((pipe) >> 19) & 1)
#define usb_pipeout(pipe)	    	(((pipe) & USB_DIR_IN) == 0)
#define usb_pipeslow(pipe)		(((pipe) >> 26) & 1)
#define usb_pipetype(pipe)		(((pipe) >> 30) & 3)
#define usb_pipeint(pipe)		(usb_pipetype((pipe)) == 1)
#define usb_pipecontrol(pipe)		(usb_pipetype((pipe)) == 2)
#define usb_pipe_endpdev(pipe)		(((pipe) >> 8) & 0x7ff)

PRIVATE u32_t Create_pipe(usbd_device_t *dev, u32_t endpoint)
{
	u8_t pktsize = 0;
	switch (dev->ddesc.bMaxPacketSize) {
		    case 8:  pktsize = 0;  break;
			case 16: pktsize = 1;  break;
			case 32: pktsize = 2;  break;
			case 64: pktsize = 3;  break;
			default: pktsize = 0;
	}
	return ((dev->devnum << 8) | (endpoint << 15) | (dev->speed << 26) | pktsize);
}

PRIVATE u32_t Default_pipe(usbd_device_t *dev)
{
	return (dev->speed << 26);
}

#define usb_sndctrlpipe(dev,endpoint) ((2 << 30) | Create_pipe(dev,endpoint))
#define usb_rcvctrlpipe(dev,endpoint) ((2 << 30) | Create_pipe(dev,endpoint) | USB_DIR_IN )
#define usb_sndintpipe(dev,endpoint)  ((1 << 30) | Create_pipe(dev,endpoint))
#define usb_rcvintpipe(dev,endpoint)  ((1 << 30) | Create_pipe(dev,endpoint) | USB_DIR_IN )
#define usb_snddefctrl(dev) ((2 << 30) | Default_pipe(dev))
#define usb_rcvdefctrl(dev) ((2 << 30) | Default_pipe(dev) | USB_DIR_IN )

#endif
