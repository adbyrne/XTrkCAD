SRCS = wlib.h mswlib.h wcolors.h mswlib.rc Makefile

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
