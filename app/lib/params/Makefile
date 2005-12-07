# # $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/params/Makefile,v 1.1 2005-12-07 15:47:50 rc-flyer Exp $
#

CC = gcc

VER = d
COPTS = -Wall
CINCLS = -I/usr/openwin/include
PACKAGEDIR = ./package
VER=0.1

all: debug

.to.xtp:
	sh $*.to > $*.xtp

MISC = Makefile nmra-to.c mkstruct.c mkcarpart.c roadname.tab color.tab
TOOLS = mkcarpart

PROTOFILES = \
	protoam.xtp

NMRAFILES = \
	nmra-ho.xtp \
	nmra-ho3.xtp \
	nmra-n.xtp \
	nmra-o.xtp \
	nmra-o3.xtp \
	nmra-s.xtp \
	nmra-tt.xtp

TURNOUTS = \
	arnold.xtp \
	atl100ho.xtp \
	atl83ho.xtp \
	atlasn.xtp \
	atlasn55.xtp \
	fl-model.xtp \
	fl-profi.xtp \
	flnpicco.xtp \
	gargrv-o.xtp \
	hornby3r.xtp \
	hubner1.xtp \
	kato-ho.xtp \
	kato-n.xtp \
	lgb.xtp \
	me-ho.xtp \
	me-n.xtp \
	minitrix.xtp \
	mrkhocde.xtp \
	mrkln1.xtp \
	mrklnhoc.xtp \
	mrklnhok.xtp \
	mrklnhom.xtp \
	mrklnz.xtp \
	p4.xtp \
	pecon55.xtp \
	pecon80.xtp \
	peconst.xtp \
	pecoho.xtp \
	pecohom.xtp \
	rocho100.xtp \
	rocho83.xtp \
	rocon.xtp \
	walth-ho.xtp \
	walth-n.xtp \
	wlthho10.xtp

MKSTRUCTS = \
	fallerho.xtp \
	pikestuf.xtp \
	revell.xtp \
	smltown.xtp \

STRUCTS = \
	ctlpanel.xtp \
	walth-ho.xtp \
	walth-n.xtp \
	tortoise.xtp \
	trees.xtp

CARS = 

CARFILES = \
	accurail.xtp \
	atlascn.xtp \
	atlascho.xtp

GENFILES = $(NMRAFILES) $(MKSTRUCTS) $(CARFILES)
XTP = $(TURNOUTS) $(STRUCTS) $(CARS) $(CARFILES) $(GENFILES) $(PROTOFILES)

RCSSRCS = $(MISC) $(TURNOUTS) $(STRUCTS) $(CARS) $(PROTOFILES) $(MKSTRUCTS:.xtp=.struct)
SRCS = $(RCSSRCS) $(NMRAFILES:.xtp=.to) $(CARFILES:.xtp=.cars)

update: debug product

debug: $(TOOLS) $(GENFILES)
	@echo nothing to do

product: $(TOOLS) $(GENFILES)
	@echo nothing to do

nmra-to: nmra-to.c
	$(CC) -o nmra-to nmra-to.c -lm

mkstruct: mkstruct.c
	$(CC) -o mkstruct mkstruct.c -lm

mkcarpart: mkcarpart.o
	$(CC) -o mkcarpart mkcarpart.o

mkcarpart.o: mkcarpart.c
	$(CC) -c -g mkcarpart.c

checked:
	@echo nothing to do

gencars: $(CARFILES)

tar:
	tar cvf xtclib.tar $(SRCS)

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(RCSSRCS) ; fi

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ;\
	co $$TAG  $(RCSSRCS)

src:
	@echo $(SRCS)

rcssrc:
	@echo $(RCSSRCS)

xtps:
	@echo $(XTP)

nmra-o.xtp: nmra-to nmra-o.to; sh nmra-o.to > nmra-o.xtp
nmra-s.xtp: nmra-to nmra-s.to; sh nmra-s.to > nmra-s.xtp
nmra-ho.xtp: nmra-to nmra-ho.to; sh nmra-ho.to > nmra-ho.xtp
nmra-tt.xtp: nmra-to nmra-tt.to; sh nmra-tt.to > nmra-tt.xtp
nmra-o3.xtp: nmra-to nmra-o3.to; sh nmra-o3.to > nmra-o3.xtp
nmra-ho3.xtp: nmra-to nmra-ho3.to; sh nmra-ho3.to > nmra-ho3.xtp
nmra-n.xtp: nmra-to nmra-n.to; sh nmra-n.to > nmra-n.xtp

fallerho.xtp: mkstruct fallerho.struct; sh fallerho.struct > fallerho.xtp
pikestuf.xtp: mkstruct pikestuf.struct; sh pikestuf.struct > pikestuf.xtp
revell.xtp: mkstruct revell.struct; sh revell.struct > revell.xtp
smltown.xtp: mkstruct smltown.struct; sh smltown.struct > smltown.xtp

accurail.xtp: accurail.cars mkcarpart; ./mkcarpart accurail.cars accurail.xtp
atlascn.xtp: atlascn.cars mkcarpart; ./mkcarpart atlascn.cars atlascn.xtp
atlascho.xtp: atlascho.cars mkcarpart; ./mkcarpart atlascho.cars atlascho.xtp

clean:
	rm -f accurail.xtp atlascho.xtp atlascn.xtp fallerho.xtp mkcarpart mkcarpart.o mkstruct 
	rm -f nmra-ho.xtp nmra-ho3.xtp nmra-n.xtp nmra-o3.xtp nmra-o.xtp nmra-s.xtp nmra-to nmra-tt.xtp
	rm -f pikestuf.xtp revell.xtp smltown.xtp 
