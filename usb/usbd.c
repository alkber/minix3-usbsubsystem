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
 * 
 * USBD which provides the abstraction to layer above and below.
 */

#include "../drivers.h"
#include "../libdriver/driver.h"

#include <minix/ds.h>
#include <minix/vm.h>
#include <minix/sysutil.h>

#include <sys/mman.h>
#include <sys/shm.h>

#include "usbd.h"
#include "usbdmem.h"

#undef  DPRINT_PREFIX
#define DPRINT_PREFIX "\nusbd: "
#define SHOW_ALL_DESC 0 
#define USBDMSG(arg) { printf(DPRINT_PREFIX); printf arg;}

struct usbd_t {
	u32_t usbd_proc;
	message m2usbd;
	message m2hc;
	/* Page at where hcds would be allocated */
	struct usbd_page hc_pool;
	/* Host controller list*/
	usbd_bus_t *hc_list_start;
	/* Device driver list  */
	usbd_device_driver_t *dd_list_start;
};

struct usbd_t usbd;

_PROTOTYPE(void usbd_req_handler, (message));
_PROTOTYPE(void usbd_register_hc, (message));
_PROTOTYPE(void usbd_probe_dd, (usbd_device_t *));
_PROTOTYPE(void usbd_notify, (int, endpoint_t));
_PROTOTYPE(void usbd_init, (void));
_PROTOTYPE(void usbd_hook_dd_device, (message));
_PROTOTYPE(void usbd_fatal_abort, (char *));
_PROTOTYPE(phys_bytes usbd_check_buffer, (void *,phys_bytes *));
_PROTOTYPE(void usbd_exit, (void));
_PROTOTYPE(void usbd_event_new_device, (message));
_PROTOTYPE(void usbd_display_usbdcdesc, (usbd_device_t *, 
					   usbd_Config_descriptor_t *));
_PROTOTYPE(void usbd_deregister_hc, (message));
_PROTOTYPE(void usbd_dealloc_usbdcdesc, (usbd_Config_descriptor_t *));
_PROTOTYPE(void usbd_dealloc_hc_devices, (usbd_bus_t *));
_PROTOTYPE(void usbd_device_detached, (message));
_PROTOTYPE(usbd_device_t *usbd_find_dev, (usbd_device_t *device));
_PROTOTYPE(usbd_device_t *usbd_alloc_device, (struct usbd_hc *));
_PROTOTYPE(usbd_device_driver_t *usbd_find_dd, (endpoint_t));
_PROTOTYPE(usbd_bus_t *usbd_find_hc,(endpoint_t, usbd_bus_t *));
_PROTOTYPE(int usbd_valid_descriptor, (u8_t *, int, u8_t, u8_t, int *));
_PROTOTYPE(int usbd_set_address, (usbd_device_t *));
_PROTOTYPE(void usbd_register_dd,(message));
_PROTOTYPE(int usbd_msg_client, (endpoint_t , message *));
_PROTOTYPE(int usbd_get_device_descriptor, (usbd_device_t *));
_PROTOTYPE(int usbd_get_descriptor, (usbd_device_t *, u16_t, u8_t, void *, int));
_PROTOTYPE(int usbd_get_configuration, (usbd_device_t *));
_PROTOTYPE(int usbd_extract_interface, (usbd_device_t *,u8_t *,	
			usbd_Config_descriptor_t *,int));
_PROTOTYPE(int usbd_deregister_dd, (message));
_PROTOTYPE(int usbd_append_interface, (usbd_device_t *, u8_t, u8_t *, 
 		   usbd_Config_descriptor_t *, usbd_Interface_descriptor_t **, int *, int));
_PROTOTYPE(int usbd_append_endpoint, (usbd_device_t *,u8_t , u8_t * , 
		    usbd_Interface_descriptor_t *, usbd_Endpoint_descriptor_t ** ,int *, int));
_PROTOTYPE(int usbd_append_config, (usbd_device_t *, u8_t , u8_t * , 
		    usbd_Config_descriptor_t **, int));

int main(void)
{
	int r;
	

	usbd_init();
	/*
	 * TO DO: asynchronous send and receive ,which could be much better choice
	 * when we have multiple clients (DDs and HCDs) communicating with USBD,
	 * in later stages possible cases for failures may be do this.
	 */
	while (TRUE) {
		  if ((r = receive(ANY, &usbd.m2usbd)) != OK)
			 panic("usbd:", "receive failed", r);

		  switch (usbd.m2usbd.m_source) {
		  case RS_PROC_NR:
			   notify(usbd.m2usbd.m_source);
		  break;
		  case PM_PROC_NR: {
			   sigset_t set;
			   if (getsigset(&set) != 0) 
				   break;
			   if (sigismember(&set, SIGTERM)) {
				   usbd_exit();
				   goto exit;
		        }
		  }
		  break;
		  default:
			   DPRINTF(0, ("default "));
			   goto m2usbd_other;		
		}
		continue;
 m2usbd_other:
		switch (usbd.m2usbd.m_type) {
		case HC2USBD_REGISTER:
			 usbd_register_hc(usbd.m2usbd);
		break;	
		case HC2USBD_HC_DERGISTER:
			 usbd_deregister_hc(usbd.m2usbd);
		break;	
		case HC2USBD_NEW_DEVICE_FOUND:
			 usbd_event_new_device(usbd.m2usbd);
		break;	
		case USBDI2USBD_REGISTER_DD:
			 usbd_register_dd(usbd.m2usbd);
		break;	
		case USBDI2USBD_DERGISTER_DD:
			 usbd_deregister_dd(usbd.m2usbd);
		break;	
		case USBDI2USBD_DD_PROBE_STS:
			 if (usbd.m2usbd.m2_i1 == DD_DEV_ACCEPT)
				 usbd_hook_dd_device(usbd.m2usbd);
		break;	
		case USBDI2USBD_REQ:
			 usbd_req_handler(usbd.m2usbd);
		break;	 	
		case HC2USBD_DEVICE_DISCONNECTED:
			 usbd_device_detached(usbd.m2usbd);
		break;
		case HC2USBD_PING:
	    break;
		case USB_INTERRUPT_REQ_STS: {
			 usbd_device_t *dev = (usbd_device_t *) usbd.m2usbd.m2_l1;
			 usbd_device_driver_t *dd;
			 
			 dev = usbd_find_dev(dev);
			 if (NULL == dev) {
				 DPRINTF(1,("no such device"));
				 break;
			 }
			 
			 dd = usbd_find_dd(dev->dd_procnr);
			 if (NULL == dd) {
				 DPRINTF(1,("no such device driver"));
				 break;
			 }
			 /* This message we just received have all the information
			  * meant for the device driver 
			  */
			 usbd_msg_client(dev->dd_procnr, &usbd.m2usbd);
		} 
		break;
		default:
			DPRINTF(1, ("Unknown message type %d from source %d\n",
				        usbd.m2usbd.m_type, usbd.m2usbd.m_source));
		}
	}
	
 exit:
	return (OK);
}

void usbd_init(void)
{
	int r = 0;

	usbd.hc_list_start = NULL;
	usbd.hc_pool.size = 0;	/* Acts as a flag */
	usbd.dd_list_start = NULL;

	r = ds_retrieve_u32("usbd", &usbd.usbd_proc);

	if (OK != r) {
		usbd_fatal_abort("usbd_init failed on ds_retreive_u32");
		return;
	}

	r = usbd_init_page(sizeof(usbd_bus_t), A4K, &usbd.hc_pool);
	if (OK != r) {
		usbd_fatal_abort("usbd_init failed with NOMEM");
		return;
	}
	
	DPRINTF(1, ("started\n"));
	DPRINTF(1, ("USB Stack %s",USB_STACK_VERSION));
	DPRINTF(1, ("Copyright (C) 2010 Althaf K Backer <althafkbacker@gmail.com>"));
	DPRINTF(1, ("License GPLv3+: GNU GPL version 3 or later"));
	DPRINTF(1, ("This is free software: you are free to change"
				" and redistribute"));
}

void usbd_exit(void)
{
	usbd_device_driver_t *current;
	message m2all;
	m2all.m_type = USBD2ALL_SIGTERM;
	/* Send all the registered host controllers and drivers SIGTERM wrt usbd 
	 * We have to choose a nonblocking send , it has been observed that 
	 * there is a deadlock when device drivers or hcds try to reply with blocking
	 * code, when minix send SIGTREM ie during shutdown
	 */
	while (usbd.hc_list_start) {
		   usbd_dealloc_hc_devices(usbd.hc_list_start);
		   sendnb(usbd.hc_list_start->hc.proc, &m2all);
		   usbd.hc_list_start = usbd.hc_list_start->next_bus;
	}
	/* */ 
	while (usbd.dd_list_start) {
		   sendnb(usbd.dd_list_start->dd_procnr, &m2all);
		   current = usbd.dd_list_start;
		   usbd.dd_list_start = usbd.dd_list_start->next;
		   free(current);
	}

	/* Check if the page were allocated for hc list */
	if (usbd.hc_pool.size)
		usbd_free_page(&usbd.hc_pool);

	DPRINTF(1, ("terminated"));
}

/*
 * Register a host controller driver all the parameters are passed in the 
 * message.
 * 
 * Following members are a used 
 * 
 * m_source : would be used for IPC and is used as unique id for hcd. 
 * m1_i1    : amount of memory that has be be allocated for the devices
 * 	      part of the bus/controller ie when a new device is found 
 * 	      it is allocate in the respective page provided for this hcd
 * 	      possible values is A4K and A64K.
 */ 			   
void usbd_register_hc(message mfrm_hc)
{
	usbd_bus_t *new_bus;
	message m2hc;
	int r;

	new_bus = (usbd_bus_t *)usbd_const_alloc(&usbd.hc_pool);
	if (!new_bus) {
		usbd_notify(USBD2HC_HC_REGISTER_FAIL, mfrm_hc.m_source);
		return;
	}

	new_bus->hc.proc = mfrm_hc.m_source;
	new_bus->dev_list_start = NULL;
	
	memset(&new_bus->devmap, 0, 128);

	/* Allocate memory pool for devices related this particular hc driver */
	r = usbd_init_page(sizeof(usbd_device_t), mfrm_hc.m1_i1, 
						&new_bus->hc.device_buffer);
	if (OK != r) {
		usbd_notify(USBD2HC_HC_REGISTER_FAIL, mfrm_hc.m_source);
		usbd_const_dealloc(&usbd.hc_pool, new_bus);
		DPRINTF(1,("couldn't allocate device pool for hc %d", mfrm_hc.m_source));
		return;
	}

	DPRINTF(0,("hc device buffer base %x",new_bus->hc.device_buffer.vir_start));

	if (usbd.hc_list_start == NULL) {
		usbd.hc_list_start = new_bus;
		new_bus->next_bus = NULL;
		goto hc_registered;
	}

	new_bus->next_bus = usbd.hc_list_start;
	usbd.hc_list_start = new_bus;

 hc_registered:
	usbd_notify(USBD2HC_HC_REGISTERED, mfrm_hc.m_source);
	DPRINTF(1, ("Host controller driver %d registered", mfrm_hc.m_source));
}

/* Is is called and should be called when a hcd terminates ,
 *  
 * Following members are used 
 * 
 * m_source : which is used to find the hcd related data structure and 
 * 	      resources allocated for it.
 */
void usbd_deregister_hc(message mfrm_hc)
{
	usbd_bus_t *prev;
	usbd_bus_t *bus;

    prev = NULL;
	if (NULL == usbd.hc_list_start)
		return;
    /* Find the bus that hc is part of */
	bus = usbd_find_hc(mfrm_hc.m_source, prev);
	if (bus) {
		usbd_dealloc_hc_devices(bus);
		usbd_free_page(&bus->hc.device_buffer);
		/* update hc listing */
		if (usbd.hc_list_start == bus)
			usbd.hc_list_start = bus->next_bus;
		if (prev)
			prev->next_bus = bus->next_bus;

		usbd_const_dealloc(&usbd.hc_pool, bus);
		DPRINTF(1,("Host Controller driver %d unregistered", mfrm_hc.m_source));
		return;
	}
}

/* 
 * Following members are used
 * 
 * m_source : as a hcd send this notification of new device , we use the
 * 	      m_source to find that hcd and related device pool for allocation
 * m1_i1    : port number at which device is connected , well for time being 
 * 	      usb stack is limited port of root hub ,once we have hub driver 
 * 	      implemented this should change, port number is used to find 
 * 	      the device when hcd notify a device disconnect.
 * m1_i2    : speed of the device ,taken from the root hub PORTSC 
 */ 
void usbd_event_new_device(message mfrm_hc)
{
	usbd_bus_t    *hc_bus;
	usbd_device_t *new_device;
	usbd_device_t *debug_dev;
	message m2hc;
	unsigned int devnum = 0 , r;
 	/* Find the bus that hcd is part of */
	hc_bus = usbd_find_hc(mfrm_hc.m_source, NULL);
	if (NULL == hc_bus) {
		DPRINTF(1,("cannot find a registered host controller:device not claimed"));
		return;
	}
	
	/* Find the free bit position from bit map */
	devnum = next_free_bit(hc_bus->devmap);
	if (devnum >= 128) {
		DPRINTF(1, ("device not claimed, bus reached max devices"));
		return;
	}
	
	set_bit(hc_bus->devmap, devnum);

	new_device = usbd_alloc_device(&hc_bus->hc);
	if (NULL == new_device) {
		DPRINTF(1, ("failed to allocate device :ENOMEM"));
		return;
	}

	new_device->bus = hc_bus;
	new_device->bus->usbrev = 0;
	new_device->devnum = devnum + 1;
	new_device->port.portno = mfrm_hc.m1_i1;
	new_device->speed = mfrm_hc.m1_i2;
	new_device->usbd_cdesc = NULL;
	new_device->active_cdesc = NULL;
	new_device->dd = NULL;
		
	r = usbd_set_address(new_device);
	if (r != OK) {
		DPRINTF(1, ("failed to SET_ADDRESS: %d", r));
		goto device_err;
	}
    	/* Allow set address to settle */
	USBD_MSLEEP(10);

	r = usbd_get_device_descriptor(new_device);
	if (r != OK) {
		DPRINTF(1, ("failed to GET_DESCRIPTOR: %d", r));
		goto device_err;
	}

	r = usbd_get_configuration(new_device);
	if (r != OK) {
		DPRINTF(1, ("failed to GET_CONFIGURATION: %d", r));
		goto device_err;
	}
   
	/* all set, append the device */
	if (hc_bus->dev_list_start == NULL) {
		hc_bus->dev_list_start = new_device;
		new_device->next_dev = NULL;
	} else {
		new_device->next_dev = hc_bus->dev_list_start;
		hc_bus->dev_list_start = new_device;
	}
	
	DPRINTF(1, ("Bus:%d Port:%x Device:%x ID 0x%04x:0x%04x attached",
		    new_device->bus->usbrev,
		    new_device->port.portno,
		    new_device->devnum, new_device->ddesc.idVendor,
		    new_device->ddesc.idProduct));
#if SHOW_ALL_DESC
	/* Just for debug show up all the descriptors we read */	    
	usbd_display_usbdcdesc(new_device,new_device->usbd_cdesc);
#endif
	/* Search for a registered driver for this device, this is call is 
	 * asynchronous ,it  returns a soon as it is called, if some driver could 
	 * claim then usbd is notified in main(). 
	 */
    usbd_probe_dd(new_device);
    return; 
    
   /* control never reach here unless an error occur */
 device_err:
	DPRINTF(1, ("Bus:%d Port:%x Device:%x rejected", new_device->bus->usbrev,
				new_device->port.portno, new_device->devnum));
	/* Release the resources */	 
	if (new_device->usbd_cdesc)
		usbd_dealloc_usbdcdesc(new_device->usbd_cdesc);	
	usbd_const_dealloc(&new_device->bus->hc.device_buffer, new_device);
	
	return;
}

usbd_device_t *usbd_alloc_device(struct usbd_hc *hc)
{
	return usbd_const_alloc(&hc->device_buffer);
}
/*
 * We are for the time being too much biased to root hub ports , so currently 
 * we use this as primary key to find the device when it disconnects.
 * 
 * Following members are used
 * 
 * m_source : which is the hcd drivers process endpoint,which helps to find
 * 	      the device pool associated with hcd and deallocate them.
 * m1_i1    : contain the port number
 * 
 */ 

void usbd_device_detached(message m2usbd)
{
    usbd_device_t *device_current;
    usbd_device_t *prev = device_current;
    usbd_bus_t *hc_bus;
    message m2hc;
    u16_t port; 
    
    hc_bus = usbd_find_hc(m2usbd.m_source, NULL);
    if (NULL == hc_bus) {
		DPRINTF(1,("\n host controller driver for device not found"));
		return;
    }
	
    port = m2usbd.m1_i1;
    device_current = hc_bus->dev_list_start;
    while (device_current) {
	    if (device_current->port.portno == port) {
		    if (device_current == usbd.hc_list_start->dev_list_start)
			    usbd.hc_list_start->dev_list_start = device_current->next_dev;
		    prev->next_dev = device_current->next_dev;
		    DPRINTF(1,("Bus:%d Port:%x Device:%x ID 0x%04x:0x%04x:"
					    "detached", device_current->bus->usbrev,
					    device_current->port.portno, 
					    device_current->devnum,
					    device_current->ddesc.idVendor,
					    device_current->ddesc.idProduct));
		    clr_bit(device_current->bus->devmap,device_current->devnum - 1);
		    m2hc.m_type = USBD2HC_CANCEL_XFER;
		    /* Later it should be XFER_ALL */
		    m2hc.m2_i1 = XFER_INTERRUPT;
		    m2hc.m2_l1 = (u32_t) device_current;
		    usbd_msg_client(device_current->bus->hc.proc,&m2hc);
		    /* if a device driver is associated with device then reset
		     * the device pointer to NULL
		     */
		    if (device_current->dd) {
			    message m2usbdi;
			    m2usbdi.m_type = USBD2USBDI_DEVICE_DISCONNECT;
			    m2usbdi.m2_l1  = (u32_t) device_current;
			    usbd_msg_client(device_current->dd->dd_procnr,&m2usbdi);
			    device_current->dd->ddevice = NULL;
		    }
		    /* Deallocate the configuration descriptor list associated 
		     * with device 
		     */
		    usbd_dealloc_usbdcdesc(device_current->usbd_cdesc);
		    usbd_const_dealloc(&device_current->bus->hc.device_buffer,
				    device_current);
		    return;
	    }
	    device_current = device_current->next_dev;
	}
	DPRINTF(0, ("no such device"));
}

usbd_device_t *usbd_find_dev(usbd_device_t *device)
{
	usbd_bus_t *buscurrent;
	usbd_device_t *dcurrent;
	
	buscurrent = usbd.hc_list_start;
	
	while (buscurrent) {
		dcurrent = buscurrent->dev_list_start;
		while (dcurrent) {
			if (dcurrent == device)
				return dcurrent;
			dcurrent = dcurrent->next_dev;
		} 
		buscurrent = buscurrent->next_bus;
	}
	return NULL;
}
/*
 * Usually called when a hcd deregister and when usbd does usbd_exit()
 */
void usbd_dealloc_hc_devices(usbd_bus_t *hc)
{
	usbd_device_t *device_current = hc->dev_list_start;

	while (device_current) {
		if (device_current->dd) {
			message m2usbdi;
			m2usbdi.m_type = USBD2USBDI_DEVICE_DISCONNECT;
			m2usbdi.m2_l1  = (u32_t) device_current;
			usbd_msg_client(device_current->dd->dd_procnr,&m2usbdi);
			device_current->dd->ddevice = NULL;
		}
		usbd_const_dealloc(&device_current->bus->hc.device_buffer,device_current);
		device_current = device_current->next_dev;
	}
}
/*
 * Meant for sending just notifications without any parameters 
 * in the body of message.
 * 
 * Blocking routine ,use it carefully to avoid deadlock
 * 
 * event  : Any constant from usbd.h meant for IPC
 * procnr : Destination endpoint
 */
void usbd_notify(int event, endpoint_t procnr)
{
	int r;
	message msg;
	msg.m_type = event;
	
	r = send(procnr, &msg);
	if (OK != r)
		DPRINTF(1, ("send() -> endpoint %d failed", procnr));
}
/* Same as usbd_notify but we could include parameters in message
 * Blocking routine 
 * 
 * procnr : Destination endpoint
 * msg 	  : the message
 */ 
int usbd_msg_client(endpoint_t procnr, message *msg)
{
	int r = 0;

	DPRINTF(0, ("sending msg ..")); 
	r = send(procnr, msg);
	if (OK != r)  {
		DPRINTF(1, ("send() to hc failed"));
		return r;
	}
	DPRINTF(0, ("msg send.."));
	return r;
}

usbd_bus_t *usbd_find_hc(endpoint_t proc, usbd_bus_t *prev)
{
	usbd_bus_t *current = prev = usbd.hc_list_start;
	while (current) {
		  if (current->hc.proc == proc)
			  return current;
		  prev = current;
		  current = current->next_bus;
	}
	return NULL;
}

/*
 * One of the clean way to terminate a driver, from what i understood from
 * service utility,thanks to sphere from #minix for the suggestion
 * to emulate 'service down' within the driver.
 * Blocking code 
 */
void usbd_fatal_abort(char *reason)
{
	message m2rs;
	m2rs.m_type = RS_DOWN;
	m2rs.RS_CMD_ADDR = "usbd";
	m2rs.RS_CMD_LEN = strlen("usbd");
	printf("%s did an abort: %s \n", DPRINT_PREFIX, reason);
	send(RS_PROC_NR, &m2rs);
}

/*
 * Control request that goes to the respective hcd
 * 
 * Input
 * dev  : device 
 * pipe : usb pipe check usbd.h for more information
 * data : if at all control request has data stage 
 * len  : data length
 * 
 * Currently this code is blocking  ie control request is send 
 * and wait for completion ,in other words it is synchronous,
 * Another way to implement it would be to have Control request 
 * queue within usbd so when a request is made it puts it into the
 * queue and returns to main , while the driver requested would be
 * blocked until the queue is executed and gets a reply from hcds 
 * something like HC2USBD_CONTROL_REQ_STS,in other words we have 
 * asynchronous control request, end result would be usbd responding
 * to requests from other drivers, so this is a possible TO DO. 
 * 
 * Well another way is Threads ;-)
 * 
 */ 
int usbd_control_req(usbd_device_t *dev, u32_t pipe, char *data, int len)
{
	int r = 0;
	message m2hc;
	message rr;
	phys_bytes phys;

	/* Setting up control request to respective hc driver */
	m2hc.m_type = USBD2HC_CONTROL_REQ;
	m2hc.m2_i1 = len;
	m2hc.m2_l2 = pipe;
	m2hc.m2_i3 = dev->port.portno;

	/* Get the physical address of our data request */
	phys = usbd_vir_to_phys(&dev->dr);
	if (phys == EINVAL) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,&dev->dr));
		return EINVAL;
	}
	m2hc.m2_l1 = phys;
	DPRINTF(0, ("phys dr ->%x ", phys));

	m2hc.m2_p1 = data;
	/* Send request to responsible host controller
	 * hmm,we are blocked 
	 */
	r = sendrec((dev->bus->hc.proc), &m2hc);
	if (r != OK) {
		DPRINTF(1, ("send()-> to hcd %d failed",dev->bus->hc.proc));
		return r;
	}

	DPRINTF(0, ("received %d", m2hc.m1_i1));
	return m2hc.m1_i1;
}

int usbd_interrupt_req(message m2usbd)
{
	int r = 0;
	message m2hc;
	phys_bytes phys;
	usbd_device_t *dev = (usbd_device_t *) m2usbd.m2_l1;
	usbd_device_driver_t *dd;
	message reply;
		 
	dev = usbd_find_dev(dev);
	if (NULL == dev) {
	    DPRINTF(0,("no such device"));
	    reply.m_type = EINVAL;
	    /* requester is blocked */
	    usbd_msg_client(m2usbd.m_source, &reply);
	    return 0;
	}
			 
	dd = usbd_find_dd(dev->dd_procnr);
	if (NULL == dd) {
	    DPRINTF(0,("no such device driver"));
	    reply.m_type = EINVAL;
	    /* requester is blocked */
	    usbd_msg_client(m2usbd.m_source, &reply);
	    return 0;
	}
	
	m2hc.m_type = USBD2HC_INTERRUPT_REQ;
	/* we assume all interrupt are  IN */
	m2hc.m2_l2  = usb_rcvintpipe(dev, m2usbd.m2_l2); /* pipe */
	m2hc.m2_l1  = (u32_t) m2usbd.m2_p1; /* data pointer */
	m2hc.m2_p1  = (char *) dev;

	/*REM: */
 	DPRINTF(0, ("phys dr ->%x ", phys));
	DPRINTF(0, ("pipe usbd ->%x ", m2hc.m2_l2));
	/* Send request to responsible host controller 
	 * we are blocked until uhci reply
	 */
	r = sendrec((dev->bus->hc.proc), &m2hc);
	if (OK != r) {
		DPRINTF(1, ("send() -> hcd failed:%d",r));
		reply.m_type = r;
		usbd_msg_client(m2usbd.m_source, &reply);
		return r;
	}
	/* reply from hc will contain the result */
	usbd_msg_client(m2usbd.m_source, &m2hc);
	return 0;
}


/*
 * These are the actual routines to send and receive control messages, check
 * USB 2.0 chapter 9 
 */

/* 9.4.1 */
/*
int usbd_clear_feature()
{
	return ENOSYS;
}
*/

/* 9.4.2 */
/*
 * A device may have multiple configuration and each configuration may have 
 * zero or more Interface and each Interface may have zero or more endpoints
 * may be each Interface have an alternate setting.
 * 
 * TO DO: it doesn't handle alternate setting 
 *  
 */ 
int usbd_get_configuration(usbd_device_t *dev)
{
	int size;
	u8_t *configdump;
	usb_config_descriptor_t *cdesc;
	usbd_Config_descriptor_t *cprev = NULL;
	int r = 0, cfindex = 0;

    /* Allocate a page to buffer all the configuration and friends following*/
	configdump = usbd_var_alloc(I386_PAGE_SIZE);
	if (NULL == configdump) {
		DPRINTF(1, ("GET CONFIGURATION :ENOMEM"));
		return ENOMEM;
	}

	for (cfindex = 0; cfindex < dev->ddesc.bNumConfigurations; cfindex++) {
        
        /* clear it up for config  */
		memset(configdump, 0, size);
	
		r = usbd_get_descriptor(dev, UDESC_CONFIG, cfindex, configdump,
				SIZEOF_CONFIG_DESC);
		if (OK != r)
			break;

		cdesc = (usb_config_descriptor_t *) configdump;
		size = cdesc->wTotalLength;

		DPRINTF(0, ("cdesc->wTotalLength %d", size));
		
		if (size > I386_PAGE_SIZE) {
			DPRINTF(1,("GET_CONFIGURATION :warning truncated to %d got size %d",
				 I386_PAGE_SIZE, size));
			size = I386_PAGE_SIZE;
		}

		r = usbd_get_descriptor(dev, UDESC_CONFIG, cfindex, configdump, size);
		if (OK != r)
			break;
        /* Extract the first config descriptor and continue to extract what ever
         * follows the config descriptor as per USB specification.
         * Append in the sense that ,device->usbd_cdesc is a list of config 
         * descriptors for the that particular device. look at the figure below,
         * to see what is happening.
         * 
         * device->usbd_cdesc(1).....................->usbd_cdesc(n)
         * 		|		|
         * 	  usbd_idsesc(1)->usbd_idesc(2)->... . 
         * 		|	        .
         * 	usbd_edesc(1)->usbd_edesc(2)->......
	 */		
		r = usbd_append_config(dev, cfindex, configdump, &cprev,size);
		if (r != OK)
			break;
	}

	usbd_var_dealloc(cdesc);

	return r;
}
/*
 * As we get request from the layer above that is the device drivers,
 * they do accompany some data buffers along with them , but hc requires
 * them as a physical address ,this routine convert them to physical and
 * possible check for a NULL condition.
 */
phys_bytes usbd_check_buffer(void *buf,phys_bytes *phys)
{	
	if (NULL != buf) {
		*phys = usbd_vir_to_phys(buf);
		if (EINVAL == *phys) {
			DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x",SELF, *phys));
			return EINVAL;
		}
		DPRINTF(0, ("phys data ->%x", *phys));
	}
	
	return OK;
}
/* 9.4.3 */
int usbd_get_descriptor(usbd_device_t *dev, u16_t type, u8_t index,
		void *buf,int size)
{
	phys_bytes phys;
	int r;
	
	dev->dr.bmRequestType = USB_DIR_IN;
	dev->dr.bRequest = UR_GET_DESCRIPTOR;
	dev->dr.wValue = (type << 8) + index;
	dev->dr.wIndex = 0;
	dev->dr.wLength = size;
	
	r = usbd_check_buffer(buf, &phys);
	if (OK != r)
		return r;
		
	return usbd_control_req(dev, usb_rcvctrlpipe(dev, 0), (char *) phys, size);
}

/* 9.4.4 */
int usbd_get_interface(usbd_device_t *dev, int iface,  char *alt_set)
{
	phys_bytes phys;
	int r;
	
	if (!dev->active_cdesc) {
		DPRINTF(1, ("GET_INTERFACE: device not yet configured"));
		return EINVAL;
	}

	dev->dr.bmRequestType = USB_DIR_IN | USB_RECIP_INTERFACE;
	dev->dr.bRequest = UR_GET_INTERFACE;
	dev->dr.wValue = 0;
	dev->dr.wIndex = iface;
	dev->dr.wLength = 1;
	
	r = usbd_check_buffer(alt_set, &phys);
	if (OK != r)
		return r;
		
	return usbd_control_req(dev, usb_rcvctrlpipe(dev, 0), (char *) phys, 1);
}

/* 9.4.5 */
int usbd_get_status(usbd_device_t *dev,char rtype,int windex,char *status)
{
	phys_bytes phys;
	int r;
	
	switch (rtype) {
	case USB_RECIP_DEVICE: windex = 0;
	case USB_RECIP_INTERFACE:
	case USB_RECIP_ENDPOINT:
	break;
		default: return EINVAL;
	} 
	
	dev->dr.bmRequestType = USB_DIR_IN | rtype;
	dev->dr.bRequest = UR_GET_STATUS;
	dev->dr.wValue = 0;
	dev->dr.wIndex = windex;
	dev->dr.wLength = 2;
	
	r = usbd_check_buffer(status, &phys);
	if (OK != r)
		return r;
	
	return usbd_control_req(dev, usb_rcvctrlpipe(dev, 0), (char *) phys, 2);
}

/* 9.4.6 */
int usbd_set_address(usbd_device_t *dev)
{
	if (dev->active_cdesc) {
		DPRINTF(1, ("SET_ADDRESS: not possible once device is configured"));
		return EINVAL;
	}

	dev->dr.bmRequestType = USB_DIR_OUT;
	dev->dr.bRequest = UR_SET_ADDRESS;
	dev->dr.wValue = dev->devnum;
	dev->dr.wIndex = 0;
	dev->dr.wLength = 0;
	return usbd_control_req(dev, usb_snddefctrl(dev), NULL, 0);

}

/* 9.4.7 */
int usbd_set_configuration(usbd_device_t *dev,int cfval)
{
	usbd_Config_descriptor_t *usbdcdesc;
	int r;
		  
	dev->dr.bmRequestType = USB_DIR_OUT;
	dev->dr.bRequest = UR_SET_CONFIG;
	dev->dr.wValue = cfval;
	dev->dr.wIndex = 0;
	dev->dr.wLength = 0;

	r = usbd_control_req(dev, usb_sndctrlpipe(dev, 0), NULL, 0);
	return r;
}

/* 9.4.8 */
/*
int usbd_set_descriptor()
{
	return ENOSYS;
}
*/

/* 9.4.9 */
/*
int usbd_set_feature()
{
	return ENOSYS;
}
*/

/* 9.4.10 */
int usbd_set_interface(usbd_device_t *dev, int iface, int alt_set)
{
	int r;

	if (!dev->active_cdesc) {
		DPRINTF(1, ("SET_INTERFACE: device not yet configured"));
		return EINVAL;
	}
	/* we at least check till here ,further we don't check for valid alternate 
	 * setting num ,let hc return request error if at all invalid.
	 */
	if (dev->active_cdesc->bNumInterface < iface)
		return EINVAL;

	dev->dr.bmRequestType = USB_DIR_OUT | USB_RECIP_INTERFACE;
	dev->dr.bRequest = UR_SET_INTERFACE;
	dev->dr.wValue = alt_set;
	dev->dr.wIndex = iface;
	dev->dr.wLength = 0;

	r = usbd_control_req(dev, usb_sndctrlpipe(dev, 0), NULL, 0);
	if (r == OK)
		dev->active_ifnum = iface;

	return r;
}

/* 9.4.11 */
/*
int usbd_synch_frame()
{
	return ENOSYS;
}
*/

int usbd_get_device_descriptor(usbd_device_t *dev)
{
	return usbd_get_descriptor(dev, UDESC_DEVICE, 0, &dev->ddesc,
				   sizeof(dev->ddesc));
}

/* Extended request */
int usbd_get_string(usbd_device_t *dev, u16_t langid, u8_t index,void *buf, int size)
{
	phys_bytes phys;
	int r;
	
	dev->dr.bmRequestType = USB_DIR_IN;
	dev->dr.bRequest = UR_GET_DESCRIPTOR;
	dev->dr.wValue = (UDESC_STRING << 8) + index;
	dev->dr.wIndex = langid;
	dev->dr.wLength = size;

    r = usbd_check_buffer(buf, &phys);
	if (OK != r)
		return r;

	return usbd_control_req(dev, usb_rcvctrlpipe(dev, 0),(char *) phys, size);
}

int usbd_set_idle(usbd_device_t *dev,  int duration, int report_id, int ifno)
{

	dev->dr.bmRequestType  = USB_RT_HIDD;
	dev->dr.bRequest = USB_REQ_SET_IDLE;
	dev->dr.wValue = (duration << 8) | report_id;
	dev->dr.wIndex = ifno;
	dev->dr.wLength = 0;
	
	return usbd_control_req(dev, usb_sndctrlpipe(dev, 0), NULL, 0);
}

int usbd_get_idle(usbd_device_t *dev, int report_id, int ifno,char *data)
{

	dev->dr.bmRequestType  = USB_RT_HIDD_IN;
	dev->dr.bRequest = USB_REQ_GET_IDLE;
	dev->dr.wValue = (0 << 8) | report_id;
	dev->dr.wIndex = ifno;
	dev->dr.wLength = 1;
	
	return usbd_control_req(dev, usb_rcvctrlpipe(dev, 0), data, 1);
}

int usbd_set_protocol(usbd_device_t *dev, int protocol, int ifno)
{
	dev->dr.bmRequestType = USB_RT_HIDD;
	dev->dr.bRequest = USB_REQ_SET_PROTOCOL;
	dev->dr.wValue = protocol;
	dev->dr.wIndex = ifno;
	dev->dr.wLength = 0;
    
    return usbd_control_req(dev, usb_sndctrlpipe(dev, 0), NULL, 0);
}


int usbd_set_report(usbd_device_t *dev,int rtype,int rid,int rlen,int ifno,															 char *data)
{
	dev->dr.bmRequestType = USB_RT_HIDD;
	dev->dr.bRequest = USB_REQ_SET_REPORT;
	dev->dr.wValue = (rtype << 8) | rid;
	dev->dr.wIndex = ifno;
	dev->dr.wLength = rlen;
    
    return usbd_control_req(dev,usb_sndctrlpipe(dev, 0) , data, rlen);
}

int usbd_get_protocol(usbd_device_t *dev, int ifno, char *data)
{
	dev->dr.bmRequestType = USB_RT_HIDD_IN;
	dev->dr.bRequest = USB_REQ_GET_PROTOCOL;
	dev->dr.wValue = 0;
	dev->dr.wIndex = ifno;
	dev->dr.wLength = 1;
    
    return usbd_control_req(dev, usb_rcvctrlpipe(dev, 0), data, 1);
}
int usbd_get_report(usbd_device_t *dev,int rtype,int rid,int rlen,int ifno,
		char *data)
{
	dev->dr.bmRequestType = USB_RT_HIDD_IN;
	dev->dr.bRequest = USB_REQ_GET_REPORT;
	dev->dr.wValue = (rtype << 8) | rid;
	dev->dr.wIndex = ifno;
	dev->dr.wLength = rlen;
    
    return usbd_control_req(dev,usb_rcvctrlpipe(dev, 0) , data, rlen);
}
/*  maintaining list in usbd_config_descriptor 
 * 
 * Input
 * dev		   : device
 * cfindex     : nth configuration 
 * configdump  : entire dump of config details
 * cprev	   : previous config
 * size		   : total size of configdump 
 */
int usbd_append_config(usbd_device_t *dev, u8_t cfindex, u8_t *configdump,     
		usbd_Config_descriptor_t **cprev, int size) 
{
	usbd_Config_descriptor_t *new_usbdcdesc;
	int offset = 0,r;
	
	/* checking to see if we have a config signature that consist of 
	 * size of config descriptor followed by UDESC_CONFIG of the config 
	 * descriptor (9.6.3) USB 2.0 if we find a valid one which may not be at 
	 * configdum[0] and configdump[1] due to crippled devices, 
	 * but usually a [0] and [1] respectively. 
	 * the offset from start of configdump is returned.
	 */ 
	usbd_valid_descriptor(configdump, size, UDESC_CONFIG, SIZEOF_CONFIG_DESC, 
			&offset);
						 
	if (EINVAL == offset) 
		return offset;
		
	configdump += offset;
	DPRINTF(0,("append config offset %d",offset));
	
	new_usbdcdesc = calloc(1, sizeof(usbd_Config_descriptor_t));
	if (NULL == new_usbdcdesc)
		return ENOMEM;
	
	DPRINTF(0, (" new_config %08x", (new_usbdcdesc)));

	new_usbdcdesc->index = cfindex + 1;
	new_usbdcdesc->usbd_idesc = NULL;
	new_usbdcdesc->next = NULL;

	DPRINTF(0, ("prev %08x", (*cprev)));

	new_usbdcdesc->cdesc = calloc(1, SIZEOF_CONFIG_DESC);
	if (!new_usbdcdesc->cdesc) {
		free(new_usbdcdesc);
		return ENOMEM;
	}
    
    
	memcpy(new_usbdcdesc->cdesc,configdump,SIZEOF_CONFIG_DESC);

	if (NULL == dev->usbd_cdesc) {
		dev->usbd_cdesc = new_usbdcdesc;
		(*cprev) = new_usbdcdesc;
		DPRINTF(0, ("prev new_config %08x", (*cprev)));
		goto done;
	}
	/* a pointer at the callee remembers the previous element */ 
	(*cprev)->next = new_usbdcdesc;
	(*cprev) = new_usbdcdesc;
	
done:
    /* now we pick out the interface details that follows the config descriptor*/
	r = usbd_extract_interface(dev, configdump + SIZEOF_CONFIG_DESC, *cprev, 
			size);  
	return r;
}

/* each configuration may have multiple interface extract one by one 
 * and fill up the details from configdump and further extract endpoint details 
 * for respective interface.
 * Input
 * 
 * dev		  : device 
 * configdump 	  : entire config  
 * current 	  : since a device have multiple configuration ,the active 
 * 		    config when this routine is called.
 * size		  : size of configdump
 */
int usbd_extract_interface(usbd_device_t *dev, u8_t *configdump,
		usbd_Config_descriptor_t *current, int size)
{
	usb_config_descriptor_t *cdsec;
	usbd_Interface_descriptor_t *iprev = NULL;
	usbd_Endpoint_descriptor_t  *eprev = NULL;
	int iindex, eindex, r, offset = 0;
	
	for (iindex = 0; current->cdesc->bNumInterface > iindex; iindex++) {
		r = usbd_append_interface(dev, iindex, configdump, current, &iprev,
				&offset, size);   
		
		DPRINTF(0,("usbd_append_interface offset %d",offset));
		if (OK != r)
			return r;
	    /* offset contains valid offset of current interface descriptor 
	     * refer usbd_append_config() for details 
	     */
		configdump += (SIZEOF_INTERFACE_DESC + offset);
		/* find enpoints */
		for (eindex = 0; iprev->idesc->bNumEndpoints > eindex; eindex++) {
			DPRINTF(0,("endpoint loop %d",current->cdesc->bNumInterface));
		    r = usbd_append_endpoint(dev, eindex, configdump, iprev, &eprev,
				    &offset, size);
			DPRINTF(0,("usbd_append_endpoint offset %d",offset));
			if (OK != r)
				return r;
			configdump += (SIZEOF_ENDPOINT_DESC + offset);
		}
		/* clear it up for next interface or else it will point to
		 * previous endpoint
		 */
		eprev = NULL;
	}
	return r;
}

/* maintaining list in usbd_interface_descriptor 
 * 
 * Input
 * iindex      : nth interface 
 * configdump  : entire config details  
 * ccurrent    : current config that interface is part of
 * iprev       : points to previous interface 
 * offset      : check out usbd_append_config()
 * size        : size of config dump 
 */

int usbd_append_interface(usbd_device_t *dev, u8_t iindex,u8_t *configdump,
		usbd_Config_descriptor_t *ccurrent, 
		usbd_Interface_descriptor_t **iprev, int *offset,int size)
{
    /* this function follows similar logic of usbd_append_config() */
    struct usbd_interface_descriptor *new_usbdidesc;
    *offset = 0;
    usbd_valid_descriptor(configdump, size, UDESC_INTERFACE, 
		    sizeof(usb_interface_descriptor_t), offset);
    if (EINVAL == *offset) 
		return *offset;
		
	new_usbdidesc = calloc(1, sizeof(usbd_Interface_descriptor_t));
	if (NULL == new_usbdidesc)
		return ENOMEM;

	new_usbdidesc->index = iindex + 1;
	new_usbdidesc->next = NULL;

	new_usbdidesc->idesc = calloc(1, SIZEOF_INTERFACE_DESC);
	if (NULL == new_usbdidesc->idesc) {
		free(new_usbdidesc);
		return ENOMEM;
	}
    	configdump += *offset;
	memcpy(new_usbdidesc->idesc, configdump ,SIZEOF_INTERFACE_DESC);

	if (NULL == ccurrent->usbd_idesc) {
		ccurrent->usbd_idesc = new_usbdidesc;
		(*iprev) = new_usbdidesc;
		return OK;
	}

	(*iprev)->next = new_usbdidesc;
	(*iprev) = new_usbdidesc;
	return OK;
}


/* maintaining list in usbd_endpoint_descriptor 
 * 
 * Input 
 * dev      : device
 * edinex   : endpoint index number
 * icurrent : current interface the endpoint is part of
 * eprev    : previous enpoint (meant for linked list)
 * offset   : check usbd_extract_interface()
 * size     : size of configdump
 */
int usbd_append_endpoint(usbd_device_t *dev, u8_t eindex, u8_t *configdump, 
		usbd_Interface_descriptor_t *icurrent, 
		usbd_Endpoint_descriptor_t **eprev , int *offset, 
		int size)
{
	/* this function follows similar logic of usbd_append_config() */
	struct usbd_endpoint_descriptor *new_usbdedesc;
    	*offset = 0;
    	usbd_valid_descriptor(configdump, size, UDESC_ENDPOINT, SIZEOF_ENDPOINT_DESC,
		    offset);
						  
	if (EINVAL == *offset) 
		return *offset;
		
	configdump += *offset;
						  
	new_usbdedesc = calloc(1, sizeof(usbd_Endpoint_descriptor_t));
	if (NULL == new_usbdedesc)
		return ENOMEM;

	new_usbdedesc->index = eindex + 1;
	new_usbdedesc->next = NULL;

	new_usbdedesc->edesc = calloc(1, SIZEOF_ENDPOINT_DESC);
	if (NULL == new_usbdedesc->edesc) {
		free(new_usbdedesc);
		return ENOMEM;
	}
	
	memcpy(new_usbdedesc->edesc,configdump,SIZEOF_ENDPOINT_DESC);

	if (NULL == icurrent->usbd_edesc) {
		icurrent->usbd_edesc = new_usbdedesc;
		(*eprev) = new_usbdedesc;
		return OK;
	}

	(*eprev)->next = new_usbdedesc;
	(*eprev) = new_usbdedesc;
	return OK;
}
/* find the proper offset of the valid descriptor type within the configdump
 * we need this to remove unrecognized data within the dump and return proper
 * offset
 */
int usbd_valid_descriptor(u8_t *configdump, int configlen, u8_t desctype, 
		u8_t descsize,int *offset)
{
     int desclen = 0;
     u16_t descts;
     *offset = 0;
     
     while (desclen < configlen) {
	     if (configdump[0] == descsize  &&  configdump[1] == desctype) {
		     DPRINTF(0,("Found %02X:%02X",descsize, desctype));
		     return *offset;
	     }
	     if (configdump[1] == desctype && desclen > descsize) {
		     printf("bug: oversized descriptor");
		     *offset = EINVAL;
		     return *offset;
	     }
	     DPRINTF(0,("%02X/%02X != %02X/%02X", descsize,desctype ,
				     configdump[0], configdump[1]));
	     configdump  += 3;
	     desclen 	+= 3;
	     (*offset)   += 3;
     }
        
    *offset = EINVAL;
    return (*offset);
}

/*
 * Well usbd_device->cdesc is pointing the start of the cdesc listing
 * this routine is meant to deallocate the chain of descriptors 
 * (config,interface,endpoint)
 */ 
void usbd_dealloc_usbdcdesc(usbd_Config_descriptor_t *usbd_cdesc)
{
	while (usbd_cdesc) {
		usbd_Config_descriptor_t *cnext;
		cnext = usbd_cdesc->next;
		/* remove interface associated with this config */
		while (usbd_cdesc->usbd_idesc) {
			usbd_Interface_descriptor_t *inext;
			inext = usbd_cdesc->usbd_idesc->next;
			/* remove endpoints associated with this interface */
			while (usbd_cdesc->usbd_idesc->usbd_edesc) {
				usbd_Endpoint_descriptor_t *enext;
				enext = usbd_cdesc->usbd_idesc->usbd_edesc->next;

				free(usbd_cdesc->usbd_idesc->usbd_edesc->edesc);
				free(usbd_cdesc->usbd_idesc->usbd_edesc);
				usbd_cdesc->usbd_idesc->usbd_edesc = enext;
			}

			free(usbd_cdesc->usbd_idesc->idesc);
			free(usbd_cdesc->usbd_idesc);
			usbd_cdesc->usbd_idesc = inext;
		}

		free(usbd_cdesc->cdesc);
		free(usbd_cdesc);
		usbd_cdesc = cnext;
	}
}

/*
 * Register a device driver , 
 * 
 * Following members are used 
 * 
 * m_source     : process endpoint ,we use it for IPC ,and its is a primary key
 * 			      while searching for a dd.
 * m2usbd.m1_i1 : vendor  id "only used when we have vendor specific driver "
 * m2usbd.m1_i2 : product id "that doesn't belong to any class driver"
 * 			     "currently we do check this while probing TO DO:"
 * Reply 
 * m_type = USBD2USBDI_DD_REGISTERED;
 * 
 * Blocking code 
 */
void usbd_register_dd(message m2usbd)
{
	usbd_device_driver_t *new_dd;
	usbd_bus_t *buscurrent;
	usbd_device_t *dcurrent; 
	message reply; 
	message probeinfo;
	endpoint_t saved_source;
	
	int r;

	new_dd = malloc(sizeof(usbd_device_driver_t));
	if (NULL == new_dd) {
		DPRINTF(1,("failed to register device driver: %d",ENOMEM));
		reply.m_type = USBD2USBDI_DD_REGISTER_FAIL;
		reply.m1_i1  = ENOMEM;
		usbd_msg_client(m2usbd.m_source, &reply);
		return;
	}
	
	new_dd->dd_procnr = m2usbd.m_source;
	new_dd->idVendor  = m2usbd.m1_i1;
	new_dd->idProduct = m2usbd.m1_i2;
	new_dd->ddevice = NULL;
	new_dd->next = NULL;

	if (NULL == usbd.dd_list_start) {
		usbd.dd_list_start = new_dd;
		goto done;
	}
	
	new_dd->next = usbd.dd_list_start;
	usbd.dd_list_start = new_dd;
	
done:
	DPRINTF(1,("Device driver %d registered",m2usbd.m_source));
	
	reply.m_type = USBD2USBDI_DD_REGISTERED;
	/* should we be blocked here? well if usbdi implements asynchronous IPC usbd
	 * could be free , well this is a possible TO DO:
	 */
	usbd_notify(USBD2USBDI_DD_REGISTERED, m2usbd.m_source);
	saved_source = m2usbd.m_source;
	/* NOTE: we need a better way to do this ,when we have more number of devices
	 * this could be a possible bottle neck for usbd response to others,another 
	 * possible way to do this would be to keep device claim information like
	 * in the case of usbkbd we check > 
	 * 
	 * idesc.bInterfaceClass, 
	 * idesc.bInterfaceSubClass
	 * idesc.bInterfaceProtocol
	 * 
	 * within the 'usbd_device_driver' and let the usbd get the device for the 
	 * driver this is another , but its kind of dirty way as we don't
	 * separate policy and mechanism , ie policy should set by driver and
	 * usbd should serve request.
	 * 
	 * currently search for a device for this driver ,send current device info 
	 * from the loop to the driver and let driver claim which ever is apt 
	 *
	 * NB: This is not the right way to implement this feature ,right now
	 * it doesn't hurt , possible way to do this to let the device driver 
	 * do something like 'usbd_dev_id_t fdev = usbdi_get_first_device()' and 
	 * similarly in a loop 'dev = usbdi_get_next_device(fdev);' so that
	 * usbd will be free to do service others and driver will claim 
	 */
	buscurrent = usbd.hc_list_start;
	while (buscurrent) {
		dcurrent = buscurrent->dev_list_start;
		while (dcurrent) {
			probeinfo.m_type = USBD2USBDI_DD_PROBE;
			probeinfo.m2_l1 = (u32_t) dcurrent;
			r = send(m2usbd.m_source,&probeinfo);
			if (OK != r) {
				DPRINTF(1, ("send() -> %d usbdi failed: %d",
							m2usbd.m_source,r));
				break;
			}
			dcurrent = dcurrent->next_dev;
		   }
		buscurrent = buscurrent->next_bus;
	}
}


int usbd_deregister_dd(message m2usbd)
{
	usbd_device_driver_t *current;
	usbd_device_driver_t *prev;
	message m2hc;
	
	current = usbd.dd_list_start;
	prev = usbd.dd_list_start;
	
	while (current) {
		if (current->dd_procnr == m2usbd.m_source) {
			if (current == usbd.dd_list_start) 
				usbd.dd_list_start = current->next;
			else 
				prev->next = current->next;				
			if (current->ddevice) {
				m2hc.m_type = USBD2HC_CANCEL_XFER;
				m2hc.m2_i1 = XFER_INTERRUPT;
				m2hc.m2_l1 = (u32_t) current->ddevice;
				usbd_msg_client(current->ddevice->bus->hc.proc,&m2hc);
				current->ddevice->dd_procnr = 0;
				current->ddevice->dd = NULL;
			}
			
			free(current);
			goto done;		   
		}
		prev = current;
		current = current->next;
	}
	DPRINTF(0,("no such device driver"));
done:
	DPRINTF(1,("Device driver %d unregistered",m2usbd.m_source));
	return OK;
}

void usbd_probe_dd(usbd_device_t *dev)
{
	usbd_device_driver_t *current;
	message probeinfo;
	int r;
	
	DPRINTF(0,("inside probe_dd"));
	probeinfo.m_type = USBD2USBDI_DD_PROBE;
	probeinfo.m2_l1 = (u32_t) dev;
	
	current = usbd.dd_list_start;
	while (current) {
		r = sendnb(current->dd_procnr,&probeinfo);
		if (OK != r) 
			DPRINTF(1,("send() -> %d usbdi failed",current->dd_procnr));
		current = current->next;
	}
}

void usbd_hook_dd_device(message m2usbd)
{
	usbd_device_driver_t *dd;
				
	dd = usbd_find_dd(m2usbd.m_source);
	if (!dd) {
		USBDMSG(("invalid driver id received from usbdi"));
		return;
	}
	
	dd->ddevice = (usbd_device_t *) m2usbd.m2_l1;
	dd->ddevice = usbd_find_dev(dd->ddevice);
   	if (NULL == dd->ddevice) {
	    USBDMSG(("invalid device id received from usbdi"));
	    return;
	}
	dd->ddevice->dd_procnr = m2usbd.m_source;
	dd->ddevice->dd = dd;
	USBDMSG(("device driver %d claimed device 0x%04x:0x%04x",m2usbd.m_source,
				dd->ddevice->ddesc.idVendor,
				dd->ddevice->ddesc.idProduct));
}

usbd_device_driver_t *usbd_find_dd(endpoint_t dd_procnr)
{
	usbd_device_driver_t *current;
	
	current = usbd.dd_list_start;
	while (current) {
		   if (current->dd_procnr == dd_procnr)
			   return current;
		   current = current->next;
	}
	return NULL;
}

usbd_Config_descriptor_t *usbd_find_usbdcdesc(usbd_device_t *dev,int cfno)
{	
	usbd_Config_descriptor_t *ccurrent;
	ccurrent = dev->usbd_cdesc;
	while (ccurrent) {
		if (ccurrent->cdesc->bConfigurationValue == cfno)
			return ccurrent;
		ccurrent = ccurrent->next;
	}
	return NULL;
}

usbd_Interface_descriptor_t *usbd_find_usbdidesc(usbd_Config_descriptor_t *usbdcdesc,int ifno)
{
	usbd_Interface_descriptor_t *icurrent;
	icurrent = usbdcdesc->usbd_idesc;
	while (icurrent) {
		if (icurrent->idesc->bInterfaceNumber == ifno)
			return icurrent;
		icurrent = icurrent->next;
	}
	return NULL;
}

usbd_Endpoint_descriptor_t *usbd_find_usbdedesc(usbd_Interface_descriptor_t *usbdidesc,int eindex)
{   
	usbd_Endpoint_descriptor_t *ecurrent;
	int index = 0; 
	
	ecurrent = usbdidesc->usbd_edesc;
	while (ecurrent) {
		if (index == eindex)
			return ecurrent;
		ecurrent = ecurrent->next;
		index++;
	}
	return NULL;
}


/*
 * All the request from USBDI are handled here 
 * 
 * m2_i1 : request 
 * m2_l1 : device handle from usbdi
 * others differ on request
 * 
 */ 
void usbd_req_handler(message m2usbd)
{
#define DBG_REQH 0 
	message reply;
	int r;
	usbd_device_t *dev ;
	usbd_Config_descriptor_t *usbdcdesc;
	usbd_Interface_descriptor_t *usbdidesc;
	usbd_Endpoint_descriptor_t *usbdedesc;
	char buf[100];
    
	dev = (usbd_device_t *) m2usbd.m2_l1;
	/* a valid device ? */
	dev = usbd_find_dev(dev);
	if (NULL == dev) {
		USBDMSG(("invalid device id received from usbdi"));
	    r = EINVAL;
	    goto done;
	}
	
	r = OK;  
	switch(m2usbd.m2_i1) {
	case USBDI2USBD_INTERRUPT_REQ:
	     	  /*
	      	  * input 
		  * m2_l2 : device endpoint
		  * m2_p1 : data ptr (physical address)
		  * reply 
		  * awalys OK 
		  */
	     	  DPRINTF(DBG_REQH,("USBDI2USBD_INTRRUPT_REQ"));
	    	  DPRINTF(0, ("endpoint ->%x ",m2usbd.m2_l2));
	     	  usbd_interrupt_req(m2usbd);
		  break; 
	case USBDI2USBD_BULK_REQ:
		 /* TO DO */
		  break;
	case USBDI2USBD_ISOC_REQ:
		 /* TO DO */
		  break;
  	case GET_UDESC_DEVICE:
		 /*
		  * m3_ca1 is 14 bytes but ddesc is 18 bytes ,so we split it into 2 
		  * as given below 
		  * 
		  * reply
		  * m3_i1  : device descriptor length + type
		  * m3_i2  : bcd version  
		  * m3_ca1 : remaining part of the device descriptor 
		  */
		 DPRINTF(DBG_REQH,("GET_UDESC_DEVICE"));
		 reply.m3_i1 = (dev->ddesc.bLength << 15) | dev->ddesc.bDescriptorType;
		 reply.m3_i2 = dev->ddesc.bcdUSB;
	   	 memcpy(reply.m3_ca1, ((u8_t *)&dev->ddesc) + 4, SIZEOF_DEVICE_DESC - 4);
		 break;		     
	case GET_UDESC_CONFIG:			
		 DPRINTF(DBG_REQH,("GET_UDESC_CONFIG"));
	     	/*
	     	 * input 
	      	 * m2_i2  : config number
	      	 * 
	         * reply 
	      	 * m3_ca1 : corresponding config information 
	      	 */ 
		 r = EINVAL;
		 usbdcdesc = usbd_find_usbdcdesc(dev, m2usbd.m2_i2);
		 if (!usbdcdesc)
		     break;
		 r = OK;	
		 memcpy(reply.m3_ca1, usbdcdesc->cdesc, SIZEOF_CONFIG_DESC);	 
		 break;	 
	case GET_UDESC_INTERFACE:
		 DPRINTF(DBG_REQH,("GET_UDESC_INTERFACE"));
		 /*
		  * input
		  * m2_i2 : config number 
		  * m2_i3 : interface number
		  * 
		  * reply 
		  * m3_ca1 :corresponding interface descriptor 
		  * 
		  */
		 r = EINVAL;
		 usbdcdesc = usbd_find_usbdcdesc(dev, m2usbd.m2_i2);
		 if (!usbdcdesc) 
			 break;
		 usbdidesc = usbd_find_usbdidesc(usbdcdesc, m2usbd.m2_i3);
		 if (!usbdidesc) 
			 break;
		 r = OK;
		 memcpy(reply.m3_ca1 , usbdidesc->idesc, SIZEOF_INTERFACE_DESC);			 
		 break; 
	case GET_UDESC_ENDPOINT:
		 DPRINTF(DBG_REQH,("GET_UDESC_ENDPOINT"));	 
		 /*
		  * input
		  * m2_i2 : config number
		  * m2_i3 : interface number
		  * m2_s1 : endpoint number
		  * reply
		  * m3_ca1 : corresponding endpoint descriptor
		  */ 
		 
		 r = EINVAL;
		 
		 usbdcdesc = usbd_find_usbdcdesc(dev, m2usbd.m2_i2);
		 if (!usbdcdesc)
		     break;	
		 usbdidesc = usbd_find_usbdidesc(usbdcdesc, m2usbd.m2_i3);
		 if (!usbdidesc) 
		     break; 
		 usbdedesc = usbd_find_usbdedesc(usbdidesc, m2usbd.m2_s1);
	     	 if (!usbdedesc)
		     break;
		 r = OK;
		 memcpy(reply.m3_ca1, usbdedesc->edesc, SIZEOF_ENDPOINT_DESC);
		 break;	 
	case GET_STATUS: /* TO DO: */
		 r = usbd_get_status(dev,m2usbd.m2_i2, m2usbd.m2_i3,buf);
		 if (OK != r) 
		     break;
		 reply.m3_i1 = 0;
		 break;	 
	case SET_CONFIG:
	   	  DPRINTF(DBG_REQH, ("SET_CONFIG"));	
	    	 /*
	      	  * input
	      	  * m2_i2 : config number 
	      	  * reply 
	      	  * r = status
	      	  */
	   	  r = EINVAL;
	     
	     	 usbdcdesc = usbd_find_usbdcdesc(dev, m2usbd.m2_i2);
		 if (!usbdcdesc) 
		     break;  
		 r = usbd_set_configuration(dev, m2usbd.m2_i2);
		 if (OK != r) 
			 break;	
		 r = OK;
		 dev->active_cdesc = usbdcdesc->cdesc;
		 break;
	case SET_INTERFACE: /* TO DO: */
		 DPRINTF(DBG_REQH, ("SET_INTERFACE"));
		 r = usbd_set_interface(dev, m2usbd.m2_i2, m2usbd.m2_i3);
		 break;
	case SET_IDLE:
		 DPRINTF(DBG_REQH, ("SET_IDLE"));
		 /*
		  * input 
		  * m2_i2 : duration
		  * m2_i3 : report id
		  * m2_l2 : interface no 
		  * reply 
		  * r = status 
		  */
		 r = usbd_set_idle(dev, m2usbd.m2_i2, m2usbd.m2_i3, m2usbd.m2_l2);
		 break;
	case GET_IDLE:
		 DPRINTF(DBG_REQH, ("GET_IDLE"));
		 /*
		  * input 
		  * m2_i3 : report id
		  * m2_l2 : interface no 
		  * m2_p1 : physical address of data 
		  * reply 
		  * r = status 
		  */
		 r = usbd_get_idle(dev, m2usbd.m2_i3, m2usbd.m2_l2, m2usbd.m2_p1);
		 break;	    
	case SET_PROTOCOL:
		 DPRINTF(DBG_REQH, ("SET_PROTOCOL"));
		  /*
		   * input
		   * m2_i2 : protocol 
		   * m2_l2 : interface no 
		   * reply
		   * r = status
		   */ 
	    	 r = usbd_set_protocol(dev, m2usbd.m2_i2, m2usbd.m2_l2);  
		 break;
	case GET_PROTOCOL:
		 DPRINTF(DBG_REQH, ("GET_PROTOCOL"));
		  /*
		   * input
		   * m2_i2 : interface no 
		   * m2_p1 : physical address of data 
		   * reply
		   * r = status
		   */ 
	     r = usbd_get_protocol(dev, m2usbd.m2_i2, m2usbd.m2_p1);  
	     break;
	case SET_REPORT:
		 DPRINTF(DBG_REQH, ("SET_REPORT"));
		 /* rtype,int rid,int rlen,int ifno,char *data)
		  * input 
		  * m2_i2 : report report type
		  * m2_i3 : report id
		  * m2_l2 : report lenght
		  * m2_s1 : interface number
		  * m2_p1 : physical address of data
		  * 
		  * reply
		  * r = status
		  */ 
		 r = usbd_set_report(dev, m2usbd.m2_i2, m2usbd.m2_i3, m2usbd.m2_l2, 
				 m2usbd.m2_s1, m2usbd.m2_p1); 
		 break;
	case GET_REPORT:
		 DPRINTF(DBG_REQH, ("GET_REPORT"));
		 /* rtype,int rid,int rlen,int ifno,char *data)
		  * input 
		  * m2_i2 : report report type
		  * m2_i3 : report id
		  * m2_l2 : report lenght
		  * m2_s1 : interface number
		  * m2_p1 : physical address of data
		  * 
		  * reply
		  * r = status
		  */ 
		 r = usbd_get_report(dev, m2usbd.m2_i2, m2usbd.m2_i3, m2usbd.m2_l2, 
				 m2usbd.m2_s1, m2usbd.m2_p1); 
		 break;
	default:
		 r = EINVAL;
   } 

done:
   reply.m_type = r;
   r = send(m2usbd.m_source,&reply);
   if (OK != r) 
	  DPRINTF(1,("send() -> driver %d failed: %d",m2usbd.m_source,r));
   return;
}

/*
 * This routine would be kicked out of usbd soon, should belong 
 * to something like usbdhelper.c
 */ 
void utf8_to_cstring(u8_t *utf8_str,u8_t *buf, int str_len)
{
	int i = 0;
	DPRINTF(0, ("String length %d", str_len));
	for (; i < str_len - 2; i += 2) {
		buf[i / 2] = utf8_str[i];
		DPRINTF(0, ("\n i %d %c", i / 2, buf[i / 2]));
	}
	buf[i / 2] = '\0';
}

/* Debug purpose only */
void usbd_display_usbdcdesc(usbd_device_t *new_device,usbd_Config_descriptor_t *usbd_cdesc)
{
	
	usb_string_descriptor_t iMan;
	usb_string_descriptor_t iPro;
	usb_string_descriptor_t iSea;
	
	message m2hc;
	int devnum = 0;
	int r;

    usbd_get_string(new_device,0x0409,new_device->ddesc.iManufacturer,&iMan,2);
	usbd_get_string(new_device,0x0409,new_device->ddesc.iManufacturer,&iMan,iMan.bLength-1);
	utf8_to_cstring(iMan.bString,iMan.bString,iMan.bLength);
	
    usbd_get_string(new_device,0x0409,new_device->ddesc.iProduct,&iPro,2);
	usbd_get_string(new_device,0x0409,new_device->ddesc.iProduct,&iPro,iPro.bLength-1);
	utf8_to_cstring(iPro.bString,iPro.bString,iPro.bLength);
	
    usbd_get_string(new_device,0x0409,new_device->ddesc.iSerialNumber,&iSea,2);
	usbd_get_string(new_device,0x0409,new_device->ddesc.iSerialNumber,&iSea,iSea.bLength-1);
	utf8_to_cstring(iSea.bString,iSea.bString,iSea.bLength);
	   
	   printf("\nDEVICE DESCRIPTOR");
	   printf("\nbLength :%d",new_device->ddesc.bLength); 
	   printf("\nbDescriptorType :%x",new_device->ddesc.bDescriptorType);
	   printf("\nbcdUSB :%x",new_device->ddesc.bcdUSB);
	   printf("\nbDeviceClass :%x",new_device->ddesc.bDeviceClass);
	   printf("\nbDeviceSubClass :%x",new_device->ddesc.bDeviceSubClass);
	   printf("\nbDeviceProtocol :%x",new_device->ddesc.bDeviceProtocol);
	   printf("\nbMaxPacketSize :%d",new_device->ddesc.bMaxPacketSize);
	   printf("\nidVendor :%04x",new_device->ddesc.idVendor);
	   printf("\nidProduct:%04x",new_device->ddesc.idProduct);
	   printf("\nbcdDevice :%x",new_device->ddesc.bcdDevice);
	   printf("\niManufacturer :%d",new_device->ddesc.iManufacturer);
	   printf(" ( %s )",iMan.bString);
	   printf("\niProduct :%d",new_device->ddesc.iProduct);
	   printf(" ( %s )",iPro.bString);
	   printf("\niSerial Number :%d",new_device->ddesc.iSerialNumber);
	   printf(" ( %s )",iSea.bString);
	 
	   printf("\nbNumConfigurations :%d",new_device->ddesc.bNumConfigurations);

	while (usbd_cdesc) {
		usb_config_descriptor_t *cdesc = usbd_cdesc->cdesc;
		usbd_Interface_descriptor_t *usbd_idesc = usbd_cdesc->usbd_idesc;
		printf("\nCONFIGURATION DESCRIPTOR");
		printf("\n bLength : %d", cdesc->bLength);
		printf("\n bDescriptorType : %d", cdesc->bDescriptorType);
		printf("\n wTotalLength : %d", cdesc->wTotalLength);
		printf("\n bNumInterface : %d", cdesc->bNumInterface);
		printf("\n bConfigurationValue : %d",
		       cdesc->bConfigurationValue);
		printf("\n iConfiguration : %d", cdesc->iConfiguration);
		printf("\n bmAttributes 0x%02x", cdesc->bmAttributes);
		printf("\n bMaxPower : %dmA", cdesc->bMaxPower * 2);

		while (usbd_idesc) {
			usb_interface_descriptor_t *idesc = usbd_idesc->idesc;
			usbd_Endpoint_descriptor_t *usbd_edesc = usbd_idesc->usbd_edesc;
			printf("\n INTERFACE DESCRIPTOR");
			printf("\n  bLength : %d", idesc->bLength);
			printf("\n  bDescriptorType : %d",idesc->bDescriptorType);
			printf("\n  bInterfaceNumber : %d",idesc->bInterfaceNumber);
			printf("\n  bAlternateSetting : %d",idesc->bAlternateSetting);
			printf("\n  bNumEndpoints : %d", idesc->bNumEndpoints);
			printf("\n  bInterfaceClass : %d",idesc->bInterfaceClass);
			printf("\n  bInterfaceSubClass : %d",idesc->bInterfaceSubClass);
			printf("\n  bInterfaceProtocol : %d",idesc->bInterfaceProtocol);
			printf("\n  iInterface : %d", idesc->iInterface);
			while (usbd_edesc) {
				usb_endpoint_descriptor_t *edesc = usbd_edesc->edesc;
				printf("\n  ENDPOINT DESCRIPTOR");
				printf("\n   bLength : %d", edesc->bLength);
				printf("\n   bDescriptorType : %d",edesc->bDescriptorType);
				printf("\n   bEndpointAddress : 0x%02x",edesc->bEndpointAddress);
				printf("\n   bmAttributes :%d",edesc->bmAttributes);
				printf("\n   wMaxPacketSize : %d",edesc->wMaxPacketSize);
				printf("\n   bInterval  : %d",edesc->bInterval);
				usbd_edesc = usbd_edesc->next;
			}
			usbd_idesc = usbd_idesc->next;
		}
		usbd_cdesc = usbd_cdesc->next;
	}
}
