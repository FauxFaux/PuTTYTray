CPP = $(TOOLPATH)g++
AR = $(TOOLPATH)ar
RANLIB = $(TOOLPATH)ranlib

CFLAGS = -mno-cygwin -Wall -O2 -D_WINDOWS -DWIN32S_COMPAT \
		-DPERSOPORT  -DCYGTERMPORT -DPUTTYCYG -DIMAGEPORT -DRECONNECTPORT -DSCPORT -DHYPERLINKPORT -DZMODEMPORT \
		-D_NO_OLDNAMES -DNO_MULTIMON -DNO_HTMLHELP -I.././ \

url.a: urlhack.cpp urlhack.h re_lib/regerror.cpp re_lib/regexp.cpp re_lib/regsub.cpp
	$(CPP) $(COMPAT) $(XFLAGS) $(CFLAGS) -c urlhack.cpp
	$(CPP) $(COMPAT) $(XFLAGS) $(CFLAGS) -c re_lib/regerror.cpp
	$(CPP) $(COMPAT) $(XFLAGS) $(CFLAGS) -c re_lib/regexp.cpp
	$(CPP) $(COMPAT) $(XFLAGS) $(CFLAGS) -c re_lib/regsub.cpp
	$(AR) cr url.a urlhack.o regerror.o regexp.o regsub.o
	$(RANLIB) url.a
	cp url.a ..
	cp urlhack.h ..