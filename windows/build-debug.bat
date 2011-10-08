@rem findstr warning | 
nmake /F Makefile.vc "XFLAGS=/DDEBUG /Zi /Od /D_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES /W1 /RTC1 /GS" XLFLAGS=/debug putty.exe 2>&1 | findstr /v overriding | findstr /v _CRT_SECURE_NO_WARNINGS
