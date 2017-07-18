set upstreammajor=
set upstreamminor=
set trayversion=
for /f "tokens=1,2 delims=p." %%i in ("%1") do set upstreammajor=%%i
for /f "tokens=2,3 delims=p.-" %%i in ("%1") do set upstreamminor=%%i
for /f "tokens=2,3 delims=-t" %%i in ("%1") do set trayversion=%%i
if %trayversion%.==. echo Usage: %0 p0.61-t004 & goto end

set INCLUDE=%PROGRAMFILES(x86)%\Microsoft SDKs\Windows\v7.1A\Include;%INCLUDE%
set LIB=%PROGRAMFILES(x86)%\Microsoft SDKs\Windows\v7.1A\lib;%LIB%

; remove leading zeros, fixing octal confusion
set /a trayversion=10000%trayversion% %% 10000

echo #define TEXTVER "PuTTYTray %1" > ../version.h
echo #define SSHVER "PuTTYTray-%1" >> ../version.h
echo #define BINARY_VERSION %upstreammajor%,%upstreamminor%,0,%trayversion% >> ../version.h

nmake /F Makefile.vc putty.exe plink.exe

signtool sign /a ^
  /fd SHA256 ^
  /tr http://tsa.startssl.com/rfc3161 ^
  /td SHA256 ^
  putty.exe ^
  plink.exe
:end
