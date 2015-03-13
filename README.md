<h1> MINIX 3 USB STACK <h1>

Project is aimed at setting up USB stack for Minix 3.1.5 and later.

Version 0.01-alpha <b>(Release:17-March-2010)</b> Only tested on 3.1.5 Qemu

<h3>Features</h3>

* Low/Full speed support
* Control Transfers (Synchronous)
* Interrupt Transfers
* UHCI host controller driver
* Dummy HID keyboard and mouse driver that works on boot protocol
* USBDI interface for usb device drivers
* USBDI support HID class driver ie HID requests from chap 7 HID specs 1.11
* Multiple host controller support
* Dynamic loading/unloading drivers even HCDs (Minix Specific)
* Reliable Modular design

<h3>Limitations</h3>

* No hub driver
* Device limited to 2 root hub ports (more are supported within stack,limitation is due to no hub driver)
* Bulk transfer
* Isochronous transfer
* Synchronous control transfer
* Single driver Single device ie only one driver can claim a device
* Only tested on Linux Host with qemu 0.11.1
