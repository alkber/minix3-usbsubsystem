* MINIX 3 USB STACK *

* USB stack for Minix 3.1.5 and later.
* Version 0.01-alpha <b>(Release:17-March-2010)</b> 
* Only tested on Minux 3.1.5 running on Qemu

* Supported features *

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


 ![1](http://lh3.ggpht.com/_fEmFcVTSPyk/S6Evr_1CFVI/AAAAAAAAAYw/6gCZSzmEC20/Screenshot-34.png)
 ![1](http://lh4.ggpht.com/_fEmFcVTSPyk/S6EwI1dHi9I/AAAAAAAAAY0/fgheiod-Kws/Screenshot-35.png)
 ![1](http://lh6.ggpht.com/_fEmFcVTSPyk/S6Eq23OBXMI/AAAAAAAAAYY/tiDiVf-2wos/Screenshot-31.png)
 ![1](http://lh5.ggpht.com/_fEmFcVTSPyk/S6EtWc9eRxI/AAAAAAAAAYo/c4b3TRgS8m4/Screenshot-33.png)
 ![1](http://lh4.ggpht.com/_fEmFcVTSPyk/S6Eq3M9iKnI/AAAAAAAAAYg/4JT3eIzU8-8/Screenshot-29.png)
 ![1](http://lh6.ggpht.com/_fEmFcVTSPyk/S6Eq3LzU7gI/AAAAAAAAAYc/PEwSsWc3D3o/Screenshot-30.png) 
