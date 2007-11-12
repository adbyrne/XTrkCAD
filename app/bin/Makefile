
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/Makefile,v 1.6 2007-11-12 18:53:14 m_fischer Exp $
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
	smalldlg.o \
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

#GTKLIBS = -lgtk-x11-2.0 -lgdk-x11-2.0 -lglib-2.0 -lpango-1.0
GTKLIBS = `pkg-config --libs libgtkhtml-2.0`
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
	bitmaps/l1.xbm bitmaps/l2.xbm bitmaps/l3.xbm bitmaps/l4.xbm \
	bitmaps/l5.xbm bitmaps/l6.xbm bitmaps/l7.xbm bitmaps/l8.xbm \
	bitmaps/l9.xbm bitmaps/l10.xbm bitmaps/l11.xbm bitmaps/l12.xbm \
	bitmaps/l13.xbm bitmaps/l14.xbm bitmaps/l15.xbm bitmaps/l16.xbm \
	bitmaps/l17.xbm bitmaps/l18.xbm bitmaps/l19.xbm bitmaps/l20.xbm \
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
	$(LIB)(smalldlg.o) \
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

#$(OBJS):	$(HDRS)

#$(LIBOBJS):	$(HDRS)


$(LIB)(bllnhlp.o): bllnhlp.c $(WLIB)/include/wlib.h
$(LIB)(ccurve.o): ccurve.c track.h ccurve.h cstraigh.h cjoin.h helix.xpm curve1.xpm curve2.xpm \
	curve3.xpm curve4.xpm circle1.xpm circle2.xpm circle3.xpm
$(LIB)(cdraw.o): cdraw.c track.h ccurve.h drawgeom.h dline.xpm ddimlin.xpm dbench.xpm \
	dtbledge.xpm dcurve1.xpm dcurve2.xpm dcurve3.xpm dcurve4.xpm \
	dcircle2.xpm dcircle3.xpm dflcrcl2.xpm dflcrcl3.xpm dbox.xpm \
	dfilbox.xpm dpoly.xpm dfilpoly.xpm
$(LIB)(celev.o): celev.c track.h cselect.h elev.xpm
$(LIB)(cgroup.o): cgroup.c track.h compound.h shrtpath.h
$(LIB)(chndldto.o): chndldto.c track.h ccurve.h cstraigh.h cjoin.h compound.h hndldto.xpm
$(LIB)(chotbar.o): chotbar.c track.h compound.h hotbarl.bmp hotbarr.bmp
$(LIB)(cjoin.o): cjoin.c track.h ccurve.h cstraigh.h cjoin.h join.xpm
$(LIB)(cmisc.o): cmisc.c track.h describe.xpm
$(LIB)(cmisc2.o): cmisc2.c track.h bridge.bmp
$(LIB)(cmodify.o): cmodify.c track.h cjoin.h ccurve.h cstraigh.h extend.xpm
$(LIB)(cnote.o): cnote.c track.h note.bmp cnote.xpm

cnvdsgn.o: cnvdsgn.c common.h utility.h

$(LIB)(compound.o): compound.c track.h compound.h shrtpath.h cjoin.h
$(LIB)(cparalle.o): cparalle.c track.h ccurve.h cstraigh.h parallel.xpm
$(LIB)(cprint.o): cprint.c track.h
$(LIB)(cprofile.o): cprofile.c track.h cselect.h shrtpath.h profile.xpm
$(LIB)(cpull.o): cpull.c track.h cselect.h compound.h pull.xpm
$(LIB)(cruler.o): cruler.c track.h ruler.xpm
$(LIB)(cselect.o): cselect.c track.h ccurve.h compound.h bmendpt.bmp bma0.bmp bma45.bmp \
	bma90.bmp bma135.bmp select.xpm delete.xpm tunnel.xpm move.xpm \
	rotate.xpm flip.xpm movedesc.xpm
$(LIB)(csnap.o): csnap.c track.h cross0.bmp bigdot.bmp snapcurs.bmp snapvis.bmp
$(LIB)(csplit.o): csplit.c track.h splittrk.xpm
$(LIB)(cstraigh.o): cstraigh.c track.h cstraigh.h straight.xpm
$(LIB)(cstruct.o): cstruct.c track.h compound.h struct.xpm
$(LIB)(ctext.o): ctext.c track.h text.xpm
$(LIB)(ctodesgn.o): ctodesgn.c track.h ccurve.h cstraigh.h compound.h toreg.lin tocrv.lin \
	towye.lin to3way.lin toxing.lin tosslip.lin todslip.lin torcross.lin \
	tolcross.lin todcross.lin tostrsct.lin tocrvsct.lin tostrsct.lin \
	tostrsct.lin
$(LIB)(ctrain.o): ctrain.c track.h trackx.h ctrain.h compound.h train.xpm stop.xpm go.xpm \
	exit.xpm newcar.xpm zero.xpm
$(LIB)(cturnout.o): cturnout.c track.h ccurve.h cstraigh.h compound.h cjoin.h turnout.xpm
$(LIB)(cturntbl.o): cturntbl.c track.h cstraigh.h turntbl.xpm
$(LIB)(cundo.o): cundo.c track.h trackx.h

custom.o: custom.c track.h version.h common.h misc.h fileio.h cjoin.h xtc64.bmp \
	xtc16.bmp

$(LIB)(dbench.o): dbench.c track.h bo_edge.xpm bo_flat.xpm bo_ll.xpm bo_lr.xpm bo_lld.xpm \
	bo_lrd.xpm bo_llu.xpm bo_lru.xpm bo_lli.xpm bo_lri.xpm bo_t.xpm \
	bo_tr.xpm bo_tl.xpm bo_ti.xpm
$(LIB)(dbitmap.o): dbitmap.c track.h
$(LIB)(dcar.o): dcar.c track.h ctrain.h carpart.xpm carproto.xpm
$(LIB)(dcmpnd.o): dcmpnd.c track.h compound.h shrtpath.h turnout.xpm struct.xpm
$(LIB)(dcustmgm.o): dcustmgm.c track.h
$(LIB)(dease.o): dease.c track.h ccurve.h cstraigh.h cjoin.h enone.xpm esharp.xpm \
	egtsharp.xpm enormal.xpm eltbroad.xpm ebroad.xpm egtbroad.xpm
$(LIB)(denum.o): denum.c track.h
$(LIB)(dlayer.o): dlayer.c track.h bitmaps/l1.xbm bitmaps/l2.xbm bitmaps/l3.xbm bitmaps/l4.xbm \
	bitmaps/l5.xbm bitmaps/l6.xbm bitmaps/l7.xbm bitmaps/l8.xbm \
	bitmaps/l9.xbm bitmaps/l10.xbm bitmaps/l11.xbm bitmaps/l12.xbm \
	bitmaps/l13.xbm bitmaps/l14.xbm bitmaps/l15.xbm bitmaps/l16.xbm \
	bitmaps/l17.xbm bitmaps/l18.xbm bitmaps/l19.xbm bitmaps/l20.xbm 	
$(LIB)(doption.o): doption.c track.h ccurve.h
$(LIB)(dpricels.o): dpricels.c track.h compound.h
$(LIB)(dprmfile.o): dprmfile.c track.h mtbox.bmp chkbox.bmp
$(LIB)(draw.o): draw.c track.h utility.h misc.h draw.h $(WLIB)/include/wlib.h
$(LIB)(drawgeom.o): drawgeom.c track.h ccurve.h compound.h drawgeom.h
$(LIB)(elev.o): elev.c track.h shrtpath.h ccurve.h
$(LIB)(fileio.o): fileio.c track.h version.h common.h utility.h draw.h misc.h compound.h
$(LIB)(lprintf.o): lprintf.c track.h
$(LIB)(macro.o): macro.c track.h version.h common.h utility.h draw.h misc.h compound.h \
	arrow0.bmp arrow3.bmp arrows.bmp flash.bmp
$(LIB)(misc.o): misc.c smalldlg.h track.h common.h utility.h draw.h misc.h cjoin.h compound.h \
	openbutt.xpm zoomin.xpm zoom.xpm zoomout.xpm undo.xpm redo.xpm \
	partlist.xpm export.xpm import.xpm
$(LIB)(misc2.o): misc2.c track.h common.h utility.h draw.h misc.h cjoin.h compound.h
$(LIB)(param.o): param.c track.h common.h utility.h misc.h compound.h $(WLIB)/include/wlib.h square10.bmp
$(LIB)(smalldlg.o): smalldlg.c smalldlg.h common.h draw.h misc.h param.h custom.h param.h
$(LIB)(shrtpath.o): shrtpath.c track.h shrtpath.h
$(LIB)(tcurve.o): tcurve.c track.h ccurve.h cstraigh.h cjoin.h
$(LIB)(tease.o): tease.c track.h ccurve.h cstraigh.h cjoin.h
$(LIB)(track.o): track.c track.h ccurve.h cstraigh.h cjoin.h compound.h trackx.h above.xpm \
	below.xpm
$(LIB)(trkseg.o): trkseg.c track.h cjoin.h
$(LIB)(tstraigh.o): tstraigh.c track.h cstraigh.h
$(LIB)(utility.o): utility.c common.h utility.h
