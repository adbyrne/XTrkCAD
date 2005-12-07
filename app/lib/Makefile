#
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/Makefile,v 1.1 2005-12-07 15:47:39 rc-flyer Exp $
#

CC = gcc

VER = d
COPTS = -Wall
PACKAGEDIR = ./package
VER=0.1

all: debug

GENFILES = xtrkcad.upd
#GENFILES = xtrkcad.ini xtrkcad.upd
#MISC = Makefile aareadme.txt xtrkcad.mst0 xtrkcad.mst1 mklyt xtrkcad.lst
MISC = Makefile aareadme.txt xtrkcad.spec.head COPYING
XTQ = \
	xtrkcad.xtq


HELPFILES = xtrkcad.bug xtrkcad.fix xtrkcad.enh

SRCS = $(MISC) $(XTQ) $(HELPFILES)

update: debug product

debug: $(GENFILES)
	@echo nothing to do

product: $(GENFILES)
	@echo nothing to do

xtrkcad.mst: xtrkcad.mst0 xtrkcad.mst1
	cat xtrkcad.mst0 > xtrkcad.mst
	N=1; \
	for f in `make --no-print-director -f params/Makefile xtps` ; do \
		echo '    CreateIniKeyValue MakePath( GetWindowsDir(), "XTRKCAD.INI" ), "Parameter File Update", "File'$$N'", MakePath( SUBDIR$$, "'$$f'" ), cmoOverwrite' >> xtrkcad.mst; \
		N=`expr $$N + 1`; \
	done; \
	cat xtrkcad.mst1 >> xtrkcad.mst

xtrkcad.lyt: mklyt
	@./mklyt xtrkcad

xtrkcad.upd:
	( date "+%Y%m%d" ; \
	for f in `make --no-print-director -f params/Makefile xtps` ; do \
	echo $$f ; done ) > xtrkcad.upd

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

xtrkcad.ini: Makefile params/Makefile
	echo '[file]' 			> xtrkcad.ini
	echo 'directory=$$i\EXAMPLES'	>> xtrkcad.ini
	echo 'paramdir=$$i\PARAMS'	>> xtrkcad.ini
	echo ''				>> xtrkcad.ini
	echo '[draw]'			>> xtrkcad.ini
	echo 'roomsizeX=96.0'		>> xtrkcad.ini
	echo 'roomsizeY=48.0'		>> xtrkcad.ini
	echo ''				>> xtrkcad.ini
	echo '[Parameter File Update]'	>> xtrkcad.ini
	N=1; \
	for f in `make --no-print-director -f params/Makefile xtps` ; do \
		echo 'File'$$N'=$$i\PARAMS\'$$f >> xtrkcad.ini; \
		N=`expr $$N + 1`; \
	done


rpm:
	#if [ ! -d ../usr ] ; then mkdir -p ../install/usr/local/lib ; ln -s ../../../../xtrkcad ../install/usr/local/lib/xtrkcad ; fi
	D=`pwd`; D=`dirname $$D`; D=`basename $$D`; \
	V=`echo $$D | sed -e 's/^r//' -e 's/_/./g'`; \
	sed -e "s/RLSEDIR/$$D/g" -e "s/RLSEVER/$$V/g"  xtrkcad.spec.head > xtrkcad.spec
	cd .. ; find xtrkcad -type f -print | sed 's/^/\/usr\/local\/lib\//' | sed '/html/s/^/%doc /' | sed '/xtrkcad.help/s/^/%doc /' >> lib/xtrkcad.spec

	if [ ! -d ../usr ] ; then mkdir -p ../install/usr/local/lib ; mv  ../xtrkcad ../install/usr/local/lib/xtrkcad ; fi
	cd ..; rpm -bb lib/xtrkcad.spec

clean:
	cd params; make clean
	rm -f xtrkcad.help xtrkcad.spec xtrkcad.upd
