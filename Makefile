# $Id: Makefile,v 1.4 1995/05/21 20:27:25 alfie Exp alfie $

BINDIR=/usr/sbin
MANDIR=/usr/man
MANEXT=8

# Compiled defaults to override swapd.h -- add to CFLAGS to taste
#  -DTMPDIR=\"/var/tmp\"	Directory to create swap files in
#  -DPRIORITY=-4		Nice level for daemon to run at
#  -DINTERVAL=30		Interval to check free VM
#  -DNUMCHUNKS=8		Maximum number of swap files
#  -DCHUNKSZ=4*1024*1024       	Size of each swap file
#  -DLOWER=CHUNKSZ/2		Lower limit for VM
#  -DUPPER=CHUNKSZ		Upper limit for VM

CFLAGS  = -O2 -Wall
LDFLAGS = -s

swapd: swapd.o

swapd.o: swapd.c swapd.h

install: swapd swapd.man
	install -s -m 0755 swapd $(BINDIR)
	install -c -m 0644 swapd.man $(MANDIR)/man$(MANEXT)/swapd.$(MANEXT)

clobber: clean
	rm -f swapd swapd.tar.gz

clean:
	rm -f *.o core
