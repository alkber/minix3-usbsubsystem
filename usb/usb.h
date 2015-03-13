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
 * (C) Copyright Althaf K Backer <althafkbacker@gmail.com> 2009,2010
 * (C) Copyright 1998 The NetBSD Foundation, Inc..
 * 	    
 * 
 * USB Specification version 2.0 data structures in Chapter 9
 */
#ifndef _USB_H_
#define _USB_H_

#define USB_STACK_VERSION "0.01-alpha(17032010)"
#define TESTED_ON_MINIX   "3.1.5 svn5612"
#define AUTHOR            "Althaf K Backer <althafkbacker@gmail.com>"

#include <minix/type.h>
typedef u8_t uByte;
typedef u16_t uWord;

#define USB_MAX_DEVICES   128
#define USB_MAXCONFIG     8
#define USB_MAXALTSETTING 128
#define USB_MAXINTERFACES 32
#define USB_MAXENDPOINTS  32

#define USB_PID_OUT       0xe1
#define USB_PID_IN        0x69
#define USB_PID_SETUP     0x2d

/* USB directions*/
#define USB_DIR_OUT         0x00
#define USB_DIR_IN          0x80
/* USB types     */
#define USB_TYPE_STANDARD   (0x00 << 5)
#define USB_TYPE_CLASS      (0x01 << 5)
#define USB_TYPE_VENDOR     (0x02 << 5)
#define USB_TYPE_RESERVED   (0x03 << 5)
/* USB recipients */
#define USB_RECIP_DEVICE    0x00
#define USB_RECIP_INTERFACE 0x01
#define USB_RECIP_ENDPOINT  0x02
#define USB_RECIP_OTHER     0x03
/*  Requests       */
#define UR_GET_STATUS		0x00
#define UR_CLEAR_FEATURE	0x01
#define UR_SET_FEATURE		0x03
/* Feature numbers */
#define  UF_ENDPOINT_HALT	0x00
#define  UF_DEVICE_REMOTE_WAKEUP 0x01
#define  UF_TEST_MODE		0x02
#define UR_SET_ADDRESS		0x05
/* Descriptor Types */
#define UR_GET_DESCRIPTOR	0x06
#define  UDESC_DEVICE		0x01
#define  UDESC_CONFIG		0x02
#define  UDESC_STRING		0x03
#define  UDESC_INTERFACE	0x04
#define  UDESC_ENDPOINT		0x05
#define  UDESC_DEVICE_QUALIFIER	0x06
#define  UDESC_OTHER_SPEED_CONFIGURATION 0x07
#define  UDESC_INTERFACE_POWER	0x08
#define UR_SET_DESCRIPTOR	0x07
#define UR_GET_CONFIG		0x08
#define UR_SET_CONFIG		0x09
#define UR_GET_INTERFACE	0x0a
#define UR_SET_INTERFACE	0x0b
#define UR_SYNCH_FRAME		0x0c
/*
 * HID specs 1.11 , 7.2 
 * 
 * HID requests
 * 
 */
#define USB_REQ_GET_REPORT	0x01
#define 	RT_IN		0x01
#define		RT_OUT		0x02
#define 	RT_FEA		0x03 
#define USB_REQ_GET_IDLE	0x02
#define USB_REQ_GET_PROTOCOL	0x03
#define USB_REQ_SET_REPORT	0x09
#define USB_REQ_SET_IDLE	0x0A
#define USB_REQ_SET_PROTOCOL	0x0B
/*
 * Request target types.
 */
#define USB_RT_DEVICE		0x00
#define USB_RT_INTERFACE	0x01
#define USB_RT_ENDPOINT		0x02
#define USB_RT_HIDD		(USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT)
#define USB_RT_HIDD_IN		(USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN)	
typedef struct usb_device_request {
	uByte bmRequestType;
	uByte bRequest;
	uWord wValue;
	uWord wIndex;
	uWord wLength;
} usb_device_request_t;

#define SIZEOF_DEVICE_DESC  18
typedef struct usb_device_descriptor {
	uByte bLength;
	uByte bDescriptorType;
	uWord bcdUSB;
	uByte bDeviceClass;
	uByte bDeviceSubClass;
	uByte bDeviceProtocol;
	uByte bMaxPacketSize;
	uWord idVendor;
	uWord idProduct;
	uWord bcdDevice;
	uByte iManufacturer;
	uByte iProduct;
	uByte iSerialNumber;
	uByte bNumConfigurations;
} usb_device_descriptor_t;

#define SIZEOF_CONFIG_DESC 9
typedef struct usb_config_descriptor {
	uByte bLength;
	uByte bDescriptorType;
	uWord wTotalLength;
	uByte bNumInterface;
	uByte bConfigurationValue;
	uByte iConfiguration;
	uByte bmAttributes;
	/* max current in 2 mA units */
	uByte bMaxPower;
} usb_config_descriptor_t;


typedef struct usb_device_qualifier {
	uByte bLength;
	uByte bDescriptorType;
	uWord bcdUSB;
	uByte bDeviceClass;
	uByte bDeviceSubClass;
	uByte bDeviceProtocol;
	uByte bMaxPacketSize0;
	uByte bNumConfigurations;
	uByte bReserved;
} usb_device_qualifier_t;

#define SIZEOF_INTERFACE_DESC 9
typedef struct usb_interface_descriptor {
	uByte bLength;
	uByte bDescriptorType;
	uByte bInterfaceNumber;
	uByte bAlternateSetting;
	uByte bNumEndpoints;
	uByte bInterfaceClass;
	uByte bInterfaceSubClass;
	uByte bInterfaceProtocol;
	uByte iInterface;
} usb_interface_descriptor_t;

#define SIZEOF_ENDPOINT_DESC 7
typedef struct usb_endpoint_descriptor {
	uByte bLength;
	uByte bDescriptorType;
	uByte bEndpointAddress;
	uByte bmAttributes;
	uWord wMaxPacketSize;
	uByte bInterval;
} usb_endpoint_descriptor_t;

#define USB_MAX_STRING_LEN 128
typedef struct usb_string_descriptor {
	uByte bLength;
	uByte bDescriptorType;
	uByte bString[126];
} usb_string_descriptor_t;


#endif				/* _USB_H_ */
