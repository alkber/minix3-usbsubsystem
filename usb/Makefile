#
# Makefile for the hello UHCIHCD.
#
UHCIHCD = uhci-hcd
USBD= usbd
USBKBD= usbkbd
USBMS= usbms
#
# Directories.
#
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

#
# Build Programs, Flags and Variables.
#
CC      = exec cc
CFLAGS  = -I$i $(CPROFILE)
LDFLAGS = -i -L../libdriver
LIBS    = -ldriver -lsys

UHCIHCDOBJ    = uhci-hcd.o usbdmem.o
USBDOBJ       = usbd.o usbdmem.o
USBKBDOBJ     = usbkbd.o usbdmem.o usbdi.o
USBMSOBJ	  = usbms.o usbdmem.o usbdi.o

# build local binary
all build:   $(UHCIHCD) $(USBD) $(USBKBD) $(USBMS)

$(UHCIHCD):  $(UHCIHCDOBJ) 
	$(CC) -v -o $@  $(LDFLAGS) $(UHCIHCDOBJ) $(LIBS)
	install -S 128k $(UHCIHCD)

$(USBD):  $(USBDOBJ) 
	$(CC) -v -o $@  $(LDFLAGS) $(USBDOBJ) $(LIBS)
	install -S 128k $(USBD)
	
$(USBKBD): $(USBKBDOBJ) 
	$(CC) -v -o $@  $(LDFLAGS) $(USBKBDOBJ) $(LIBS)
	install -S 128k $(USBKBD)
	
$(USBMS): $(USBMSOBJ) 
	$(CC) -v -o $@  $(LDFLAGS) $(USBMSOBJ) $(LIBS)
	install -S 128k $(USBMS)
	
# install with other drivers 
install: 
	install -o root -cs $(USBD) /usr/sbin/$(USBD)
	install -o root -cs $(UHCIHCD) /usr/sbin/$(UHCIHCD)
	install -o root -cs $(USBKBD) /usr/sbin/$(USBKBD)    
	install -o root -cs $(USBMS) /usr/sbin/$(USBMS)    
	   
# clean up local files
clean:
	rm -f *.o $(UHCIHCD)
	rm -f *.o $(USBD) 
	rm -f *.o $(USBKBD)
	rm -f *.o $(USBMS)
depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend
