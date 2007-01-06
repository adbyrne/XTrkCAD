# dies ist das Makefile für die neue DS&W Homepage
#
# Erstellt: 27.10.2000
SHELL = /bin/bash

WPP = wpp
WPPFLAGS = 
PROCESS = $(WPP) $(WPPFLAGS) 

SRCS := $(wildcard *.raw)
DEPS := $(patsubst %.raw, %.dep, $(SRCS))
HTMLS := $(patsubst %.raw, ../%.html, $(SRCS))


all: $(HTMLS)
	
%.dep: %.raw
	$(WPP) -d $< > $@

../%.html: %.raw
	$(PROCESS) $<

depend: $(DEPS)
	@echo "Dependencies sind jetzt aktuell."
	@echo $(HTMLS)

-include $(DEPS)
