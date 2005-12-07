all: $(TEST).exe wtest.res

some: wtest.exe wtest.res



FREDOBJS = $(TEST).obj

.c.obj:
	@cl /nologo /DWINDOWS /c /I.. /Gsw /Od /Zpe /AL /W3 /Zi $<
	

mswlibl:
	echo making mswlib.lib
	cd ..\mswlib
	makemsw
	cd ..\wtest


$(TEST).exe: $(FREDOBJS) wtest.def mswlibl
	@echo /nologo /nod /CO +> $(TEST).lnk
	@echo $(TEST).obj >> $(TEST).lnk
	@echo $(TEST) >> $(TEST).lnk
	@echo $(TEST) >> $(TEST).lnk
	@echo ..\mswlib\mswlib llibcew libw commdlg ..\mswlib\ctl3d >> $(TEST).lnk
	@echo wtest.def >> $(TEST).lnk
	link @$(TEST).lnk
	rc /I.. /I..\mswlib wtest.rc $(TEST).exe

