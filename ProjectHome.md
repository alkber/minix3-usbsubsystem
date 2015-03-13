# MINIX 3 USB STACK #

> Project is aimed at setting up USB stack for Minix 3.1.5 and later.

Version 0.01-alpha (Release:17-March-2010) Only tested on 3.1.5

## Features ##
  * Low/Full speed support
  * Control Transfers  (Synchronous)
  * Interrupt Transfers
  * UHCI host controller driver
  * Dummy HID keyboard and mouse driver that works on boot protocol
  * USBDI interface for usb device drivers
  * USBDI support HID class driver ie HID requests from chap 7 HID specs 1.11
  * Multiple host controller support
  * Dynamic loading/unloading drivers even HCDs (Minix Specific)
  * Reliable Modular design


## Limitations ##
  * No hub driver
  * Device limited to 2 root hub ports (more are supported within stack,limitation is due  to no hub  driver)
  * Bulk transfer
  * Isochronous transfer
  * Synchronous control transfer
  * Single driver Single device ie only one driver can claim a device
  * Only tested on Linux Host with qemu 0.11.1

## Screenshots ##
[Screenshots](http://code.google.com/p/minix3-usbsubsystem/wiki/Screenshotsalpha17032010)

## Draft Project Report ##
> Well, this report contain good introduction to USB basics and the stack design,please
> note the this is not the final report and may contain spelling and grammatical errors.
[Project Report Draft](http://minix3-usbsubsystem.googlecode.com/files/Project-Report-Draft-%28pdf%29.tar.bz2)