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
 * February 2010
 * 
 * (C) Copyright 2009,2010 Althaf K Backer <althafkbacker@gmail.com> 
 * (C) Copyright  1998 The NetBSD Foundation, Inc.
 * 	   for the usb_2_set1[] mapping
 * 
 * Simple USB HID keyboard driver that works on Boot protocol as per HID 
 * specification version 1.11 (27/6/2001) 
 */

/*NOTE: This is not exactly a driver rather just an implementation to show
 *      that the usb stack work from top to bottom , what you see here is
 *      just a dummy driver that reads what ever key is being pressed 
 *      and display them along with scan codes 
 * 
 *      It would be nice if some one try to integrate this with tty 
 */
  
#include "../drivers.h"
#include "../libdriver/driver.h"

#include <minix/ds.h>
#include <minix/vm.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <ibm/pci.h>

#include <sys/mman.h> 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usbd.h"
#include "usbdi.h"

#undef  DPRINT_PREFIX
#define DPRINT_PREFIX "\nusbkbd: "

/* Testing */
#define SET_BOOT_P 1
#define SET_REPORT_P 0

usbd_dev_id_t kbd_device;

char numon  = 1 << 0;
char capson = 1 << 1;
char scrlon = 1 << 2;
char allon  = 1 << 0 | 1 << 1 | 1 << 2;
char alloff = 0;
char cson = 1 << 1 | 1 << 2;

char dat[10] = 
{	 0, 0,
	 0, 0,
	 0, 0,
	 0, 0,
	 0, 0
};

char buf[2] = {0, 0};
#define NN 0			/* no translation */
short keymap[256] = {
	NN, NN, NN, NN, 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',

	'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z', '1', '2',

	'3', '4', '5', '6', '7', '8', '9', '0',
	'\n', '\e', '\b', '\t', ' ', '-', '=', '[',

	']', '\\', NN, ';', '\'', '`', ',', '.',
	'/', NN, F1, F2, F3, F4, F5, F6,
	F7, F8, F9, F10, F11, F12, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, '*', '-', '+',
	NN, END, DOWN, PGUP, LEFT, NN, RIGHT, HOME,
	UP, PGDN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN,

	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
	NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN, NN,
};

/*
 * Translate USB keycodes to US keyboard XT scancodes.
 * Scancodes >= 0x80 represent EXTENDED keycodes.
 *
 * See http://www.microsoft.com/whdc/device/input/Scancode.mspx
 */
const u8_t usb_2_set1[256] = {
      NN,   NN,   NN,   NN, 0x1e, 0x30, 0x2e, 0x20, /* 00 - 07 */
    0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, /* 08 - 0f */
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, /* 10 - 17 */
    0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, /* 18 - 1f */
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, /* 20 - 27 */
    0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a, /* 28 - 2f */
    0x1b, 0x2b, 0x2b, 0x27, 0x28, 0x29, 0x33, 0x34, /* 30 - 37 */
    0x35, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, /* 38 - 3f */
    0x41, 0x42, 0x43, 0x44, 0x57, 0x58, 0xaa, 0x46, /* 40 - 47 */
    0x7f, 0xd2, 0xc7, 0xc9, 0xd3, 0xcf, 0xd1, 0xcd, /* 48 - 4f */
    0xcb, 0xd0, 0xc8, 0x45, 0xb5, 0x37, 0x4a, 0x4e, /* 50 - 57 */
    0x9c, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, /* 58 - 5f */
    0x48, 0x49, 0x52, 0x53, 0x56, 0xdd, 0x84, 0x59, /* 60 - 67 */
    0x5d, 0x5e, 0x5f,   NN,   NN,   NN,   NN,   NN, /* 68 - 6f */
      NN,   NN,   NN,   NN, 0x97,   NN, 0x93, 0x95, /* 70 - 77 */
    0x91, 0x92, 0x94, 0x9a, 0x96, 0x98, 0x99, 0xa0, /* 78 - 7f */
    0xb0, 0xae,   NN,   NN,   NN, 0x7e,   NN, 0x73, /* 80 - 87 */
    0x70, 0x7d, 0x79, 0x7b, 0x5c,   NN,   NN,   NN, /* 88 - 8f */
      NN,   NN, 0x78, 0x77, 0x76,   NN,   NN,   NN, /* 90 - 97 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* 98 - 9f */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* a0 - a7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* a8 - af */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* b0 - b7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* b8 - bf */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* c0 - c7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* c8 - cf */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* d0 - d7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* d8 - df */
    0x1d, 0x2a, 0x38, 0xdb, 0x9d, 0x36, 0xb8, 0xdc, /* e0 - e7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* e8 - ef */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* f0 - f7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* f8 - ff */
};

_PROTOTYPE(void kbd_probe, (message m2kbd));
_PROTOTYPE(void kbd_device_dettached, (void));
_PROTOTYPE(void kbd_irq, (void));

int main(void) 
{
	U32_t self_proc;
 	message m2kbd;
	int r;

	system_hz = sys_hz();
	r = ds_retrieve_u32("usbkbd",&self_proc);
	if (OK != r) {
		printf("ds_retrieve_32: failed");
		return EXIT_FAILURE;
	}
	
	if(OK != usbdi_init()) 
  	   usbdi_fatal_abort("usbkbd","failed to register");
	if(OK != usbdi_register_driver())
	   usbdi_fatal_abort("usbkbd","failed to register");
		
	while (TRUE) {
		if ((r = receive(ANY, &m2kbd)) != OK)
			panic("uhci-hcd:", "receive failed", r);
		switch (m2kbd.m_source) {
			case RS_PROC_NR:
				notify(m2kbd.m_source);
				break;
			case PM_PROC_NR: {
						 sigset_t set;
						 if (getsigset(&set) != 0)
							 break;
						 if (sigismember(&set, SIGTERM)) {
							 usbdi_dereisgter_driver();
							 goto aborted;
						 }
					 }
					 break;
			default:
			DPRINTF(0, ("default %d ", m2kbd.m_type));
			goto usbd2usbdi_msg;
		}
		continue;
		
 usbd2usbdi_msg:
		switch (m2kbd.m_type) {		
			case USBD2USBDI_DD_PROBE:
				kbd_probe(m2kbd);
				break;
			case USBD2USBDI_DEVICE_DISCONNECT:
				/* Message info from usbd
				 *  
				 *  m2kbd.m2_l1 : device id
				 *  this a valid case if this driver 
				 *  handle multiple devices  
				 */
				kbd_device_dettached();
				break;
			case USB_INTERRUPT_REQ_STS:
				if (OK == m2kbd.m2_i1)
					kbd_irq();
				break;
			case USBD2ALL_SIGTERM:
				DPRINTF(1,("SIGTERM received from usbd , driver unstable"));
				break;
			default:
				DPRINTF(0, ("unknown type %d from source %d", 
							m2kbd.m_type, m2kbd.m_source));
		}
	}
 aborted:
	return (OK);
}
	
	
void kbd_probe(message m2kbd)
{
	usb_interface_descriptor_t idesc;
	usb_endpoint_descriptor_t edesc;
	usb_config_descriptor_t cdesc;
	message reply;
	int r;
	
	DPRINTF(0, ("inside kbd_probe"));
	
	r = usbdi_get_device_cdesc(m2kbd.m2_l1,&cdesc,1);
	if (OK != r) {
		printf("\nusbkbd: failed to GET_UDESC_CONFIG: %d",r);
		return;
	}
	
	r = usbdi_get_device_idesc(m2kbd.m2_l1,&idesc,1,0);
	if (OK != r) {
		printf("\nusbkbd: failed to GET_UDESC_INTERFACE: %d",r);
		return;
	}
	
	if(3 != idesc.bInterfaceClass || 1 != idesc.bInterfaceSubClass || 
			1 != idesc.bInterfaceProtocol)  { 
		usbdi_probe_status(m2kbd.m2_l1,DD_DEV_REJECT);
		return;
	}
	kbd_device = m2kbd.m2_l1;
	/* Inform usbd driver can claim the device */
	usbdi_probe_status(kbd_device,DD_DEV_ACCEPT);
	printf("\nusbkbd: USB HID Keyboard found");
	r = usbdi_get_device_edesc(kbd_device,&edesc,1,0,0);
	if (OK != r) {
		printf("\nusbkbd: failed to GET_UDESC_ENDPOINT: %d",r);
		return;
	}
	r = usbdi_set_config(kbd_device,1);
	if (OK != r) {
		printf("\nusbkbd: failed to SET_CONFIG: %d",r);
		return;
	}

#if SET_BOOT_P
	printf("\nusbdkbd: setting boot protocol");
	/* Set boot protocol */
	r = usbdi_set_protocol(kbd_device, 0, 0);
	if (OK != r)
		return;
		
/*  r = usbdi_get_protocol(kbd_device, idesc.bInterfaceNumber, buf);
	if (OK != r) {
		printf("\n get protocols");
	}
	printf("\n get protocol 0x%x 0x%x",buf[0],buf[1]); 
*/   
#endif 

#if SET_REPORT_P
	printf("\nusbdkbd: setting report protocol");
	/* Set boot protocol */
	r = usbdi_set_protocol(kbd_device, 1, idesc.bInterfaceNumber);
	if (OK != r)
		return;
	r = usbdi_get_report(kbd_device, RT_IN, 0, 8, idesc.bInterfaceNumber, dat);
	if (OK != r) {
		printf("\n failed");
		return;
	}
	kbd_irq();
#endif 

#if SET_BOOT_P
	r = usbdi_set_idle(kbd_device, 0, 0, idesc.bInterfaceNumber);
	if (OK != r)
		return;
/*	r = usbdi_get_idle(kbd_device, 0, idesc.bInterfaceNumber, buf);
	if (OK != r)
		return;
	printf("\n get idle 0x%x 0x%x",buf[0],buf[1]);
*/
    /* Just for fun show up some light show :D */
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&numon);   USBD_MSLEEP(2);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&capson);  USBD_MSLEEP(50);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&scrlon);  USBD_MSLEEP(2);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&alloff);  USBD_MSLEEP(100);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&numon);   USBD_MSLEEP(2);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&alloff);  USBD_MSLEEP(10);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&scrlon);  USBD_MSLEEP(60);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&capson);  USBD_MSLEEP(2);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&numon);   USBD_MSLEEP(10);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&alloff);  USBD_MSLEEP(10);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&allon);   USBD_MSLEEP(2);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&alloff);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&numon);
#if 0
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&capson);
	USBD_MSLEEP(100);
	usbdi_set_report(kbd_device,RT_OUT,0,1,idesc.bInterfaceNumber,&cson); 
#endif     
	/* Start the interrupt polling for keyboard events */ 
	usbdi_interrupt_req(kbd_device,edesc.bEndpointAddress,dat);
#endif 

}

void kbd_irq( void )
{
   int i = 0;
   char buf[10];
   DPRINTF(1, ("usbkbd: irq running"));
   memcpy(buf,dat,8);
   for (i = 0;i < 8;i++) 
	   printf("\n usb scan code[%d] = 0x%02x scan code set 1 0x%02x %c",i,buf[i],
			   usb_2_set1[buf[i]],keymap[buf[i]]);	
   DPRINTF(1, ("usbkbd: irq done"));
}

void kbd_device_dettached( void ) 
{
	/* Handle dettached case */
	DPRINTF(1,("HID USB Keyboard detached"));
}
#if 0
void kbd_device_info(message m2kbd)
{
	message reply;
	int r;
	usb_device_descriptor_t ddesc; 
	usb_config_descriptor_t cdesc;
	usb_interface_descriptor_t idesc;
	usb_endpoint_descriptor_t edesc;
	printf("\nusbkbd: kbd_probe(message m2kbd)");
	kbd_device = m2kbd.m2_l1;
	
	usbdi_probe_status(kbd_device,DD_DEV_ACCEPT);

	r = usbdi_get_device_ddesc(kbd_device,&ddesc);
	if (r != OK) {
		printf("\nusbkbd: failed to GET_UDESC: %d",r);
		return;
	}
	printf("\nDEVICE DESCRIPTOR");
    printf("\nbLength :%d",ddesc.bLength); 
    printf("\nbDescriptorType :%x",ddesc.bDescriptorType);
	printf("\nbcdUSB :%x",ddesc.bcdUSB);
	printf("\nbDeviceClass :%x",ddesc.bDeviceClass);
	   printf("\nbDeviceSubClass :%x",ddesc.bDeviceSubClass);
	   printf("\nbDeviceProtocol :%x",ddesc.bDeviceProtocol);
	   printf("\nbMaxPacketSize :%d",ddesc.bMaxPacketSize);
	   printf("\nidVendor :%04x",ddesc.idVendor);
	   printf("\nidProduct:%04x",ddesc.idProduct);
	   printf("\nbcdDevice :%x",ddesc.bcdDevice);
	   printf("\niManufacturer :%d",ddesc.iManufacturer);
	   printf("\niProduct :%d",ddesc.iProduct);
	   printf("\niSerial Number :%d",ddesc.iSerialNumber);
	 
   r = usbdi_get_device_cdesc(kbd_device,&cdesc,1);
  	if (r != OK) {
		printf("\nusbkbd: failed to GET_UDESC_CONFIG: %d",r);
		return;
	}
	
		printf("\nCONFIGURATION DESCRIPTOR");
		printf("\n bLength : %d", cdesc.bLength);
		printf("\n bDescriptorType : %d", cdesc.bDescriptorType);
		printf("\n wTotalLength : %d", cdesc.wTotalLength);
		printf("\n bNumInterface : %d", cdesc.bNumInterface);
		printf("\n bConfigurationValue : %d",cdesc.bConfigurationValue);
		printf("\n iConfiguration : %d", cdesc.iConfiguration);
		printf("\n bmAttributes 0x%02x", cdesc.bmAttributes);
		printf("\n bMaxPower : %dmA", cdesc.bMaxPower * 2);
	
   r = usbdi_get_device_idesc(kbd_device,&idesc,1,0);
   if (r != OK) {
		printf("\nusbkbd: failed to GET_UDESC_INTERFACE: %d",r);
		return;
	}
			printf("\n INTERFACE DESCRIPTOR");
			printf("\n  bLength : %d", idesc.bLength);
			printf("\n  bDescriptorType : %d",idesc.bDescriptorType);
			printf("\n  bInterfaceNumber : %d",idesc.bInterfaceNumber);
			printf("\n  bAlternateSetting : %d",idesc.bAlternateSetting);
			printf("\n  bNumEndpoints : %d", idesc.bNumEndpoints);
			printf("\n  bInterfaceClass) : %d",idesc.bInterfaceClass);
			printf("\n  bInterfaceSubClass : %d",idesc.bInterfaceSubClass);
			printf("\n  bInterfaceProtocol : %d",idesc.bInterfaceProtocol);
			printf("\n  iInterface : %d", idesc.iInterface);
   r = usbdi_get_device_edesc(kbd_device,&edesc,1,0,0);
   if (r != OK) {
		printf("\nusbkbd: failed to GET_UDESC_INTERFACE: %d",r);
		return;
   }
   				printf("\n  ENDPOINT DESCRIPTOR");
				printf("\n   bLength : %d", edesc.bLength);
				printf("\n   bDescriptorType : %d",edesc.bDescriptorType);
				printf("\n   bEndpointAddress : 0x%02x",edesc.bEndpointAddress);
				printf("\n   bmAttributes :%d",edesc.bmAttributes);
				printf("\n   wMaxPacketSize : %d",edesc.wMaxPacketSize);
				printf("\n   bInterval  : %d",edesc.bInterval);
				
  
}
#endif
