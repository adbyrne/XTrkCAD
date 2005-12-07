
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/Makefile,v 1.1 2005-12-07 15:46:51 rc-flyer Exp $
#

CC = gcc

WLIB = ../wlib
VER = d
COPTS = -Wall
CINCLS = -I$(WLIB)/include
PACKAGEDIR = ./package
VER=0.1
XXXDIR = /usr3/downloads/src/gtk/gtk+-1.2.10
#GLIBDIR = /usr/src/glib-1.2.1

all: debug

CONVERTERS = bdf2xtp

MISCOBJS = \
	ccurve.o \
	cdraw.o \
	celev.o \
	cgroup.o \
	chndldto.o \
	chotbar.o \
	cjoin.o \
	cmisc.o \
	cmisc2.o \
	cnote.o \
	compound.o \
	cparalle.o \
	cprint.o \
	cprofile.o \
	cpull.o \
	cruler.o \
	cselect.o \
	csplit.o \
	csnap.o \
	cstraigh.o \
	cstruct.o \
	ctext.o \
	ctodesgn.o \
	ctrain.o \
	cturnout.o \
	cturntbl.o \
	cundo.o \
	dbench.o \
	dbitmap.o \
	dcar.o \
	dcmpnd.o \
	dcustmgm.o \
	dease.o \
	denum.o \
	dlayer.o \
	doption.o \
	dpricels.o \
	dprmfile.o \
	draw.o \
	drawgeom.o \
	elev.o \
	fileio.o \
	lprintf.o \
	macro.o \
	misc.o \
	misc2.o \
	param.o \
	shrtpath.o \
	tcurve.o \
	tease.o \
	track.o \
	trkseg.o \
	tstraigh.o \
	utility.o

OBJS = $(MISCOBJS)

LIB = xtrkcad$(VER).a
.PRECIOUS: $(LIB)

WLIBS = $(WLIB)/gtklib/gtk2lib$(VER).a

GTKLIBS = -lgtk-x11-2.0 -lgdk-x11-2.0 -lglib-2.0
XLIBS = -L/usr/X11R6/lib -lX11

LIBS = $(WLIBS) $(GTKLIBS) $(XLIBS) -lm


CSRCS = $(OBJS:.o=.c) custom.c
HDRS = \
	compound.h \
	ccurve.h \
	cjoin.h \
	cmodify.c \
	cselect.h \
	cstraigh.h \
	ctrain.h \
	cundo.h \
	custom.h \
	common.h \
	misc.h \
	draw.h \
	drawgeom.h \
	track.h \
	trackx.h \
	utility.h \
	version.h \
	acclkeys.h \
	misc2.h \
	param.h \
	fileio.h \
	shrtpath.h

XPMS = \
	above.xpm below.xpm \
	bo_edge.xpm bo_flat.xpm \
	bo_ll.xpm bo_lld.xpm bo_lli.xpm bo_llu.xpm \
	bo_lr.xpm bo_lrd.xpm bo_lri.xpm bo_lru.xpm \
	bo_t.xpm bo_ti.xpm bo_tl.xpm bo_tr.xpm \
	carpart.xpm carproto.xpm \
	circle1.xpm circle2.xpm circle3.xpm \
	cnote.xpm \
	curve1.xpm curve2.xpm curve3.xpm curve4.xpm \
	dbench.xpm dbox.xpm \
	dcircle1.xpm dcircle2.xpm dcircle3.xpm \
	dcurve1.xpm dcurve2.xpm dcurve3.xpm dcurve4.xpm \
	ddimlin.xpm \
	delete.xpm \
	describe.xpm \
	dfilbox.xpm dfilpoly.xpm \
	dflcrcl1.xpm dflcrcl2.xpm dflcrcl3.xpm \
	dline.xpm dpoly.xpm dtbledge.xpm \
	elev.xpm \
	ebroad.xpm egtbroad.xpm egtsharp.xpm eltbroad.xpm enone.xpm enormal.xpm esharp.xpm \
	exit.xpm \
	export.xpm \
	extend.xpm \
	flip.xpm \
	go.xpm \
	helix.xpm \
	hndldto.xpm \
	import.xpm \
	join.xpm \
	move.xpm \
	movedesc.xpm \
	openbutt.xpm \
	newcar.xpm \
	parallel.xpm \
	partlist.xpm \
	profile.xpm \
	pull.xpm \
	redo.xpm \
	rotate.xpm \
	ruler.xpm \
	select.xpm \
	snapcurs.xpm snapvis.xpm \
	splittrk.xpm \
	stop.xpm \
	straight.xpm \
	struct.xpm \
	text.xpm \
	train.xpm \
	turnout.xpm turntbl.xpm \
	tunnel.xpm \
	undo.xpm \
	zero.xpm \
	zoom.xpm zoomin.xpm zoomout.xpm \
	xtc64.xpm


BMPS = xtrkcad.bmp \
	bigdot.bmp \
	bma0.bmp bma135.bmp bma45.bmp bma90.bmp bmendpt.bmp \
	note.bmp arrow0.bmp arrow3.bmp arrows.bmp flash.bmp \
	triangl.bmp triangr.bmp triangu.bmp triangd.bmp cross0.bmp \
	k1.bmp k2.bmp k3.bmp k4.bmp k5.bmp k6.bmp k7.bmp k7.bmp k8.bmp k9.bmp k10.bmp \
	k11.bmp k12.bmp k13.bmp k14.bmp k15.bmp k16.bmp k17.bmp k17.bmp k18.bmp k19.bmp k20.bmp \
	l1.bmp l2.bmp l3.bmp l4.bmp l5.bmp l6.bmp l7.bmp l7.bmp l8.bmp l9.bmp l10.bmp \
	l11.bmp l12.bmp l13.bmp l14.bmp l15.bmp l16.bmp l17.bmp l17.bmp l18.bmp l19.bmp l20.bmp \
	square10.bmp \
	hotbarl.bmp hotbarr.bmp \
	mtbox.bmp chkbox.bmp \
	snapcurs.bmp snapvis.bmp bridge.bmp xtc16.bmp xtc64.bmp

MISC = cnvdsgn.c bdf2xtp.c 
TURNOUTSRCS = to3way.src todcross.src torcross.src tostrsct.src \
	tocrv.src todslip.src toreg.src towye.src \
	tocrvsct.src tolcross.src tosslip.src toxing.src
TURNOUTLINS = to3way.lin todcross.lin torcross.lin tostrsct.lin \
	tocrv.lin todslip.lin toreg.lin towye.lin \
	tocrvsct.lin tolcross.lin tosslip.lin toxing.lin
EXAMPLES = examples/mr03.trk

MSWSRCS = xtrkcad.def xtrkcad.rc xtrkcad.mak xtrkcad.dsp xtrkcad.dsw
PRODS=$(TURNOUTLINS) $(SHROUDFILES) bllnhlp.c
SRCS = $(MISC) $(CSRCS) $(HDRS) $(BMPS) $(XPMS) $(TURNOUTSRCS) $(MSWSRCS) Makefile $(LIB) $(LIB)

#$(VER)xvlib:
#	cd xvlib; $(MAKE) CC="$(CC)" COPTS="$(COPTS)" VER="$(VER)"

.c.a:
	@echo $<
	$(CC) -c $(CFLAGS) $(COPTS) $(CINCLS) $<
	@ar rv $@ $*.o
	@rm -f $*.o

.src.lin:
	@echo $<
	./cnvdsgn < $*.src > $*.lin

LIBOBJS = \
	$(LIB)(bllnhlp.o) \
	$(LIB)(ccurve.o) \
	$(LIB)(cdraw.o) \
	$(LIB)(celev.o) \
	$(LIB)(cgroup.o) \
	$(LIB)(chndldto.o) \
	$(LIB)(chotbar.o) \
	$(LIB)(cjoin.o) \
	$(LIB)(cmisc.o) \
	$(LIB)(cmisc2.o) \
	$(LIB)(cmodify.o) \
	$(LIB)(cnote.o) \
	$(LIB)(compound.o) \
	$(LIB)(cparalle.o) \
	$(LIB)(cprint.o) \
	$(LIB)(cprofile.o) \
	$(LIB)(cpull.o) \
	$(LIB)(cruler.o) \
	$(LIB)(cselect.o) \
	$(LIB)(csnap.o) \
	$(LIB)(csplit.o) \
	$(LIB)(cstraigh.o) \
	$(LIB)(cstruct.o) \
	$(LIB)(ctext.o) \
	$(LIB)(ctodesgn.o) \
	$(LIB)(ctrain.o) \
	$(LIB)(cturnout.o) \
	$(LIB)(cturntbl.o) \
	$(LIB)(cundo.o) \
	$(LIB)(dbench.o) \
	$(LIB)(dbitmap.o) \
	$(LIB)(dcar.o) \
	$(LIB)(dcmpnd.o) \
	$(LIB)(dcustmgm.o) \
	$(LIB)(dease.o) \
	$(LIB)(denum.o) \
	$(LIB)(dlayer.o) \
	$(LIB)(dprmfile.o) \
	$(LIB)(doption.o) \
	$(LIB)(dpricels.o) \
	$(LIB)(draw.o) \
	$(LIB)(drawgeom.o) \
	$(LIB)(elev.o) \
	$(LIB)(fileio.o) \
	$(LIB)(lprintf.o) \
	$(LIB)(macro.o) \
	$(LIB)(misc.o) \
	$(LIB)(misc2.o) \
	$(LIB)(param.o) \
	$(LIB)(shrtpath.o) \
	$(LIB)(tcurve.o) \
	$(LIB)(tease.o) \
	$(LIB)(track.o) \
	$(LIB)(trkseg.o) \
	$(LIB)(tstraigh.o) \
	$(LIB)(utility.o)


$(LIB) : $(LIBOBJS)
	ranlib $(LIB)

update: debug product #libc5-debug libc5-product

debug:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS)" CFLAGS="-g -DDEBUG" LDFLAGS= VER=d GTKVER="2" GTKLIBS="$(GTKLIBS)" xtrkcadd mkturnout #$(CONVERTERS)

staticgtklib:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS)" CFLAGS="-g -DDEBUG" LDFLAGS= VER=d GTKLIBS="$(GTKSTATICLIBS)" xtrkcadd-static mkturnout #$(CONVERTERS)

product:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS)" CFLAGS="-O2 -DFASTRACK" LDFLAGS= VER="" GTKVER="2" GTKLIBS="$(GTKLIBS)" xtrkcad #$(CONVERTERS)

profile:
	$(MAKE) CC="$(CC)" COPTS="$(COPTS)" CINCLS="$(CINCLS)" CFLAGS="-pg -O2 -DFASTRACK" LDFLAGS="-pg" VER=p xtrkcadp #$(CONVERTERS)


checked:
	$(MAKE) CC="checkergcc" COPTS="$(COPTS)" CINCLS="$(CINCLS)" CFLAGS="-g -DDEBUG" LDFLAGS= VER=c xtrkcadc $(CONVERTERS)

mkpackage:
	-rm -fr $(PACKAGEDIR)
	mkdir $(PACKAGEDIR)
	mkdir $(PACKAGEDIR)/xtrkcad
	mkdir $(PACKAGEDIR)/xtrkcad/examples
	cp xtrkcad $(HELPFILES) $(PARAMS) $(PACKAGEDIR)/xtrkcad/
	strip $(PACKAGEDIR)/xtrkcad/xtrkcad
	cp examples/* $(PACKAGEDIR)/xtrkcad/examples/
	cp AAAREADME $(PACKAGEDIR)/xtrkcad/
	cd $(PACKAGEDIR)/; tar cvfz ../xtrkcad-$(VER).tar.gz ./xtrkcad/
	(cat AAAREADME ; uuencode xtrkcad-$(VER).tar.gz < xtrkcad-$(VER).tar.gz ) > xtrkcad-$(VER).tar.gz.u
	csplit -k -f xtrkcad-0.1.tar.gz.u. xtrkcad-0.1.tar.gz.u 1000 \{999\}

xtrkcad$(VER): $(LIB) xtrkcad$(VER).o $(WLIBS)
	ar xv $(LIB) misc.o
	$(CC) $(LDFLAGS) -o xtrkcad$(VER) misc.o xtrkcad$(VER).o $(LIB) $(LIBS)
	rm -f misc.o
	if [ "$(VER)"x = x ] ; then cp xtrkcad xtrkcad-ns; strip xtrkcad; fi

xtrkcad$(VER)-static: $(LIB) xtrkcad$(VER).o
	ar xv $(LIB) misc.o
	$(CC) $(LDFLAGS) -o xtrkcad$(VER)-static misc.o xtrkcad$(VER).o $(LIB) $(LIBS) -lXext
	rm -f misc.o

xtrkcad$(VER).o: custom.c
	$(CC) -c $(CFLAGS) $(COPTS) $(CINCLS) -o xtrkcad$(VER).o custom.c

wintest: wintest.o xvdraw.o
	$(CC) -o wintest wintest.o xvdraw.o $(LIBS)

xvfilsel: xvfilsel.c
	$(CC) $(CFLAGS) $(CINCLS) -g -DTEST xvfilsel.c
	$(CC) -o xvfilsel xvfilsel.o xtrkcadd.a $(LIBS)

xvfont: xvfont.c
	$(CC) -c $(CFLAGS) $(CINCLS) -g -DTEST xvfont.c
	ar xv xtrkcadd.a xvcmd.o xvdraw.o utility.o
	$(CC) -o xvfont xvfont.o xvcmd.o xvdraw.o utility.o $(LIBS)
	rm -f xvcmd.o xvdraw.o utility.o

#.c.o:
#	$(CC) $(CFLAGS) -c $<

tar:
#	cd mswlib; make tar
#	cd xvlib; make tar
	tar cvf track.tar $(SRCS) $(PRODS) #mswlib/mswlib.tar xvlib/xvlib.tar

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(SRCS) ; fi

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ;\
	co $$TAG  $(SRCS)

src:
	@echo $(SRCS) $(PRODS)

bmps:
	@echo $(BMPS)

xpms:
	@echo $(XPMS)

rcssrc:
	@echo $(SRCS)

depend:

$(LIB)(ctodesgn.o): $(TURNOUTLINS)

$(LIB)(fileio.o):

cnvdsgn: cnvdsgn.c
	cc -o cnvdsgn cnvdsgn.c utility.c -lm

toreg.lin: cnvdsgn toreg.src
	./cnvdsgn < toreg.src > toreg.lin
tocrv.lin: cnvdsgn tocrv.src
	./cnvdsgn < tocrv.src > tocrv.lin
towye.lin: cnvdsgn towye.src
	./cnvdsgn < towye.src > towye.lin
to3way.lin: cnvdsgn to3way.src
	./cnvdsgn < to3way.src > to3way.lin
toxing.lin: cnvdsgn toxing.src
	./cnvdsgn < toxing.src > toxing.lin
tosslip.lin: cnvdsgn tosslip.src
	./cnvdsgn < tosslip.src > tosslip.lin
todslip.lin: cnvdsgn todslip.src
	./cnvdsgn < todslip.src > todslip.lin
torcross.lin: cnvdsgn torcross.src
	./cnvdsgn < torcross.src > torcross.lin
tolcross.lin: cnvdsgn tolcross.src
	./cnvdsgn < tolcross.src > tolcross.lin
todcross.lin: cnvdsgn todcross.src
	./cnvdsgn < todcross.src > todcross.lin
tostrsct.lin: cnvdsgn tostrsct.src
	./cnvdsgn < tostrsct.src > tostrsct.lin
tocrvsct.lin: cnvdsgn tocrvsct.src
	./cnvdsgn < tocrvsct.src > tocrvsct.lin

mkturnout: ctodesgn.c utility.c
	cc -g -o mkturnout $(CINCLS) -DMKTURNOUT ctodesgn.c utility.c -lm

misc.o: misc.c messages.inc messages.h

clean:
	rm -f $(OBJS) $(TURNOUTLINS) $(LIB) xtrkcad xtrkcad-ns xtrkcadd mkturnout cnvdsgn $(MISCOBJS)
	rm -f xtrkcad.a xtrkcadd.a
	rm -f xtrkcad.o xtrkcadd.o messages.h bllnhlp.c 
	rm -f *.exe
