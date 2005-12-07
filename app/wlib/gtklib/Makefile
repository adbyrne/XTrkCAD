#
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/Makefile,v 1.1 2005-12-07 15:48:42 rc-flyer Exp $
#

#GTKDIR=/usr/src/x/gtk/gtk+-1.0.6
GTK1INCL=`gtk-config --cflags`
GTK2INCL=-I/usr/include/gtk-2.0 -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo

COPTS = -Wall
CINCLS = -I../include $(GTKINCL)

all: debug2

#OBJS = gtkmisc.o gtklist.o gtksimple.o gtksingle.o gtktext.o gtkmenu.o gtkdraw.o \
#	gtkfont.o gtkfilsel.o wpref.o psprint.o
OBJS = \
	gtkbutton.o \
	gtkcolor.o \
	gtkdraw.o \
	gtklist.o \
	gtkfilsel.o \
	gtkfont.o \
	gtkhelp.o \
	gtkmenu.o \
	gtkmisc.o \
	gtksimple.o \
	gtksingle.o \
	gtkwindow.o \
	gtktext.o \
	wpref.o \
	psprint.o 


LIBS = -lgtk -lgdk -lglib -lXext -lX11 -lm
#LIBS = $(GTKDIR)/gtk/.libs/libgtk.a  $(GTKDIR)/gdk/.libs/libgdk.a $(GTKDIR)/glib/.libs/libglib.a -lXext -lX11 -lm

CSRCS = $(OBJS:.o=.c)
HDRS = gtkint.h dynarr.h
BMPS = bold.bmp italic.bmp square10.bmp
MISC = Makefile
SRCS = $(MISC) $(CSRCS) $(HDRS) $(BMPS)

.c.a:
	@echo $<
	$(CC) -c $(CFLAGS) $(COPTS) -DEXPORT= -I.. $(CINCLS) $<
	@ar rv $@ $*.o
	@rm -f $*.o

LIBOBJS = \
	$(LIB)(gtkbutton.o) \
	$(LIB)(gtkcolor.o) \
	$(LIB)(gtkdraw.o) \
	$(LIB)(gtkfilsel.o) \
	$(LIB)(gtkfont.o) \
	$(LIB)(gtkhelp.o) \
	$(LIB)(gtklist.o) \
	$(LIB)(gtkmenu.o) \
	$(LIB)(gtkmisc.o) \
	$(LIB)(gtksimple.o) \
	$(LIB)(gtksingle.o) \
	$(LIB)(gtktext.o) \
	$(LIB)(gtkwindow.o) \
	$(LIB)(wpref.o) \
	$(LIB)(psprint.o)

#	$(LIB)(gtklist.o) \
#	$(LIB)(gtksimple.o) \
#	$(LIB)(gtksingle.o) \
#	$(LIB)(gtktext.o) \
#	$(LIB)(gtkdraw.o) \
#	$(LIB)(gtkmenu.o) \
#	$(LIB)(gtkfont.o) \
#	$(LIB)(gtkfilsel.o) \

$(LIB) : $(LIBOBJS)
	ranlib $(LIB)

update: product debug 

update2: product2 debug2

debug:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK1INCL)" CFLAGS="-g -DDEBUG -DGTK1" VER=d LIB=gtklibd.a gtklibd.a

product:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK1INCL)" CFLAGS="-O2 -DFASTRACK -DGTK1" VER= LIB=gtklib.a gtklib.a

profile:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK1INCL)" CFLAGS="-pg -O2 -DFASTRACK -DGTK1" VER=p LIB=gtklibp.a gtklibp.a

checked:
	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK1INCL)" CFLAGS="-O2 -DFASTRACK -DGTK1" VER=c LIB=gtklibc.a gtklibc.a

debug2:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK2INCL)" CFLAGS="-g -DDEBUG" VER=d LIB=gtk2libd.a gtk2libd.a

product2:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK2INCL)" CFLAGS="-O2 -DFASTRACK" VER= LIB=gtk2lib.a gtk2lib.a

profile2:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK2INCL)" CFLAGS="-pg -O2 -DFASTRACK" VER=p LIB=gtk2libp.a gtk2libp.a

checked2:
	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTK2INCL)" CFLAGS="-O2 -DFASTRACK" VER=c LIB=gtk2libc.a gtk2libc.a

gtkfilsel: gtkfilsel.c
	$(CC) $(CFLAGS) $(CINCLS) -g -DTEST gtkfilsel.c
	$(CC) -o gtkfilsel gtkfilsel.o xtrkcadd.a $(LIBS)

gtkfont: gtkfont.c
	$(CC) -c $(CFLAGS) $(CINCLS) -g -DTEST gtkfont.c ../utility.c
	$(CC) -o gtkfont gtklib.a $(LIBS)
	rm -f gtkfont.o utility.o

test1: test1.c debug
	$(CC) -c $(CFLAGS) $(CINCLS) -g test1.c
	$(CC) -o test1 test1.o gtklibd.a $(LIBS)
	rm -f test1.o

.c.o:
	$(CC) $(CFLAGS) -c $<

tar:
	tar cvf gtklib.tar $(SRCS)

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(SRCS) ; fi

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ; \
	co $$TAG  $(SRCS)

src:
	@echo $(SRCS)

rcssrc:
	@echo $(SRCS)

clean:
	rm -f *.a
