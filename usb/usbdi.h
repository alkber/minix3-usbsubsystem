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
 * USB driver for the devices (function drivers) use this api to communicate
 * with usbd , any communication to usbd should be through USBDI ,this 
 * will keep things clean ,new api could be added here and it will work
 * as long as you properly set them in "usbd_req_handler()" in usbd. 
 * 
 * When adding new functionality to usbdi one must be really carefull 
 * about the message passing ie you could be in deadlock if you don't 
 * properly understand the communication ,at the moment most of the 
 * calls are blocking ,ie we send and wait for a reply within the 
 * function.
 *  
 */
#ifndef _USBDI_
#define _USBDI_

#include "usbd.h"

_PROTOTYPE(int usbdi_init, (void));
_PROTOTYPE(int usbdi_register_driver, (void));
_PROTOTYPE(int usbdi_get_device_desc, (usbd_dev_id_t ,usb_device_descriptor_t *));
_PROTOTYPE(int usbdi_get_device_cdesc, (usbd_dev_id_t ,usb_config_descriptor_t *,int));
_PROTOTYPE(int usbdi_get_device_idesc, (usbd_dev_id_t ,usb_interface_descriptor_t *,int ,int ));
_PROTOTYPE(int usbdi_get_device_edesc, (usbd_dev_id_t ,usb_endpoint_descriptor_t *,int ,int ,int));
_PROTOTYPE(int usbdi_get_status, (usbd_dev_id_t ,char ,int));
_PROTOTYPE(int usbdi_get_report, (usbd_dev_id_t , int, int, int, int, char *));
_PROTOTYPE(int usbdi_get_protocol, (usbd_dev_id_t , int, char *)); 
_PROTOTYPE(int usbdi_get_idle, (usbd_dev_id_t, int, int, char *));
_PROTOTYPE(int usbdi_set_config, (usbd_dev_id_t ,int ));
_PROTOTYPE(int usbdi_set_report, (usbd_dev_id_t ,int ,int ,int ,int ,char *));
_PROTOTYPE(int usbdi_set_protocol,(usbd_dev_id_t, int, int));
_PROTOTYPE(int usbdi_set_idle, (usbd_dev_id_t, int, int, int));
_PROTOTYPE(int usbdi_interrupt_req, (usbd_dev_id_t, u32_t , char*));
/* TO DO */
_PROTOTYPE(int usbdi_bulk_req, ( void ));
/* TO DO */
_PROTOTYPE(int usbdi_isoc_req, ( void ));
_PROTOTYPE(int usbdi_probe_status, (usbd_dev_id_t , char));
_PROTOTYPE(int usbdi_msg_usbd, (message msg));
_PROTOTYPE(void usbdi_fatal_abort, (char *,char *));
_PROTOTYPE(void usbdi_dereisgter_driver, (void));
#endif
