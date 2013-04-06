@for /f "usebackq delims=" %%i in (`git describe`) do set desc=%%i
for /f "tokens=2,3 delims=tv-" %%i in ("%desc%") do set trayversion=%%i & set traypatch=%%j
if "%traypatch%" == "" set traypatch=9001
nmake /F Makefile.vc "VER=/DSNAPSHOT= /DSVN_REV=%desc% /DSVN_REV1=%trayversion% /DSVN_REV2=%traypatch%" "XFLAGS=/DDEBUG /Zi /Od /RTC1 /GS" XLFLAGS=/debug putty.exe 2>&1 | findstr /v "D9025 : overriding '/O1' with '/Od'"
