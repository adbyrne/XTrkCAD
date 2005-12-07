all: debug

RLSE=dev

OBJDISK = j
SRCDISK = q
OBJDIR = $(OBJDISK):\xtrkcad\$(RLSE)\bin
#OBJDIR = .

RLSE = dev

#MSWOBJS1 = mswmisc.obj mswbutt.obj mswchoic.obj mswcolor.obj mswdraw.obj mswlines.obj
#MSWOBJS2 =  mswlist.obj mswmenu.obj mswmsg.obj mswprint.obj
#MSWOBJS3 =  mswedit.obj mswtext.obj mswbox.obj mswpref.obj mswchksm.obj
#MSWOBJS = $(MSWOBJS1) $(MSWOBJS2) $(MSWOBJS3)
MSWOBJS = \
	$(OBJDIR)\mswmisc.obj \
	$(OBJDIR)\mswbutt.obj \
	$(OBJDIR)\mswchoic.obj \
	$(OBJDIR)\mswcolor.obj \
	$(OBJDIR)\mswdraw.obj \
	$(OBJDIR)\mswlines.obj \
	$(OBJDIR)\mswlist.obj \
	$(OBJDIR)\mswmenu.obj \
	$(OBJDIR)\mswmsg.obj \
	$(OBJDIR)\mswprint.obj \
	$(OBJDIR)\mswedit.obj \
	$(OBJDIR)\mswtext.obj \
	$(OBJDIR)\mswbox.obj \
	$(OBJDIR)\mswpref.obj \
	$(OBJDIR)\mswchksm.obj
	

.c{$(OBJDIR)}.obj:
	@cl /nologo /DWINDOWS /I..\include /c /Gt1 /J /Gf /Gw $(COPTS) /Zpe /AL /W3 /YX /Fo$*.obj $<
	@lib /nologo $(OBJDIR)\mswlib -+$@, ,
	
debug: mswchksm.exe
	nmake /f mswlib.mak RLSE=$(RLSE) COPTS="/Od /Zi" OBJDIR="$(OBJDIR)" LOPTS="/CO" $(OBJDIR)\mswlib.lib

product: mswchksm.exe
	nmake /f mswlib.mak RLSE=$(RLSE) COPTS="/G3 /Gs /f- /Oo- /Zi" OBJDIR="$(OBJDIR)" LOPTS= $(OBJDIR)\mswlib.lib

$(OBJDIR)\mswlib.lib: $(MSWOBJS)

mswchksm.exe: mswchksm.c
	cl /nologo /DTEST /I..\include /AL mswchksm.c
