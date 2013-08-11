for /f "usebackq delims=" %%i in (`git describe`) do set desc=%%i
for /f "tokens=2,3 delims=tv-" %%i in ("%desc%") do set trayversion=%%i & set traypatch=%%j
if "%traypatch%" == "" set traypatch=9001
nmake /F Makefile.vc DEBUG=1 ^
  "VER=/DSNAPSHOT= /DSVN_REV=%desc% /DSVN_REV1=%trayversion% /DSVN_REV2=%traypatch%" ^
  putty.exe ^
  pageant.exe ^
  2>&1 ^
  | findstr /v overriding