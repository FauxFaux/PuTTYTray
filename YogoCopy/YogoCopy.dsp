# Microsoft Developer Studio Project File - Name="YogoCopy" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=YogoCopy - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "YogoCopy.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "YogoCopy.mak" CFG="YogoCopy - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "YogoCopy - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "YogoCopy - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "YogoCopy - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_USRDLL" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 /nologo /subsystem:windows /dll /machine:I386
# Begin Special Build Tool
OutDir=.\Release
SOURCE="$(InputPath)"
PostBuild_Desc=Registering the COM Server
PostBuild_Cmds=regsvr32.exe /s /c $(OUTDIR)\YogoCopy.dll
# End Special Build Tool

!ELSEIF  "$(CFG)" == "YogoCopy - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_USRDLL" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
OutDir=.\Debug
SOURCE="$(InputPath)"
PostBuild_Desc=Registering the COM Server
PostBuild_Cmds=regsvr32.exe /s /c $(OUTDIR)\YogoCopy.dll
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "YogoCopy - Win32 Release"
# Name "YogoCopy - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AboutDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\CancelDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\FileProcess.cpp
# End Source File
# Begin Source File

SOURCE=.\ShellExt.cpp
# End Source File
# Begin Source File

SOURCE=.\ShellExtReg.cpp
# End Source File
# Begin Source File

SOURCE=.\ShellYogoCopy.cpp
# End Source File
# Begin Source File

SOURCE=.\ShUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\YogoCopy.cpp
# End Source File
# Begin Source File

SOURCE=.\YogoCopy.def
# End Source File
# Begin Source File

SOURCE=.\YogoCopy.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\CancelDlg.h
# End Source File
# Begin Source File

SOURCE=.\FileProcess.h
# End Source File
# Begin Source File

SOURCE=.\Priv.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\ShellExt.h
# End Source File
# Begin Source File

SOURCE=.\ShUtils.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\ThreadStruct.h
# End Source File
# Begin Source File

SOURCE=.\YogoCopy.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\ani_2.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_2.ico
# End Source File
# Begin Source File

SOURCE=.\ani_3.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_3.ico
# End Source File
# Begin Source File

SOURCE=.\ani_4.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_4.ico
# End Source File
# Begin Source File

SOURCE=.\ani_5.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_5.ico
# End Source File
# Begin Source File

SOURCE=.\ani_6.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_6.ico
# End Source File
# Begin Source File

SOURCE=.\ani_7.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_7.ico
# End Source File
# Begin Source File

SOURCE=.\ani_8.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_8.ico
# End Source File
# Begin Source File

SOURCE=.\ani_9.ico
# End Source File
# Begin Source File

SOURCE=.\res\ani_9.ico
# End Source File
# Begin Source File

SOURCE=.\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\menu_bmp.bmp
# End Source File
# Begin Source File

SOURCE=.\res\YogoCopy.rc2
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
