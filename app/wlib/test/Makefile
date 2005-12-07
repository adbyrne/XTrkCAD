
#
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/test/Makefile,v 1.1 2005-12-07 15:48:39 rc-flyer Exp $
#

CC = gcc

VER = d
COPTS = -Wall -g
CINCLS = -I../include -I/usr3/downloads/src/gtk/gtk+-1.2.10 -I/usr3/downloads/src/gtk/glib-1.2.10
CFLAGS = $(COPTS) $(CINCLS)
EXTLIBS =

TESTS = alarmtst boxtest btest colortst draw fred listtest
CSRCS = alarmtst.c boxtest.c btest.c colortst.c draw.c fred.c listtest.c
HDRS =
BMPS = fred.bmp bits.bmp dtest.bmp
MISC = Makefile
MSWSRCS = alarmtst.mak boxtest.mak btest.mak colortst.mak draw.mak \
	fred.mak listtest.mak \
	wtest.mak wtest.rc wtest.def 
BINS = wtest.ico

#GTKDIR = /usr/src/x/gtk/gtk+-1.0.6

SRCS = $(MISC) $(CSRCS) $(HDRS) $(BMPS) $(MSWSRCS)

#LIBS = \
#	../gtklib/gtklib$(VER).a \
#	$(GTKDIR)/gtk/.libs/libgtk.a \
#	$(GTKDIR)/gdk/.libs/libgdk.a \
#	$(GTKDIR)/glib/.libs/libglib.a \
#	-lXext -lX11 \
#	-lm
LIBS = ../gtklib/gtklibd.a -lgtk -lgdk -lglib -L/usr/X11R6/lib -lX11 -lm

.c: ../gtklib/gtklib$(VER).a
	$(CC) -c -I../include $(CFLAGS) $(COPTS) $(CINCLS) $<
	$(CC) -o $@ $@.o $(LIBS) $(EXTLIBS)
	rm -f $@.o

comp:
	$(CC) -I../include $(COPTS) $(CINCLS) -c $(PROG).c
	$(CC) -o $(PROG) $(PROG).o $(LIBS)

all:
	$(MAKE) VER="$(VER)" CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS)" $(TESTS) EXTLIBS="$(EXTLIBS)"

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
