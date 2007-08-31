
#
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/test/Makefile,v 1.2 2007-08-31 15:18:47 m_fischer Exp $
#

CC = gcc

VER = d
COPTS = -Wall -g

GTKINCL=`pkg-config --cflags gtk+-2.0`
LIBS=`pkg-config --libs gtk+-2.0`

CFLAGS = $(COPTS) 

EXTLIBS =

# this is the original list of tests
OLDTESTS = alarmtst boxtest btest colortst draw fred listtest

# these tests are updated and work with current library
TESTS= alarmtst boxtest colortst testapp

CSRCS = alarmtst.c boxtest.c btest.c colortst.c draw.c fred.c listtest.c
HDRS =
BMPS = fred.bmp bits.bmp dtest.bmp
MISC = Makefile
MSWSRCS = alarmtst.mak boxtest.mak btest.mak colortst.mak draw.mak \
	fred.mak listtest.mak \
	wtest.mak wtest.rc wtest.def 
BINS = wtest.ico

SRCS = $(MISC) $(CSRCS) $(HDRS) $(BMPS) $(MSWSRCS)

.c: 
	$(CC) -c -I../include $(CFLAGS) $(CINCLS) $<
	$(CC) -o $@ $@.o ../gtklib/gtk2libd.a $(LIBS)
	rm -f $@.o

all:	$(TESTS) ../gtklib/gtk2libd.a

clean:
	rm -f $(TESTS) *.o
tar:
	tar cvf gtklib.tar $(SRCS)

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(SRCS) ; fi

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ; \
	co $$TAG  $(SRCS)

src:
	@echo $(SRCS) $(BINS)

rcssrc:
	@echo $(SRCS)

