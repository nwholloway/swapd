# $Id: Makefile,v 1.2 1995/02/12 22:36:00 alfie Exp alfie $

BINDIR=/usr/sbin
MANDIR=/usr/man
MANEXT=8

# Compiled defaults to override swapd.h -- add to CFLAGS to taste
#  -DTMPDIR=\"/var/tmp\"
#  -DPRIORITY=-4
#  -DINTERVAL=30
#  -DNUMCHUNKS=8
#  -DCHUNKSZ=2*1024*1024
#  -DLOWER=CHUNKSZ/2
#  -DUPPER=CHUNKSZ

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
