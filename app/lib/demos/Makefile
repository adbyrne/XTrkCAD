#
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/demos/Makefile,v 1.1 2005-12-07 15:47:43 rc-flyer Exp $
#

CC = gcc

VER = d
COPTS = -Wall
CINCLS = -I/usr/openwin/include
PACKAGEDIR = ./package
VER=0.1

all: debug

MISC = Makefile

DEMOS =  \
	dmadjend.xtr \
	dmbench.xtr \
	dmcancel.xtr \
	dmcircle.xtr \
	dmconn1.xtr \
	dmconn2.xtr \
	dmctlpnl.xtr \
	dmcrvtrk.xtr \
	dmdelund.xtr \
	dmdialog.xtr \
	dmdimlin.xtr \
	dmease.xtr \
	dmelev.xtr \
	dmexcept.xtr \
	dmextend.xtr \
	dmintro.xtr \
	dmhelix.xtr \
	dmflip.xtr \
	dmgroup.xtr \
	dmhndld.xtr \
	dmjcir.xtr \
	dmjnabut.xtr \
	dmjncs.xtr \
	dmjnmove.xtr \
	dmjnss.xtr \
	dmjntt.xtr \
	dmlines.xtr \
	dmlines2.xtr \
	dmmouse.xtr \
	dmmovabt.xtr \
	dmnotes.xtr \
	dmparall.xtr \
	dmplymod.xtr \
	dmprof.xtr \
	dmrescal.xtr \
	dmrotate.xtr \
	dmruler.xtr \
	dmselect.xtr \
	dmsplit.xtr \
	dmstrtrk.xtr \
	dmtbledg.xtr \
	dmtodes.xtr \
	dmtosel.xtr \
	dmtotrim.xtr \
	dmtoyard.xtr \
	dmtrkwid.xtr \
	dmtrntab.xtr

SRCS = $(MISC) $(DEMOS)

update: debug product

debug:
	@echo nothing to do

product:
	@echo nothing to do

checked:
	@echo nothing to do

tar:
	tar cvf xtclib.tar $(SRCS)

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(SRCS) ; fi

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ;\
	co $$TAG  $(SRCS)

src:
	@echo $(SRCS)

rcssrc:
	@echo $(SRCS)

demofiles:
	@echo $(DEMOS)
