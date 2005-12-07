RTFS = contents.rtf mswhead.rtf mswtail.rtf
HPJ = xtrkcad.hpj.first xtrkcad.hpj.last
HLPSRC = xtrkcad.hlpsrc xvfontsel.hlpsrc xvprintsel.hlpsrc
MISC = Makefile genhelp.in messages.in xtrkcad.tip
CSRCS = prochelp.c genhelp.c genmessages.c mkxpmbutt.c pageorder.c fixbmp.c readpng.c readpng.h
TOOLS = mkpngs mkpngbutt mkshg
TOOLSRCS = mkpngs mkpngbutt mkshg.c mkshgs
EXTRAS = dmpbmp dmppng

SRCS = $(RTFS) $(HPJ) $(HLPSRC) $(MISC) $(CSRCS) $(TOOLSRCS)

PNGS = \
	images.orig/bglayer.png images.orig/bhotbar.png images.orig/bitmap.png \
	images.orig/blayer.png \
	images.orig/carinv.png images.orig/caritem.png images.orig/carpart.png \
	images.orig/carproto.png images.orig/celev.png \
	images.orig/chelix.png images.orig/cparalle.png images.orig/cprofile.png \
	images.orig/ctext.png images.orig/custmgm.png \
	images.orig/dchgelev.png images.orig/demo.png images.orig/dlayer.png \
	images.orig/easeex1.png images.orig/easeex2.png images.orig/easew.png \
	images.orig/grid.png images.orig/layer.png images.orig/pricels.png \
	images.orig/print.png images.orig/prmfile.png \
	images.orig/regw.png images.orig/rescale.png images.orig/strsel.png \
	images.orig/train.png images.orig/trainbar.png images.orig/ttdiam.png \
	images.orig/turndes.png images.orig/turnsel.png \
	images.orig/updttl.png images.orig/wenum.png images.orig/wmain.png images.orig/wmap.png

all: ../bin/bllnhlp.c xtrkcad.help xtrkcad.text html/xtrkcad.html ../bin/messages.h mkxpmbutt pageorder

bllnhlp.c: genhelp.in genhelp
	./genhelp -bh genhelp.in bllnhlp.out
	sed -e '/"help"/d' -e '/"cancel"/d' -e '/"close"/d' bllnhlp.out > bllnhlp.c

../bin/bllnhlp.c: bllnhlp.c
	@if [ ! -f ../bin/bllnhlp.c ] ; then \
		cp -v bllnhlp.c ../bin/bllnhlp.c ; \
	else \
	if diff bllnhlp.c ../bin/bllnhlp.c > /dev/null 2>&1 ; then \
		echo no change in bllnhlp.c; \
	else \
		cp -v bllnhlp.c ../bin/bllnhlp.c ; \
	fi ; \
	fi

messages.h messages.hlpsrc: messages.in genmessages
	./genmessages < messages.in

../bin/messages.h: messages.h
	@if [ ! -f ../bin/messages.h ] ; then \
		cp -v messages.h ../bin/messages.h ; \
	else \
	if diff messages.h ../bin/messages.h > /dev/null 2>&1 ; then \
		echo no change in messages.h; \
	else \
		cp -v messages.h ../bin/messages.h ; \
	fi ; \
	fi

popups.rtf: genhelp.in genhelp
	./genhelp -msw genhelp.in popups.rtf

xtrkcad.info: genhelp.in genhelp
	./genhelp -xv genhelp.in xtrkcad.info

xtrkcad.help: xtrkcad.hlpsrc messages.hlpsrc prochelp messages.hlpsrc
	./prochelp -xv -C unix xtrkcad.hlpsrc xtrkcad.help

xtchelp.d/xtrkcad.hpj: genhelp.in genhelp xtrkcad.hpj.first xtrkcad.hpj.last
	@if [ ! -d xtchelp.d ] ; then mkdir xtchelp.d ; fi
	./genhelp -hpj genhelp.in popups.ali
	cat xtrkcad.hpj.first popups.ali xtrkcad.hpj.last > xtchelp.d/xtrkcad.hpj

xtchelp: xtchelp.d/xtrkcad.hpj mkshgs mkshg
	./mkshgs
	@make xtchelp.d/xtrkcad.rtf

checkshgs:
	@make bmps
	@for BMP in bmps.d/*.bmp ; do \
		SHG=`echo $$BMP|sed -e 's/\.bmp/.shg/' -e 's/bmps.d/winhelp.d/'`; \
		if [ ! -f $$SHG -o $$BMP -nt $$SHG ] ; then \
			echo update $$SHG ; \
		fi \
	done
	
xtchelp.d/xtrkcad.rtf: xtrkcad.hlpsrc messages.hlpsrc prochelp popups.rtf mswhead.rtf mswtail.rtf
	@if [ ! -d xtchelp.d ] ; then mkdir xtchelp.d ; fi
	( cat mswhead.rtf ; \
	  ./prochelp -mswhelp -C msw xtrkcad.hlpsrc - ; \
	  cat popups.rtf mswtail.rtf ) | \
	  sed 's/{\\b XTrkCad}/{\\b\\i XTrkCad}/g' > xtchelp.d/xtrkcad.rtf

#bmps:
#	@if [ ! -d bmps.d ] ; then mkdir bmps.d ; fi
#	@for GIF in gifs.d/*.gif images.orig/*.gif ; do \
#		BMP=`echo $$GIF|sed -e 's/\.gif/.bmp/' -e 's/gifs.d/bmps.d/' -e 's/images.orig/bmps.d/'`; \
#		if [ ! -f $$BMP -o $$GIF -nt $$BMP  ] ; then \
#			echo updating $$BMP ; \
#			convert +compress $$GIF BMP:$$BMP ; \
#		fi \
#	done

pngs: mkpngbutt mkxpmbutt
	@if [ ! -d png.d ] ; then mkdir png.d ; fi
	./mkpngs

xtcword: #pngs
	@make xtcword.d/xtcword.rtf

xtcword.d/xtcword.rtf: xtrkcad.hlpsrc messages.hlpsrc prochelp mswhead.rtf mswtail.rtf # png.d/*.png images.orig/*.png
	@if [ ! -d xtcword.d ] ; then mkdir xtcword.d ; fi
	@( sed 's/fs20/fs22/g' mswhead.rtf ; \
	  ./prochelp -mswword -C msw -toc -d png.d -d images.orig -ml 0.50 -mr 0.50 -mg 0.25 -mb 0.25 -mt 0.25 xtrkcad.hlpsrc - ; \
	  cat mswtail.rtf ) | \
	  sed 's/{\\b XTrkCad}/{\\b\\i XTrkCad}/g' > xtcword.d/xtcword.rtf

test.rtf:
	( sed 's/fs20/fs22/g' mswhead.rtf ; \
	  ./prochelp -mswword -C msw -toc -d png.d -d images.orig -ml 0.50 -mr 0.50 -mg 0.25 -mb 0.25 -mt 0.25 test.hlpsrc - ; \
	  cat mswtail.rtf ) | \
	  sed 's/{\\b XTrkCad}/{\\b\\i XTrkCad}/g' > xtcword.d/test.rtf

xtrkcad.text: xtrkcad.hlpsrc messages.hlpsrc prochelp
	./prochelp -text -C unix xtrkcad.hlpsrc xtrkcad.text

html/xtrkcad.html: xtrkcad.hlpsrc messages.hlpsrc prochelp
	@rm -rf html
	mkdir html
	@cd html ; ln -s ../png.d/*.png ../images.orig/*.png ./
	cd html ; ../prochelp -html -C unix -d .. xtrkcad.hlpsrc xtrkcad

prochelp: prochelp.c readpng.c
	cc -g -o prochelp prochelp.c readpng.c -lpng -lz -lm

mkxmpbutt: mkxmpbutt.c
	cc -g -o mkxmpbutt mkxmpbutt.c

pageorder: pageorder.c
	cc -g -o pageorder pageorder.c

genhelp: genhelp.c
	cc -g -o genhelp genhelp.c

genmessages: genmessages.c
	cc -g -o genmessages genmessages.c

dmpbmp: dmpbmp.o
	cc -o dmpbmp dmpbmp.o
dmpbmp.o: dmpbmp.c
	cc -g -c dmpbmp.c

mkshg: mkshg.o readpng.o
	cc -o mkshg mkshg.o readpng.o -lpng -lz -lm
mkshg.o: mkshg.c
	cc -g -c mkshg.c
dmppng: dmppng.o readpng.o
	cc -o dmppng dmppng.o readpng.o -lpng -lz -lm
dmppng.o: dmppng.c
	cc -g -c dmppng.c
readpng.o: readpng.c
	cc -g -c readpng.c

src:
	@echo $(SRCS) $(PNGS)

rcssrc:
	@echo $(SRCS)

tar:
	tar cvf track.tar $(SRCS)

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(SRCS) ; fi

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ;\
	co $$TAG  $(SRCS)

clean:
	rm -f bllnhlp.c bllnhlp.out genhelp genmessages messages.h messages.hlpsrc mkxpmbutt pageorder 
	rm -f prochelp xtrkcad.help xtrkcad.text
	rm -fr html  png.d
