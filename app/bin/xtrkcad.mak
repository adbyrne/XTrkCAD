
RLSE = dev

OBJDISK = j
SRCDISK = q
OBJDIR = $(OBJDISK):\xtrkcad\$(RLSE)\bin
MSWLIB = $(OBJDISK):\xtrkcad\$(RLSE)\bin\mswlib.lib
MSWINC = $(SRCDISK):\wlib\$(RLSE)\include
XTCDIR = $(SRCDISK):\xtrkcad\$(RLSE)\bin
MSWDIR = $(SRCDISK):\wlib\$(RLSE)\mswlib

#OBJDIR = .
#MSWLIB = $(OBJDISK):\dave\$(RLSE)\bin\mswlib.lib
#MSWINC = $(OBJDISK):\dave\$(RLSE)\include
#XTCDIR = $(OBJDISK):\dave\$(RLSE)\bin
#MSWDIR = $(OBJDISK):\dave\$(RLSE)\mswlib

all: codeview

OBJS = \
	$(OBJDIR)\bllnhlp.obj \
	$(OBJDIR)\ccurve.obj \
	$(OBJDIR)\cdraw.obj \
	$(OBJDIR)\celev.obj \
	$(OBJDIR)\cgroup.obj \
	$(OBJDIR)\chndldto.obj \
	$(OBJDIR)\chotbar.obj \
	$(OBJDIR)\cjoin.obj \
	$(OBJDIR)\cmisc.obj \
	$(OBJDIR)\cmisc2.obj \
	$(OBJDIR)\cmodify.obj \
	$(OBJDIR)\cnote.obj \
	$(OBJDIR)\compound.obj \
	$(OBJDIR)\cparalle.obj \
	$(OBJDIR)\cprint.obj \
	$(OBJDIR)\cprofile.obj \
	$(OBJDIR)\cpull.obj \
	$(OBJDIR)\cruler.obj \
	$(OBJDIR)\cselect.obj \
	$(OBJDIR)\csplit.obj \
	$(OBJDIR)\csnap.obj \
	$(OBJDIR)\cstraigh.obj \
	$(OBJDIR)\cstruct.obj \
	$(OBJDIR)\ctext.obj \
	$(OBJDIR)\ctodesgn.obj \
	$(OBJDIR)\ctrain.obj \
	$(OBJDIR)\cturnout.obj \
	$(OBJDIR)\cturntbl.obj \
	$(OBJDIR)\cundo.obj \
	$(OBJDIR)\dbench.obj \
	$(OBJDIR)\dbitmap.obj \
	$(OBJDIR)\dcar.obj \
	$(OBJDIR)\dcmpnd.obj \
	$(OBJDIR)\dcustmgm.obj \
	$(OBJDIR)\dease.obj \
	$(OBJDIR)\denum.obj \
	$(OBJDIR)\dlayer.obj \
	$(OBJDIR)\doption.obj \
	$(OBJDIR)\dprmfile.obj \
	$(OBJDIR)\dpricels.obj \
	$(OBJDIR)\dlayer.obj \
	$(OBJDIR)\draw.obj \
	$(OBJDIR)\drawgeom.obj \
	$(OBJDIR)\elev.obj \
	$(OBJDIR)\fileio.obj \
	$(OBJDIR)\lprintf.obj \
	$(OBJDIR)\macro.obj \
	$(OBJDIR)\misc.obj \
	$(OBJDIR)\misc2.obj \
	$(OBJDIR)\param.obj \
	$(OBJDIR)\shrtpath.obj \
	$(OBJDIR)\tcurve.obj \
	$(OBJDIR)\tease.obj \
	$(OBJDIR)\track.obj \
	$(OBJDIR)\trkseg.obj \
	$(OBJDIR)\tstraigh.obj \
	$(OBJDIR)\utility.obj

CCOPTS="/G3 /f- /Oo- /Gs /Zi" 

.c{$(OBJDIR)}.obj:
	cl /nologo /DWINDOWS /c /I$(MSWINC) /Gt1 /J /GA /Gf $(COPTS) /Zpe /AL /W3 /YX /Fo$*.obj $<
	@lib /nologo /page:4096 $(OBJDIR)\xtrkcad -+$@, ,

$(OBJDIR)\xtrkcad.obj: custom.c
	cl /nologo /DWINDOWS /c /I$(MSWINC) /Gt1 /J /GA /Gf $(COPTS) /Zpe /AL /W3 /YX /Fo$(OBJDIR)\xtrkcad.obj custom.c

$(OBJDIR)\katocad.obj: custom.c
	cl /nologo /DKATO /DWINDOWS /c /I$(MSWINC) /Gt1 /J /GA /Gf $(COPTS) /Zpe /AL /W3 /YX /Fo$(OBJDIR)\katocad.obj custom.c

debug: # convrtrs
	cd $(MSWDIR)
	nmake /f mswlib.mak RLSE=$(RLSE) debug
	cd $(XTCDIR)
	-del $(OBJDIR)\xtrkcad.exe
	nmake /f xtrkcad.mak RLSE=$(RLSE) COPTS=$(CCOPTS) OBJDIR=$(OBJDIR) LOPTS="/CO" PROD=xtrkcad CV= $(OBJDIR)\xtrkcad.exe

product: # convrtrs
	cd $(MSWDIR)
	nmake /f mswlib.mak RLSE=$(RLSE) product
	cd $(XTCDIR)
	-del $(OBJDIR)\xtrkcad.exe
	nmake /f xtrkcad.mak RLSE=$(RLSE) COPTS=$(CCOPTS) OBJDIR=$(OBJDIR) LOPTS="/LINE" PROD=xtrkcad CV= $(OBJDIR)\xtrkcad.exe
	-copy $(OBJDIR)\xtrkcad.exe $(XTCDIR)

codeview: # convrtrs
	cd $(MSWDIR)
	nmake /f mswlib.mak RLSE=$(RLSE) product
	cd $(XTCDIR)
	-del $(OBJDIR)\xtrkcadc.exe
	nmake /f xtrkcad.mak RLSE=$(RLSE) COPTS=$(CCOPTS) OBJDIR=$(OBJDIR) LOPTS="/LINE /CO" PROD=xtrkcad CV=c $(OBJDIR)\xtrkcadc.exe
	-copy $(OBJDIR)\xtrkcadc.exe $(XTCDIR)

xtrkcadc: # convrtrs
	cd $(MSWDIR)
	nmake /f mswlib.mak RLSE=$(RLSE) product
	cd $(XTCDIR)
	-del $(OBJDIR)\xtrkcadc.exe
	nmake /f xtrkcad.mak RLSE=$(RLSE) COPTS=$(CCOPTS) OBJDIR=$(OBJDIR) LOPTS="/LINE /CO" PROD=xtrkcad CV=c $(OBJDIR)\xtrkcadc.exe

convrtrs: bdf2xtp.exe


$(OBJDIR)\$(PROD)$(CV).exe: $(OBJS) $(OBJDIR)\$(PROD).obj $(PROD).def $(PROD).rc #mswlib\mswlib.lib
	echo /nologo $(LOPTS) /NOE /LINE /ONERROR:NOEXE /MAP:FULL +> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(OBJDIR)\misc.obj + >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(OBJDIR)\$(PROD).obj >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(OBJDIR)\$(PROD)$(CV) >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(OBJDIR)\$(PROD)$(CV) >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(OBJDIR)\xtrkcad + >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(MSWLIB) + >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo llibcew libw commdlg + >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo ctl3ds >> $(OBJDIR)\$(PROD)$(CV).lnk
	echo $(PROD).def >> $(OBJDIR)\$(PROD)$(CV).lnk
	link/batch/seg:1000 @$(OBJDIR)\$(PROD)$(CV).lnk
	rc -K /v -I. /I$(MSWINC) $(PROD).rc $(OBJDIR)\$(PROD)$(CV).exe
	$(MSWDIR)\mswchksm -s $(OBJDIR)\$(PROD)$(CV).exe

bdf2xtp.exe: bdf2xtp.c
	cl /AL /c bdf2xtp.c
	link /stack:5120 bdf2xtp.obj, bdf2xtp.exe, , , ,

trk2xtc.exe: trk2xtc.c
	cl /AL /c trk2xtc.c
	link /stack:5120 trk2xtc.obj, trk2xtc.exe, , , ,

hello:
	echo $(OBJDIR)
	echo $(MSWLIB)
