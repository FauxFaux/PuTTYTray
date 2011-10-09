set trayversion=
for /f "tokens=2,3 delims=-t" %%i in ("%1") do set trayversion=%%i
if %trayversion%.==. echo Usage: %0 p0.61-t004 & goto end

nmake /F Makefile.vc "VER=/DRELEASE=%1 /DSVN_REV1=%trayversion%" "XFLAGS=/Zi /D_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES /GL /GS /Gy" "XLFLAGS=/debug /release /dynamicbase /nxcompat /ltcg" LFLAGS=/incremental:no putty.exe
:end