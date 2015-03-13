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
 * March 2010
 * 
 * (C) Copyright 2009,2010 Althaf K Backer <althafkbacker@gmail.com> 
 */

#include "usbdi.h"
#include <minix/ds.h>


u32_t usbd_procnr = 0;

/* All drivers should call this first before use of any api */
int usbdi_init()
{
	int r;
	r= ds_retrieve_u32("usbd", &usbd_procnr);
	if (OK != r) {
		printf("\nusbdi: usbdi_init() ds_retrieve_u32 failed : %d", r);
	    return ENXIO;
	}
	return OK;
}

/* Register with usbd , usbd is keep the procnr (process endpoint number)
 * of the registered driver process and use is for further communication
 */
int usbdi_register_driver()
{
	message m2usbd;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
	m2usbd.m_type = USBDI2USBD_REGISTER_DD;
	/* We are blocked on doing this */
	r = sendrec(usbd_procnr, &m2usbd);
	if (OK != r) {
		printf("\nusbdi: usbdi_register_driver send() -> usbd failed: %d", r);
		return r;
	}
	
	if (USBD2USBDI_DD_REGISTERED != m2usbd.m_type ) {
		printf("\nusbdi: usbdi_register_driver got bad reply from usbd: %d", 
				m2usbd.m_type);
		return USBD2USBDI_DD_REGISTER_FAIL;
	}
	
	return OK;
}

/*
 * This just emulate the 'service down' from what i understand this 
 * should be a clean way to terminate driver from within 
 */
void usbdi_fatal_abort(char *driver,char *reason)
{
	message m2rs;
	m2rs.m_type = RS_DOWN;
	m2rs.RS_CMD_ADDR = driver;
	m2rs.RS_CMD_LEN = strlen(driver);

	DPRINTF(1,(" aborted: %s", reason));
	/* Is send non blocking the right way to communicate with RS ? */
	send(RS_PROC_NR, &m2rs);
}

/* When driver exit this should be called */
void usbdi_dereisgter_driver()
{
	message m2usbd;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return;
	}
	
	m2usbd.m_type = USBDI2USBD_DERGISTER_DD;
  	r = sendnb(usbd_procnr, &m2usbd);
	if (r != 0) {
		printf("\nusbdi: usbdi_deregister_driver send() -> usbd failed: %d", r);
		return;
	}
}

int usbdi_msg_usbd(message msg)
{
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	r = send(usbd_procnr,&msg);
	if (OK != r) 
		printf("\nusbdi: send() -> usbd failed: %d",r);
    	return r;
}

/*
 * Set the device to the config in cfno 
 * 
 * device : device id from probe
 * cfno   : config number 
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned.
 */
  
int usbdi_set_config(usbd_dev_id_t device,int cfno)
{ 
	message msg;
	int r; 
    
    	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = SET_CONFIG;
    	msg.m2_i2  = cfno;
    	msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	
	return msg.m_type;
    		
}

/* 
 * Meant to reply the status of the probe to usbd , usbd does
 * a broadcast of new device when it is found to all the drivers
 * registered
 * 
 * device : device id obtained when probe request is sent from usbd
 * status : DD_DEV_ACCEPT or DD_DEV_REJECT
 * 
 * Return 
 * Fails only if usbd is down 
 */
int usbdi_probe_status(usbd_dev_id_t device,char status)
{
	message msg;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    msg.m_type = USBDI2USBD_DD_PROBE_STS;
	msg.m2_i1 = status;
	msg.m2_l1 = device;
	
    r = send(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
}

/*
 * Get the device descriptor of the device id 
 * 
 * device : deviceid that is obtained after the probe 
 * ddesc  : will contain the device descriptor 
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */
int usbdi_get_device_ddesc(usbd_dev_id_t device,usb_device_descriptor_t *ddesc)
{
	message msg;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
	msg.m_type = USBDI2USBD_REQ;
	msg.m2_i1  = GET_UDESC_DEVICE;
	msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	if (msg.m_type != OK) 
		return msg.m_type;
	if ((msg.m3_i1 >> 15) != 18)
		 return EPACKSIZE;
	if ((msg.m3_i1 & 0xff) != UDESC_DEVICE)
		 return EINVAL;
		 
	memcpy(((u8_t *)ddesc)+4,msg.m3_ca1,SIZEOF_DEVICE_DESC-4);	
	ddesc->bLength = (msg.m3_i1 >> 15);
	ddesc->bDescriptorType = (msg.m3_i1 & 0xff) ;
	ddesc->bcdUSB = msg.m3_i2;
	return OK;
}

/*
 * Get the configuration descriptor wrt to cnfno of the device 
 * 
 * device : device id obtained during the probe 
 * cdesc  : will contain the config descriptor wrt cnfno
 * cnfno  : config number
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */
int usbdi_get_device_cdesc(usbd_dev_id_t device,usb_config_descriptor_t *cdesc,int cnfno)
{
	message msg;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
	msg.m_type = USBDI2USBD_REQ;
	msg.m2_i1  = GET_UDESC_CONFIG;
	msg.m2_i2  = cnfno;
	msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return ENXIO;
	}
	
	if (msg.m_type != OK) 
		return msg.m_type;
		
	if (msg.m3_ca1[0] != SIZEOF_CONFIG_DESC) {
		printf("\n %d %d",msg.m3_ca1[0],SIZEOF_DEVICE_DESC);
	    return EPACKSIZE;
	}
	
	if (msg.m3_ca1[1] != UDESC_CONFIG)
		 return EINVAL;
		 
	memcpy(cdesc,msg.m3_ca1,SIZEOF_DEVICE_DESC);
	return OK;
}

/*
 * Get the interface descriptor wrt to cnfno ,ifno of the device 
 * 
 * device : device id obtained during the probe 
 * idesc  : will contain the interface descriptor wrt cnfno
 * cnfno  : config number
 * ifno   : interface number
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */
int usbdi_get_device_idesc(usbd_dev_id_t device,
		usb_interface_descriptor_t *idesc,int cfno,int ifno)
{
	message msg;
	int r; 
    
   	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = GET_UDESC_INTERFACE;
    	msg.m2_i2  = cfno;
    	msg.m2_i3  = ifno;
	msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return ENXIO;
	}
	
	if (msg.m_type != OK) 
		return msg.m_type;
		
	if (msg.m3_ca1[0] != SIZEOF_INTERFACE_DESC) {
		 printf("\n %d %d",msg.m3_ca1[0],SIZEOF_INTERFACE_DESC);
		 return EPACKSIZE;
	}
	
	if (msg.m3_ca1[1] != UDESC_INTERFACE)
		 return EINVAL;
		 
    	memcpy(idesc,msg.m3_ca1,SIZEOF_INTERFACE_DESC);
		
   	return 0;
}

/*
 * Get the endpoint descriptor wrt to cnfno,ifno,epno of the device 
 * 
 * device : device id obtained during the probe 
 * cdesc  : will contain the endpoint descriptor wrt cnfno,ifno,eindex
 * cnfno  : config number
 * ifno   : inteface number
 * eindex : enpoint number
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */
int usbdi_get_device_edesc(usbd_dev_id_t device,
		usb_endpoint_descriptor_t *edesc,int cfno,int ifno,int eindex)
{
	message msg;
	int r; 
	
    	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = GET_UDESC_ENDPOINT;
    	msg.m2_i2  = cfno;
    	msg.m2_i3  = ifno;
    	msg.m2_s1  = eindex;
	msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return ENXIO;
	}
	if (msg.m_type != OK) 
		return msg.m_type;
		
	if (msg.m3_ca1[0] != SIZEOF_ENDPOINT_DESC) {
		 printf("\n %d %d",msg.m3_ca1[0],SIZEOF_ENDPOINT_DESC);
		 return EPACKSIZE;
	}
	
	if (msg.m3_ca1[1] != UDESC_ENDPOINT)
		 return EINVAL;
		 
  	memcpy(edesc,msg.m3_ca1,SIZEOF_ENDPOINT_DESC);
		
	return OK;	
}

#if 0
/*
 * Not tested yet  
 */
int usbdi_get_status(usbd_dev_id_t device,char rtype,int windex)
{   
	message msg;
	int r; 
	
    if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialzed");
		return EINVAL;
	}
    
    msg.m_type = USBDI2USBD_REQ;
    msg.m2_i1  = GET_STATUS;
    msg.m2_i2  = rtype;
    msg.m2_i3  = windex;
    msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return ENXIO;
	}
	
	if (msg.m_type != OK) 
		return msg.m_type;
    			
    return msg.m3_i1;
}
#endif 

#if 0
/* Not tested */
int usbdi_set_interface(usbd_dev_id_t device,int ifno,int alt_set)
{ 
	message msg;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialzed");
		return EINVAL;
	}
	    
    msg.m_type = USBDI2USBD_REQ;
    msg.m2_i1  = SET_INTERFACE;
    msg.m2_i2  = ifno;
    msg.m2_i3  = alt_set;
    msg.m2_l1  = device;
	
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
				
   return msg.m_type;
}
#endif 

/*
 * Request an interrupt request  
 * 
 * device   : device id from probe
 * endpoint : enpoint where interrupt request is needed
 * data 	: will contain the requested data on interrupt 
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned.
 */
int usbdi_interrupt_req(usbd_dev_id_t device,u32_t endpoint,char *data)
{
	int r ;
	message msg;
	phys_bytes phys;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}

	msg.m_type = USBDI2USBD_REQ;
	msg.m2_i1  = USBDI2USBD_INTERRUPT_REQ;
	msg.m2_l2  = endpoint;
	msg.m2_l1  = device;
	
	/* Get the physical address of our data request */
	phys = usbd_vir_to_phys(data);
	if (phys == EINVAL) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,data));
		return EINVAL;
	}
	
	msg.m2_p1 = (char *)phys;
	
	DPRINTF(0, ("phys dr ->%x ", phys));
	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	
	return msg.m_type;
}

/*
 * HID specs 1.11 , 7.2.1
 * GET_REPORT 
 *  
 * Input
 * device : device handle / id from usbd obtained during probing 
 * rtype  : report type
 * rid 	  : report id 
 * rlen   : report lenght
 * ifno	  : interface number
 * data   : virtual address of the data buffer 
 * 
 * Return :
 * Respective error codes from error.h or usb specifice error codes
 * from usbd.h if all worked fine OK is returned. 
 * Note : This request is useful at initialization time for absolute items
 * and for determining the state of feature items.
 * 
 * (xxxx)This request is not meant to be used for polling the device state
 * on regular basis ,interrupt IN pipe is to be used for the purpose. 
 * 
 */  
int usbdi_get_report(usbd_dev_id_t device,int rtype,int rid,int rlen,int ifno,char *data)
{   
	message msg;
	phys_bytes phys;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}

	msg.m_type = USBDI2USBD_REQ;
	msg.m2_i1  = GET_REPORT;
	msg.m2_i2  = rtype;
	msg.m2_i3  = rid;
	msg.m2_l2  = rlen;
	msg.m2_s1  = ifno;
	
	phys = usbd_vir_to_phys(data);
	if (EINVAL == phys) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,data));
		return EINVAL;
	}
	msg.m2_p1  = (char *) phys;
    	msg.m2_l1  = device;
    
    	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	return msg.m_type;
}

/*
 * HID specs 1.11 , 7.2.2 
 * SET_REPORT 
 *  
 * Input
 * device : device handle / id from usbd obtained during probing 
 * rtype  : report type
 * rid 	  : report id 
 * rlen   : report lenght
 * ifno	  : interface number
 * data   : virtual address of the data buffer 
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 *
 */  
int usbdi_set_report(usbd_dev_id_t device,int rtype,int rid,int rlen,int ifno,char *data)
{   
	message msg;
	phys_bytes phys;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = SET_REPORT;
    	msg.m2_i2  = rtype;
    	msg.m2_i3  = rid;
    	msg.m2_l2  = rlen;
    	msg.m2_s1  = ifno;
    
    	phys = usbd_vir_to_phys(data);
	if (EINVAL == phys) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,data));
		return EINVAL;
	}
	msg.m2_p1  = (char *) phys;
   	msg.m2_l1  = device;
    
    	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	return msg.m_type;
}

/*
 * HID specs 1.11 , 7.2.3
 * GET_IDLE
 * 
 * Input  (Read specs)  
 * rid 		  : report id  
 * ifno		  : interface number
 * data		  : virtual address of the data buffer
 *  
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */
int usbdi_get_idle(usbd_dev_id_t device, int rid, int ifno, char *data)
{
	message msg;
	int r;
	phys_bytes phys;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = GET_IDLE;
	msg.m2_i3  = rid;
	msg.m2_l2  = ifno;
	msg.m2_l1  = device;
	
	phys = usbd_vir_to_phys(data);
	if (EINVAL == phys) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,data));
		return EINVAL;
	}
	msg.m2_p1  = (char *) phys;
	
    r = sendrec(usbd_procnr,&msg);
	if (OK != r) { 
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	return msg.m_type;
}

/*
 * HID specs 1.11 , 7.2.4
 * SET_IDLE
 * 
 * Input  (Read specs) 
 * duration   :  
 * rid		  : report id  
 * ifno		  : interface number
 *  
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */
int usbdi_set_idle(usbd_dev_id_t device, int duration, int rid,int ifno)
{
	message msg;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = SET_IDLE;
	msg.m2_i2  = duration;
	msg.m2_i3  = rid;
	msg.m2_l1  = device;
	msg.m2_l2  = ifno;
	
    	r = sendrec(usbd_procnr, &msg);
	if (OK != r) { 
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	return msg.m_type;
}
/* 
 * HID Specs 1.11 , 7.2.5
 * GET_PROTOCOL 
 * 
 * Input 
 * device   : device id / handle that is obtained from probe 
 * protocol : boot / report 0 ,1 resp
 * ifno     : interface number  
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */ 
int usbdi_get_protocol(usbd_dev_id_t device, int ifno, char *data)
{   
	message msg;
	int r;
	phys_bytes phys; 
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = GET_PROTOCOL;
    	msg.m2_i2  = ifno;
    	msg.m2_l1  = device;
    
    	phys = usbd_vir_to_phys(data);
	if (EINVAL == phys) {
		DPRINTF(1,("sys_umap failed for proc %d vir addr 0x%x", SELF,data));
		return EINVAL;
	}
	msg.m2_p1  = (char *) phys;
    
    	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	
	return msg.m_type;
}

/* 
 * HID Specs 1.11 , 7.2.6
 * SET_PROTOCOL 
 * 
 * Input 
 * device   : device id / handle that is obtained from probe 
 * protocol : boot / report 0 ,1 resp
 * ifno     : interface number  
 * 
 * Return :
 * Respective error codes from error.h or usb specific error codes
 * from usbd.h if all worked fine OK is returned. 
 */ 
int usbdi_set_protocol(usbd_dev_id_t device, int protocol, int ifno)
{   
	message msg;
	int r;
	
	if (!usbd_procnr) {
		printf("\nusbdi: usbdi not yet initialized");
		return EINVAL;
	}
	
    	msg.m_type = USBDI2USBD_REQ;
    	msg.m2_i1  = SET_PROTOCOL;
    	msg.m2_i2  = protocol;
    	msg.m2_l1  = device;
    	msg.m2_l2  = ifno;
    
    	r = sendrec(usbd_procnr,&msg);
	if (OK != r) {
		printf("\nusbdi: send() -> usbd failed: %d",r);
		return r;
	}
	
	return msg.m_type;
}
