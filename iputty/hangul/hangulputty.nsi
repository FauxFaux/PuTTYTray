;
; Hangul PuTTY NSIS Installation Script
;
; Created by Hye-Shik Chang <perky@i18n.org>
;
; $Id$
;

!include "MUI.nsh"

;--------------------------------
;Version and Basic Scheme

!define MUI_PRODUCT "한글 PuTTY"
!define MUI_PRODUCT_NEUTRAL "HangulPuTTY"
!define MUI_VERSION "0.53b.h3"

!define REGISTRY_ROOT HKCU
!define REGISTRY_PATH "Software\SimonTatham\PuTTY"
!define SMGROUP "한글 PuTTY"
!define SMPATH "$SMPROGRAMS\${SMGROUP}"

!define PUTTYBASE "..\putty"

XPStyle on

;--------------------------------
;Configuration

  ;General
  OutFile "HangulPuTTY-${MUI_VERSION}.exe"

  ;Folder selection page
  InstallDir "C:\Program Files\HangulPuTTY ${MUI_VERSION}"
  
  ;Remember install folder
  InstallDirRegKey "${REGISTRY_ROOT}" "${REGISTRY_PATH}\InstallPath" ""

  ;Show its version
  BrandingText "${MUI_PRODUCT} ${MUI_VERSION}"

;--------------------------------
;Modern UI Configuration

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !define MUI_ABORTWARNING

  ;Icons and graphics
  !define MUI_ICON "contrib\yi-box_install.ico"
  !define MUI_UNICON "contrib\yi-box_uninstall.ico"
  !define MUI_CHECKBITMAP "contrib\yi-box_check.bmp"

  
;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "Korean"
  !insertmacro MUI_RESERVEFILE_LANGDLL
  
  LicenseData "LICENSE.txt"

;--------------------------------
;Installer Sections

Section ""

  SetOutPath "$INSTDIR"
  File /oname=LICENSE.txt "LICENSE.txt"
  File "${PUTTYBASE}\pageant.exe"
  File "${PUTTYBASE}\plink.exe"
  File "${PUTTYBASE}\pscp.exe"
  File "${PUTTYBASE}\psftp.exe"
  File "${PUTTYBASE}\putty.exe"
  File "${PUTTYBASE}\puttygen.exe"
  File "${PUTTYBASE}\puttytel.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "DisplayName" "${MUI_PRODUCT_NEUTRAL} ${MUI_VERSION}"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "InstallLocation" "$INSTDIR"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "DisplayIcon" "$INSTDIR\putty.exe,-0"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "DisplayVersion" "${MUI_VERSION}"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "Publisher" "Hye-Shik Chang"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "HelpLink" "http://openlook.org/wiki/HangulPuTTY"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}" "URLInfoAbout" "http://www.chiark.greenend.org.uk/~sgtatham/putty/"

  CreateDirectory "${SMPATH}"
  SetOutPath "$INSTDIR"
  CreateShortCut "${SMPATH}\PuTTY.lnk" "$INSTDIR\putty.exe" "" "$INSTDIR\putty.exe" 0
  CreateShortCut "${SMPATH}\Pageant (SSH 세션관리자).lnk" "$INSTDIR\pageant.exe" "" "$INSTDIR\pageant.exe" 0
  CreateShortCut "${SMPATH}\PuTTYgen (SSH 키 생성/관리자).lnk" "$INSTDIR\puttygen.exe" "" "$INSTDIR\puttygen.exe" 0
  CreateShortCut "${SMPATH}\PuTTYtel (텔넷 클라이언트).lnk" "$INSTDIR\puttytel.exe" "" "$INSTDIR\puttytel.exe" 0
  CreateShortCut "${SMPATH}\psftp (SFTP 콘솔 클라이언트).lnk" "$INSTDIR\psftp.exe" "" "$INSTDIR\psftp.exe" 0
  CreateShortCut "${SMPATH}\PuTTY 제거.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTIONS_DESCRIPTION_BEGIN
!insertmacro MUI_FUNCTIONS_DESCRIPTION_END
 

;Uninstaller Section

Section "Uninstall"

  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\pageant.exe"
  Delete "$INSTDIR\plink.exe"
  Delete "$INSTDIR\pscp.exe"
  Delete "$INSTDIR\psftp.exe"
  Delete "$INSTDIR\putty.exe"
  Delete "$INSTDIR\puttygen.exe"
  Delete "$INSTDIR\puttytel.exe"
  Delete "$INSTDIR\LICENSE.txt"

  Delete "${SMPATH}\*.*"
  RMDir "${SMPATH}"

  RMDir "$INSTDIR"

  ;----------------------------------------
  ;Uninstall Information
  DeleteRegKey ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT_NEUTRAL}-${MUI_VERSION}"

  DeleteRegKey ${REGISTRY_ROOT} "${REGISTRY_PATH}\InstallPath\InstallGroup"
  DeleteRegKey ${REGISTRY_ROOT} "${REGISTRY_PATH}\InstallPath"

  ;Display the Finish header
  !insertmacro MUI_UNFINISHHEADER

SectionEnd
