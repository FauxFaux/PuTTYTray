# Visual C++ Makefile for PuTTY.
#
# Use `nmake' to build.
#

##-- help
#
# Extra options you can set:
#
#  - FWHACK=/DFWHACK
#      Enables a hack that tunnels through some firewall proxies.
#
#  - VER=/DSNAPSHOT=1999-01-25
#      Generates executables whose About box report them as being a
#      development snapshot.
#
#  - VER=/DRELEASE=0.43
#      Generates executables whose About box report them as being a
#      release version.
#
#  - COMPAT=/DAUTO_WINSOCK
#      Causes PuTTY to assume that <windows.h> includes its own WinSock
#      header file, so that it won't try to include <winsock.h>.
#
#  - COMPAT=/DWINSOCK_TWO
#      Causes the PuTTY utilities to include <winsock2.h> instead of
#      <winsock.h>, except Plink which _needs_ WinSock 2 so it already
#      does this.
#
#  - COMPAT=/DNO_SECURITY
#      Disables Pageant's use of <aclapi.h>, which is not available
#      with some development environments. This means that Pageant
#      won't care about the local user ID of processes accessing it; a
#      version of Pageant built with this option will therefore refuse
#      to run under NT-series OSes on security grounds (although it
#      will run fine on Win95-series OSes where there is no access
#      control anyway).
#
#      Note that this definition is always enabled in the Cygwin
#      build, since at the time of writing this <aclapi.h> is known
#      not to be available in Cygwin.
#
#  - COMPAT=/DNO_MULTIMON
#      Disables PuTTY's use of <multimon.h>, which is not available
#      with some development environments. This means that PuTTY's
#      full-screen mode (configurable to work on Alt-Enter) will
#      not behave usefully in a multi-monitor environment.
#
#  - COMPAT=/DMSVC4
#  - RCFL=/DMSVC4
#      Makes a couple of minor changes so that PuTTY compiles using
#      MSVC 4. You will also need /DNO_SECURITY and /DNO_MULTIMON.
#
#  - RCFL=/DASCIICTLS
#      Uses ASCII rather than Unicode to specify the tab control in
#      the resource file. Probably most useful when compiling with
#      Cygnus/mingw32, whose resource compiler may have less of a
#      problem with it.
#
#  - XFLAGS=/DDEBUG
#      Causes PuTTY to enable internal debugging.
#
#  - XFLAGS=/DMALLOC_LOG
#      Causes PuTTY to emit a file called putty_mem.log, logging every
#      memory allocation and free, so you can track memory leaks.
#
#  - XFLAGS=/DMINEFIELD
#      Causes PuTTY to use a custom memory allocator, similar in
#      concept to Electric Fence, in place of regular malloc(). Wastes
#      huge amounts of RAM, but should cause heap-corruption bugs to
#      show up as GPFs at the point of failure rather than appearing
#      later on as second-level damage.
#
##--

# Enable debug and incremental linking and compiling
# CFLAGS = /nologo /W3 /YX /Yd /O1 /Gi /D_WINDOWS /DDEBUG /D_WIN32_WINDOWS=0x401
# LFLAGS = /debug

# Disable debug and incremental linking and compiling
CFLAGS = /nologo /W3 /O1 /D_WINDOWS /D_WIN32_WINDOWS=0x401 /DWINVER=0x401
LFLAGS = /incremental:no /fixed

# Use MSVC DLL
# CFLAGS = /nologo /W3 /O1 /MD /D_WINDOWS /D_WIN32_WINDOWS=0x401
# LFLAGS = /incremental:no

.c.obj:
	cl $(COMPAT) $(FWHACK) $(XFLAGS) $(CFLAGS) /c $*.c

OBJ=obj
RES=res

##-- objects putty puttytel
GOBJS1 = window.$(OBJ) windlg.$(OBJ) winctrls.$(OBJ) terminal.$(OBJ)
GOBJS2 = sizetip.$(OBJ) wcwidth.$(OBJ) unicode.$(OBJ) logging.$(OBJ)
##-- objects putty puttytel plink
LOBJS1 = telnet.$(OBJ) raw.$(OBJ) rlogin.$(OBJ) ldisc.$(OBJ) winnet.$(OBJ)
##-- objects putty plink
POBJS = be_all.$(OBJ)
##-- objects puttytel
TOBJS = be_nossh.$(OBJ)
##-- objects plink
PLOBJS = plink.$(OBJ) logging.$(OBJ)
##-- objects pscp
SOBJS = scp.$(OBJ) winnet.$(OBJ) be_none.$(OBJ) wildcard.$(OBJ)
##-- objects psftp
FOBJS = psftp.$(OBJ) winnet.$(OBJ) be_none.$(OBJ)
##-- objects pscp psftp
SFOBJS = sftp.$(OBJ) int64.$(OBJ) logging.$(OBJ)
##-- objects putty puttytel pscp psftp plink
MOBJS = misc.$(OBJ) version.$(OBJ) winstore.$(OBJ) settings.$(OBJ)
MOBJ2 = tree234.$(OBJ)
##-- objects pscp psftp plink
COBJS = console.$(OBJ)
##-- objects putty pscp psftp plink
OBJS1 = sshcrc.$(OBJ) sshdes.$(OBJ) sshmd5.$(OBJ) sshrsa.$(OBJ) sshrand.$(OBJ)
OBJS2 = sshsha.$(OBJ) sshblowf.$(OBJ) noise.$(OBJ) sshdh.$(OBJ) sshcrcda.$(OBJ)
OBJS3 = sshpubk.$(OBJ) ssh.$(OBJ) pageantc.$(OBJ) sshzlib.$(OBJ) sshdss.$(OBJ)
OBJS4 = x11fwd.$(OBJ) portfwd.$(OBJ) sshaes.$(OBJ) sshsh512.$(OBJ) sshbn.$(OBJ)
##-- objects pageant
PAGE1 = pageant.$(OBJ) sshrsa.$(OBJ) sshpubk.$(OBJ) sshdes.$(OBJ) sshbn.$(OBJ)
PAGE2 = sshmd5.$(OBJ) version.$(OBJ) tree234.$(OBJ) misc.$(OBJ) sshaes.$(OBJ)
PAGE3 = sshsha.$(OBJ) pageantc.$(OBJ) sshdss.$(OBJ) sshsh512.$(OBJ)
##-- objects puttygen
GEN1 = puttygen.$(OBJ) sshrsag.$(OBJ) sshdssg.$(OBJ) sshprime.$(OBJ)
GEN2 = sshdes.$(OBJ) sshbn.$(OBJ) sshmd5.$(OBJ) version.$(OBJ) sshrand.$(OBJ)
GEN3 = noise.$(OBJ) sshsha.$(OBJ) winstore.$(OBJ) misc.$(OBJ) winctrls.$(OBJ)
GEN4 = sshrsa.$(OBJ) sshdss.$(OBJ) sshpubk.$(OBJ) sshaes.$(OBJ) sshsh512.$(OBJ)
##-- resources putty puttytel
PRESRC = win_res.$(RES)
##-- resources pageant
PAGERC = pageant.$(RES)
##-- resources puttygen
GENRC = puttygen.$(RES)
##-- resources pscp psftp
SRESRC = scp.$(RES)
##-- resources plink
LRESRC = plink.$(RES)
##--

##-- gui-apps
# putty
# puttytel
# pageant
# puttygen
##-- console-apps
# pscp
# psftp
# plink ws2_32
##--

LIBS1 = advapi32.lib user32.lib gdi32.lib
LIBS2 = comctl32.lib comdlg32.lib
LIBS3 = shell32.lib winmm.lib imm32.lib
SOCK1 = wsock32.lib
SOCK2 = ws2_32.lib

all: putty.exe puttytel.exe pscp.exe psftp.exe \
     plink.exe pageant.exe puttygen.exe

putty.exe: $(GOBJS1) $(GOBJS2) $(LOBJS1) $(POBJS) $(MOBJS) $(MOBJ2) $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4) $(PRESRC) putty.rsp
	link $(LFLAGS) -out:putty.exe -map:putty.map @putty.rsp

puttytel.exe: $(GOBJS1) $(GOBJS2) $(LOBJS1) $(TOBJS) $(MOBJS) $(MOBJ2) $(PRESRC) puttytel.rsp
	link $(LFLAGS) -out:puttytel.exe -map:puttytel.map @puttytel.rsp

pageant.exe: $(PAGE1) $(PAGE2) $(PAGE3) $(PAGERC) pageant.rsp
	link $(LFLAGS) -out:pageant.exe -map:pageant.map @pageant.rsp

puttygen.exe: $(GEN1) $(GEN2) $(GEN3) $(GEN4) $(GENRC) puttygen.rsp
	link $(LFLAGS) -out:puttygen.exe -map:puttygen.map @puttygen.rsp

pscp.exe: $(SOBJS) $(SFOBJS) $(COBJS) $(MOBJS) $(MOBJ2) $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4) $(SRESRC) pscp.rsp
	link $(LFLAGS) -out:pscp.exe -map:pscp.map @pscp.rsp

psftp.exe: $(FOBJS) $(SFOBJS) $(COBJS) $(MOBJS) $(MOBJ2) $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4) $(SRESRC) psftp.rsp
	link $(LFLAGS) -out:psftp.exe -map:psftp.map @psftp.rsp

plink.exe: $(LOBJS1) $(POBJS) $(PLOBJS) $(COBJS) $(MOBJS) $(MOBJ2) $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4) $(LRESRC) plink.rsp
	link $(LFLAGS) -out:plink.exe -map:plink.map @plink.rsp

ssh.obj:
	cl $(COMPAT) $(FWHACK) $(VER) $(XFLAGS) $(CFLAGS) /Gi- /c ssh.c

putty.rsp: makefile
	echo /nologo /subsystem:windows > putty.rsp
	echo $(GOBJS1) >> putty.rsp
	echo $(GOBJS2) >> putty.rsp
	echo $(LOBJS1) >> putty.rsp
	echo $(POBJS) >> putty.rsp
	echo $(MOBJS) >> putty.rsp
	echo $(MOBJ2) >> putty.rsp
	echo $(OBJS1) >> putty.rsp
	echo $(OBJS2) >> putty.rsp
	echo $(OBJS3) >> putty.rsp
	echo $(OBJS4) >> putty.rsp
	echo $(PRESRC) >> putty.rsp
	echo $(LIBS1) >> putty.rsp
	echo $(LIBS2) >> putty.rsp
	echo $(LIBS3) >> putty.rsp
	echo $(SOCK1) >> putty.rsp

puttytel.rsp: makefile
	echo /nologo /subsystem:windows > puttytel.rsp
	echo $(GOBJS1) >> puttytel.rsp
	echo $(GOBJS2) >> puttytel.rsp
	echo $(LOBJS1) >> puttytel.rsp
	echo $(TOBJS) >> puttytel.rsp
	echo $(MOBJS) >> puttytel.rsp
	echo $(MOBJ2) >> puttytel.rsp
	echo $(PRESRC) >> puttytel.rsp
	echo $(LIBS1) >> puttytel.rsp
	echo $(LIBS2) >> puttytel.rsp
	echo $(LIBS3) >> puttytel.rsp
	echo $(SOCK1) >> puttytel.rsp

pageant.rsp: makefile
	echo /nologo /subsystem:windows > pageant.rsp
	echo $(PAGE1) >> pageant.rsp
	echo $(PAGE2) >> pageant.rsp
	echo $(PAGE3) >> pageant.rsp
	echo $(PAGERC) >> pageant.rsp
	echo $(LIBS1) >> pageant.rsp
	echo $(LIBS2) >> pageant.rsp
	echo $(LIBS3) >> pageant.rsp

puttygen.rsp: makefile
	echo /nologo /subsystem:windows > puttygen.rsp
	echo $(GEN1) >> puttygen.rsp
	echo $(GEN2) >> puttygen.rsp
	echo $(GEN3) >> puttygen.rsp
	echo $(GEN4) >> puttygen.rsp
	echo $(GENRC) >> puttygen.rsp
	echo $(LIBS1) >> puttygen.rsp
	echo $(LIBS2) >> puttygen.rsp
	echo $(LIBS3) >> puttygen.rsp

pscp.rsp: makefile
	echo /nologo /subsystem:console > pscp.rsp
	echo $(SOBJS) >> pscp.rsp
	echo $(SFOBJS) >> pscp.rsp
	echo $(COBJS) >> pscp.rsp
	echo $(MOBJS) >> pscp.rsp
	echo $(MOBJ2) >> pscp.rsp
	echo $(OBJS1) >> pscp.rsp
	echo $(OBJS2) >> pscp.rsp
	echo $(OBJS3) >> pscp.rsp
	echo $(OBJS4) >> pscp.rsp
	echo $(SRESRC) >> pscp.rsp
	echo $(LIBS1) >> pscp.rsp
	echo $(LIBS2) >> pscp.rsp
	echo $(SOCK1) >> pscp.rsp

psftp.rsp: makefile
	echo /nologo /subsystem:console > psftp.rsp
	echo $(FOBJS) >> psftp.rsp
	echo $(SFOBJS) >> psftp.rsp
	echo $(COBJS) >> psftp.rsp
	echo $(MOBJS) >> psftp.rsp
	echo $(MOBJ2) >> psftp.rsp
	echo $(OBJS1) >> psftp.rsp
	echo $(OBJS2) >> psftp.rsp
	echo $(OBJS3) >> psftp.rsp
	echo $(OBJS4) >> psftp.rsp
	echo $(SRESRC) >> psftp.rsp
	echo $(LIBS1) >> psftp.rsp
	echo $(LIBS2) >> psftp.rsp
	echo $(SOCK1) >> psftp.rsp

plink.rsp: makefile
	echo /nologo /subsystem:console > plink.rsp
	echo $(LOBJS1) >> plink.rsp
	echo $(POBJS) >> plink.rsp
	echo $(PLOBJS) >> plink.rsp
	echo $(COBJS) >> plink.rsp
	echo $(MOBJS) >> plink.rsp
	echo $(MOBJ2) >> plink.rsp
	echo $(OBJS1) >> plink.rsp
	echo $(OBJS2) >> plink.rsp
	echo $(OBJS3) >> plink.rsp
	echo $(OBJS4) >> plink.rsp
	echo $(LRESRC) >> plink.rsp
	echo $(LIBS1) >> plink.rsp
 	echo $(LIBS2) >> plink.rsp
 	echo $(SOCK2) >> plink.rsp

##-- dependencies
be_all.$(OBJ): be_all.c network.h misc.h puttymem.h putty.h 
be_none.$(OBJ): be_none.c network.h misc.h puttymem.h putty.h 
be_nossh.$(OBJ): be_nossh.c network.h misc.h puttymem.h putty.h 
ber.$(OBJ): ber.c network.h asn.h misc.h asnerror.h int64.h puttymem.h ssh.h putty.h 
cert.$(OBJ): cert.c asn.h asnerror.h misc.h puttymem.h cert.h crypto.h 
debug.$(OBJ): debug.c debug.h 
int64.$(OBJ): int64.c int64.h 
ldisc.$(OBJ): ldisc.c network.h misc.h puttymem.h putty.h 
misc.$(OBJ): misc.c network.h misc.h puttymem.h putty.h 
mscrypto.$(OBJ): mscrypto.c network.h int64.h puttymem.h ssh.h 
no_ssl.$(OBJ): no_ssl.c network.h misc.h puttymem.h putty.h 
noise.$(OBJ): noise.c network.h misc.h puttymem.h storage.h int64.h ssh.h putty.h 
pageant.$(OBJ): pageant.c network.h int64.h puttymem.h ssh.h tree234.h 
pageantc.$(OBJ): pageantc.c puttymem.h 
plink.$(OBJ): plink.c network.h misc.h puttymem.h storage.h putty.h tree234.h 
portfwd.$(OBJ): portfwd.c network.h misc.h int64.h puttymem.h ssh.h putty.h 
psftp.$(OBJ): psftp.c network.h misc.h sftp.h ssh.h storage.h int64.h puttymem.h putty.h 
puttygen.$(OBJ): puttygen.c network.h misc.h int64.h puttymem.h winstuff.h ssh.h putty.h 
raw.$(OBJ): raw.c network.h misc.h puttymem.h putty.h 
rlogin.$(OBJ): rlogin.c network.h misc.h puttymem.h putty.h 
scp.$(OBJ): scp.c network.h misc.h sftp.h ssh.h storage.h puttymem.h int64.h putty.h winstuff.h 
settings.$(OBJ): settings.c network.h misc.h puttymem.h storage.h putty.h 
sftp.$(OBJ): sftp.c sftp.h int64.h 
sizetip.$(OBJ): sizetip.c network.h misc.h puttymem.h winstuff.h putty.h 
ssh.$(OBJ): ssh.c network.h misc.h int64.h puttymem.h ssh.h putty.h tree234.h 
sshaes.$(OBJ): sshaes.c network.h int64.h puttymem.h ssh.h 
sshblowf.$(OBJ): sshblowf.c network.h int64.h puttymem.h ssh.h 
sshbn.$(OBJ): sshbn.c network.h misc.h int64.h puttymem.h ssh.h putty.h 
sshcrc.$(OBJ): sshcrc.c 
sshdes.$(OBJ): sshdes.c network.h int64.h puttymem.h ssh.h 
sshdh.$(OBJ): sshdh.c network.h int64.h puttymem.h ssh.h 
sshdss.$(OBJ): sshdss.c network.h misc.h int64.h puttymem.h ssh.h 
sshdssg.$(OBJ): sshdssg.c network.h misc.h int64.h puttymem.h ssh.h 
sshmd5.$(OBJ): sshmd5.c network.h int64.h puttymem.h ssh.h 
sshprime.$(OBJ): sshprime.c network.h int64.h puttymem.h ssh.h 
sshpubk.$(OBJ): sshpubk.c network.h int64.h puttymem.h ssh.h 
sshrand.$(OBJ): sshrand.c network.h int64.h puttymem.h ssh.h 
sshrsa.$(OBJ): sshrsa.c network.h int64.h puttymem.h ssh.h 
sshrsag.$(OBJ): sshrsag.c network.h int64.h puttymem.h ssh.h 
sshsh512.$(OBJ): sshsh512.c network.h int64.h puttymem.h ssh.h 
sshsha.$(OBJ): sshsha.c network.h int64.h puttymem.h ssh.h 
sshzlib.$(OBJ): sshzlib.c network.h int64.h puttymem.h ssh.h 
ssl.$(OBJ): ssl.c network.h asnerror.h misc.h cert.h crypto.h ssl.h int64.h puttymem.h 
telnet.$(OBJ): telnet.c network.h misc.h puttymem.h putty.h 
terminal.$(OBJ): terminal.c network.h misc.h puttymem.h putty.h tree234.h 
logging.$(OBJ): logging.c misc.h puttymem.h putty.h
test.$(OBJ): test.c network.h int64.h puttymem.h ssh.h 
tree234.$(OBJ): tree234.c tree234.h 
unicode.$(OBJ): unicode.c network.h misc.h puttymem.h putty.h 
version.$(OBJ): version.c 
wcwidth.$(OBJ): wcwidth.c 
wildcard.$(OBJ): wildcard.c 
winctrls.$(OBJ): winctrls.c network.h misc.h puttymem.h putty.h winstuff.h 
windlg.$(OBJ): windlg.c network.h misc.h ssh.h storage.h puttymem.h int64.h putty.h winstuff.h win_res.h 
window.$(OBJ): window.c network.h misc.h puttymem.h storage.h winstuff.h putty.h win_res.h 
winnet.$(OBJ): winnet.c network.h misc.h puttymem.h putty.h tree234.h 
winstore.$(OBJ): winstore.c network.h misc.h puttymem.h storage.h putty.h 
x11fwd.$(OBJ): x11fwd.c network.h misc.h int64.h puttymem.h ssh.h putty.h 
##--

# Hack to force version.obj to be rebuilt always
version.obj: *.c *.h *.rc
	cl $(FWHACK) $(VER) $(CFLAGS) /c version.c

##-- dependencies
win_res.$(RES): win_res.rc win_res.h putty.ico
##--
win_res.$(RES):
	rc $(FWHACK) $(RCFL) -r -DWIN32 -D_WIN32 -DWINVER=0x0400 win_res.rc

##-- dependencies
scp.$(RES): scp.rc scp.ico
##--
scp.$(RES):
	rc $(FWHACK) $(RCFL) -r -DWIN32 -D_WIN32 -DWINVER=0x0400 scp.rc

##-- dependencies
pageant.$(RES): pageant.rc pageant.ico pageants.ico
##--
pageant.$(RES):
	rc $(FWHACK) $(RCFL) -r -DWIN32 -D_WIN32 -DWINVER=0x0400 pageant.rc

##-- dependencies
puttygen.$(RES): puttygen.rc puttygen.ico
##--
puttygen.$(RES):
	rc $(FWHACK) $(RCFL) -r -DWIN32 -D_WIN32 -DWINVER=0x0400 puttygen.rc

clean: tidy
	-del *.exe

tidy:
	-del *.obj
	-del *.res
	-del *.pch
	-del *.aps
	-del *.ilk
	-del *.pdb
	-del *.rsp
	-del *.dsp
	-del *.dsw
	-del *.ncb
	-del *.opt
	-del *.plg
	-del *.map
	-del *.idb
	-del debug.log
