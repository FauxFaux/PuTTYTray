# Microsoft Developer Studio Project File - Name="pageant" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=pageant - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pageant.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pageant.mak" CFG="pageant - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pageant - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "pageant - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pageant - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\..\..\./" /I "..\..\..\charset/" /I "..\..\..\windows/" /I "..\..\..\unix/" /I "..\..\..\mac/" /I "..\..\..\macosx/" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\./" /I "..\..\..\charset/" /I "..\..\..\windows/" /I "..\..\..\unix/" /I "..\..\..\mac/" /I "..\..\..\macosx/" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 advapi32.lib comctl32.lib comdlg32.lib gdi32.lib imm32.lib shell32.lib user32.lib winmm.lib winspool.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "pageant - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\./" /I "..\..\..\charset/" /I "..\..\..\windows/" /I "..\..\..\unix/" /I "..\..\..\mac/" /I "..\..\..\macosx/" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\./" /I "..\..\..\charset/" /I "..\..\..\windows/" /I "..\..\..\unix/" /I "..\..\..\mac/" /I "..\..\..\macosx/" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 advapi32.lib comctl32.lib comdlg32.lib gdi32.lib imm32.lib shell32.lib user32.lib winmm.lib winspool.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "pageant - Win32 Release"
# Name "pageant - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\misc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshaes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshbn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshdes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshdss.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshmd5.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshpubk.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshrsa.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshsh512.c
# End Source File
# Begin Source File

SOURCE=..\..\..\sshsha.c
# End Source File
# Begin Source File

SOURCE=..\..\..\tree234.c
# End Source File
# Begin Source File

SOURCE=..\..\..\version.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winhelp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winmisc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winpgnt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winpgntc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winutils.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\charset\charset.h
# End Source File
# Begin Source File

SOURCE=..\..\..\int64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\mac\macstuff.h
# End Source File
# Begin Source File

SOURCE=..\..\..\macosx\osx.h
# End Source File
# Begin Source File

SOURCE=..\..\..\misc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\network.h
# End Source File
# Begin Source File

SOURCE=..\..\..\putty.h
# End Source File
# Begin Source File

SOURCE=..\..\..\puttymem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\puttyps.h
# End Source File
# Begin Source File

SOURCE=..\..\..\ssh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\tree234.h
# End Source File
# Begin Source File

SOURCE=..\..\..\unix\unix.h
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\rcstuff.h
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winhelp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\winstuff.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\windows\pageant.ico
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\pageant.rc
# End Source File
# Begin Source File

SOURCE=..\..\..\windows\pageants.ico
# End Source File
# End Group
# End Target
# End Project
