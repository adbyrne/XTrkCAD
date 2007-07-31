# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/doc/Makefile,v 1.2 2007-07-31 18:33:15 m_fischer Exp $
#
# Makefile for halibut based documentation
#

HALIBUT = ../tools/halibut/halibut

all: help

BUTFILES = \
	chmconf.but \
	intro.but \
	mainw.but \
	filem.but \
	editm.but \
	viewm.but \
	addm.but \
	changem.but \
	drawm.but \
	managem.but \
	optionm.but \
	win_help.but \
	map_hot.but \
	appendix.but

help: index.html

index.html: $(BUTFILES)
	$(HALIBUT) $^

clean:
	rm *.html
	rm xtrkcad.hhp
	rm xtrkcad.hhc
