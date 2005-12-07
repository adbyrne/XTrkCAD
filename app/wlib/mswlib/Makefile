
CSRCS = \
	mswchksm.c \
	mswmisc.c \
	mswbutt.c \
	mswchoic.c \
	mswdraw.c \
	mswcolor.c \
	mswlines.c \
	mswlist.c \
	mswmenu.c \
	mswmsg.c \
	mswprint.c \
	mswedit.c \
	mswtext.c \
	mswbox.c \
	mswpref.c \
	checksum.c

HDRS = \
	mswint.h dynarr.h

MISC = Makefile mswlib.mak square10.bmp wlib.dsp checksum.dsp

SRCS =  $(CSRCS) $(HDRS) $(MISC)

all:
	@echo nothing to do

src:
	@echo $(SRCS)

rcssrc:
	@echo $(SRCS)

fetch:
	if [ "$(TAG)"x != "x" ] ; then TAG=-r$(TAG) ; fi ;\
	co $$TAG  $(SRCS)

tag:
	if [ "$(TAG)"x = "x" ] ; then echo define TAG ; else rcs -N$(TAG):HEAD $(SRCS) ; fi

mswchksm: mswchksm.c
	cc -DTEST -o mswchksm -g -I../include mswchksm.c
