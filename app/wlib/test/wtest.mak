all: $(TEST).exe wtest.res  

some: wtest.exe wtest.res    

FREDOBJS = $(TEST).obj  

.c.obj: 	
		@cl /nologo /DWINDOWS /c /I../include /Od /Zp /W3 $<  	  

mswlibl: 	
		echo making mswlib.lib 	
		cd ..\mswlib 	
		makemsw 	
		cd ..\wtest   

$(TEST).exe: $(FREDOBJS) wtest.def 	
		@echo /nologo > $(TEST).lnk 	
		@echo $(TEST).obj >> $(TEST).lnk 	
		@echo $(TEST) >> $(TEST).lnk 	
		@echo $(TEST) >> $(TEST).lnk 	
		@echo ..\mswlib\Debug\wlib.lib  >> $(TEST).lnk 	
		link @$(TEST).lnk 	
		rc /I.. /I..\mswlib wtest.rc $(TEST).exe  