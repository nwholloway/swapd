# $Id$

BINDIR=/usr/sbin
MANDIR=/usr/man
MANEXT=8

CCFLAGS = -O2 -Wall
LDFLAGS = -s

swapd: swapd.o
	$(CC) $(LDFLAGS) -o swapd swapd.o

swapd.o: swapd.c swapd.h
	$(CC) $(CCFLAGS) -c swapd.c

install: swapd swapd.man
	install -s -m 0755 swapd $(BINDIR)
	install -c -m 0644 swapd.man $(MANDIR)/man.$(MANEXT)/swapd.$(MANEXT)
clobber: clean
	rm -f swapd swapd.tar.gz
clean:
	rm -f *.o core
tar:
	GZIP=-9v tar zcvf swapd.tar.gz README Makefile swapd.h swapd.c swapd.man
