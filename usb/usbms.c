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
 * 
 * Simple USB HID mouse driver that works on Boot protocol as per HID 
 * specification version 1.11 (27/6/2001) 
 */

/* NOTE: You are better off looking into usbkbd.c ,both are of similar
 * nature.
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
#define DPRINT_PREFIX "\nusbms: "

_PROTOTYPE(void ms_probe, (message m2ms));
_PROTOTYPE(void ms_irq, (void));
_PROTOTYPE(void ms_device_dettached, ( void )); 

usbd_dev_id_t ms_device;

char dat[10] = 
{    0, 0,
     0, 0,
     0, 0,
     0, 0,
     0, 0
};

int main(void) 
{
	U32_t self_proc;
 	message m2ms;
	int r;

	system_hz = sys_hz();
	r = ds_retrieve_u32("usbms",&self_proc);
	if (OK != r) {
		printf("ds_retrieve_32: failed");
		return EXIT_FAILURE;
	}
	
	if(OK != usbdi_init()) 
  	   usbdi_fatal_abort("usbms","failed to register");
	if(OK != usbdi_register_driver())
	   usbdi_fatal_abort("usbms","failed to register");
		
	while (TRUE) {
		if ((r = receive(ANY, &m2ms)) != OK)
			panic("uhci-hcd:", "receive failed", r);
		switch (m2ms.m_source) {
			case RS_PROC_NR:
				notify(m2ms.m_source);
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
					 DPRINTF(0, ("default %d ", m2ms.m_type));
					 goto usbd2usbdi_msg;
		}
		continue;
		
 usbd2usbdi_msg:
		switch (m2ms.m_type) {		
			case USBD2USBDI_DEVICE_DISCONNECT:
			  /* Message info from usbd
			   * 
			   * m2kbd.m2_l1 : device id
			   * this a valid case if this driver 
			   * handle multiple devices  
			   */
			  ms_device_dettached();
			  break;
			case USB_INTERRUPT_REQ_STS:
			  if (OK == m2ms.m2_i1)
				  ms_irq();
			  break;
			case USBD2USBDI_DD_PROBE:
			  ms_probe(m2ms);
			  break;
			case USBD2ALL_SIGTERM:
			  DPRINTF(1,("SIGTERM received from usbd , driver unstable"));
			  break;
			default:
			  DPRINTF(0, ("unknown type %d from source %d", m2ms.m_type, 
						  m2ms.m_source));
		}
	}
 aborted:
	return (OK);
}
	
void ms_probe(message m2ms)
{
	usb_interface_descriptor_t idesc;
	usb_endpoint_descriptor_t edesc;
	usb_config_descriptor_t cdesc;
	message reply;
	int r;
	
	DPRINTF(0,("inside ms_probe"));
	
	r = usbdi_get_device_cdesc(m2ms.m2_l1,&cdesc,1);
	if (OK != r) {
		DPRINTF(1, ("failed to GET_UDESC_CONFIG: %d",r));
		return;
	}
	
	r = usbdi_get_device_idesc(m2ms.m2_l1,&idesc,1,0);
    	if (OK != r) {
		DPRINTF(1, ("failed to GET_UDESC_INTERFACE: %d",r));
		return;
	}
	
	if(3 != idesc.bInterfaceClass || 1 != idesc.bInterfaceSubClass || 
	   2 != idesc.bInterfaceProtocol)  { 
       		usbdi_probe_status(m2ms.m2_l1,DD_DEV_REJECT);
        	return;
	}
    
        ms_device = m2ms.m2_l1;
    	/* Inform usbd driver can claim the device */
   	usbdi_probe_status(ms_device,DD_DEV_ACCEPT);
    	DPRINTF(1, ("USB HID mouse found"));
    
    	r = usbdi_get_device_edesc(ms_device,&edesc,1,0,0);
	if (OK != r) {
		DPRINTF(1, ("failed to GET_UDESC_ENDPOINT: %d",r));
		return;
	}
  
    	r = usbdi_set_config(ms_device,1);
   	if (OK != r) {
		DPRINTF(1, ("failed to SET_CONFIG: %d",r));
		return;
    	}
   	 DPRINTF(1,("setting boot protocol"));
	/* Set boot protocol */
  	usbdi_set_protocol(ms_device, 0,0);
    	usbdi_set_idle(ms_device,0,0,0);
    	usbdi_interrupt_req(ms_device,edesc.bEndpointAddress,dat);
}

void ms_irq( void )
{
   int i = 0;
   char buf[10];
   DPRINTF(1, ("irq running"));
   memcpy(buf,dat,8);
   if (buf[0] & 1 << 0)
	   printf("\nRight button clicked");
   if (buf[0] & 1 << 1)
	   printf("\nLeft button clicked");
   if (buf[0] & 1 << 2)
	   printf("\nMiddle button clicked");	
		
   printf("\n (x: %d,y: %d)",buf[1],buf[2]);	   
   DPRINTF(1, ("irq done"));
}

void ms_device_dettached( void ) 
{
	/* Handle detached case */
	DPRINTF(1,("HID USB mouse detached"));
}
