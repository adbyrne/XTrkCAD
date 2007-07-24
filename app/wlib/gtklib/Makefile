#
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/Makefile,v 1.5 2007-07-24 16:14:53 m_fischer Exp $
#

GTKINCL=`pkg-config --cflags gtk+-2.0`

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
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-g -DDEBUG -DGTK1" VER=d LIB=gtklibd.a gtklibd.a

product:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-DFASTRACK -DGTK1" VER= LIB=gtklib.a gtklib.a
#	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-O2 -DFASTRACK -DGTK1" VER= LIB=gtklib.a gtklib.a

profile:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-pg -DFASTRACK -DGTK1" VER=p LIB=gtklibp.a gtklibp.a
#	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-pg -O2 -DFASTRACK -DGTK1" VER=p LIB=gtklibp.a gtklibp.a

checked:
	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-DFASTRACK -DGTK1" VER=c LIB=gtklibc.a gtklibc.a
#	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-O2 -DFASTRACK -DGTK1" VER=c LIB=gtklibc.a gtklibc.a

debug2:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-g -DDEBUG" VER=d LIB=gtk2libd.a gtk2libd.a

product2:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-DFASTRACK" VER= LIB=gtk2lib.a gtk2lib.a
#	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-O2 -DFASTRACK" VER= LIB=gtk2lib.a gtk2lib.a

profile2:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-pg -DFASTRACK" VER= LIB=gtk2libp.a gtk2libp.a
#	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-O2 -DFASTRACK" VER= LIB=gtk2lib.a gtk2lib.a

checked2:
	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-DFASTRACK" VER=c LIB=gtk2libc.a gtk2libc.a
#	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS) $(GTKINCL)" CFLAGS="-O2 -DFASTRACK" VER=c LIB=gtk2libc.a gtk2libc.a

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



$(LIB)(gtkbutton.o): gtkbutton.c gtkint.h
$(LIB)(gtkcolor.o): gtkcolor.c gtkint.h square10.bmp
$(LIB)(gtkdraw.o): gtkdraw.c gtkint.h /usr/include/gtk-2.0/gdk/gdkkeysyms.h
$(LIB)(gtkfilsel.o): gtkfilsel.c gtkint.h
$(LIB)(gtkfont.o): gtkfont.c /usr/include/X11/Xlib.h ../include/wlib.h gtkint.h bold.bmp italic.bmp
$(LIB)(gtkhelp.o): gtkhelp.c gtkint.h
$(LIB)(gtklist.o): gtklist.c gtkint.h
$(LIB)(gtkmenu.o): gtkmenu.c gtkint.h
$(LIB)(gtkmisc.o): gtkmisc.c gtkint.h
$(LIB)(gtksimple.o): gtksimple.c gtkint.h
$(LIB)(gtksingle.o): gtksingle.c gtkint.h
$(LIB)(gtktext.o): gtktext.c gtkint.h
$(LIB)(gtkwindow.o): gtkwindow.c gtkint.h
$(LIB)(psprint.o): psprint.c ../include/wlib.h dynarr.h
$(LIB)(wpref.o): wpref.c ../include/wlib.h dynarr.h
