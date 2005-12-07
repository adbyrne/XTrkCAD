EXAMPLES = \
	katoxing.xtc \
	mr03.xtc \
	ntrak4.xtc \
	ntrak6.xtc \
	ntrak8.xtc \
	spagw1.xtc \
	spagw2.xtc \
	spagw3.xtc \
	spagw4.xtc \
	spagw5.xtc \
	cascade.xtc \
	timesavr.xtc \
	Control-Panels.xtc

SRCS = $(EXAMPLES) Makefile

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

examplefiles:
	@echo $(EXAMPLES)
