@for /f "usebackq delims=" %%i in (`git describe`) do set desc=%%i
for /f "tokens=2,3 delims=tv-" %%i in ("%desc%") do set trayversion=%%i & set traypatch=%%j
nmake /F Makefile.vc "VER=/DSNAPSHOT= /DSVN_REV=%desc% /DSVN_REV1=%trayversion% /DSVN_REV2=%traypatch%" "XFLAGS=/DDEBUG /Zi /Od /D_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES /W1 /RTC1 /GS" XLFLAGS=/debug putty.exe 2>&1 | findstr /v overriding | findstr /v _CRT_SECURE_NO_WARNINGS
