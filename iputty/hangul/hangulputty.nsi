;
; Hangul PuTTY NSIS Installation Script
;
; Created by Hye-Shik Chang <perky@i18n.org>
;
; $Id$
;

;Icons and graphics
!define MUI_ICON "contrib\yi-box_install.ico"
!define MUI_UNICON "contrib\yi-box_uninstall.ico"
!define MUI_CHECKBITMAP "contrib\yi-box_check.bmp"

;Predefined constants
!define MUI_COMPONENTSPAGE_SMALLDESC

!include "MUI.nsh"

;--------------------------------
;Version and Basic Scheme

!define PRODUCT_NAME "한글 PuTTY"
!define PRODUCT_NEUTRAL "HangulPuTTY"
!define VERSION_NAME "0.55.h1"

!define REGISTRY_ROOT HKCU
!define REGISTRY_PATH "Software\SimonTatham\PuTTY"
!define SMGROUP "한글 PuTTY"
!define SMPATH "$SMPROGRAMS\${SMGROUP}"

!define PUTTYBASE "..\putty"

XPStyle on

;--------------------------------
;Configuration

  ;General
  Name "${PRODUCT_NAME} ${VERSION_NAME}"
  OutFile "HangulPuTTY-${VERSION_NAME}.exe"

  ;Folder selection page
  InstallDir "C:\Program Files\HangulPuTTY ${VERSION_NAME}"
  
  ;Remember install folder
  InstallDirRegKey "${REGISTRY_ROOT}" "${REGISTRY_PATH}\InstallPath" ""

  ;Show its version
  BrandingText "${PRODUCT_NAME} ${VERSION_NAME}"

;--------------------------------
;Modern UI Configuration

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "License.txt"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !define MUI_ABORTWARNING


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
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "DisplayName" "${PRODUCT_NEUTRAL} ${VERSION_NAME}"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "InstallLocation" "$INSTDIR"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "DisplayIcon" "$INSTDIR\putty.exe,-0"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "DisplayVersion" "${VERSION_NAME}"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "Publisher" "Hye-Shik Chang"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "HelpLink" "http://openlook.org/wiki/HangulPuTTY"
  WriteRegStr ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}" "URLInfoAbout" "http://www.chiark.greenend.org.uk/~sgtatham/putty/"

  CreateDirectory "${SMPATH}"
  SetOutPath "$INSTDIR"
  CreateShortCut "${SMPATH}\PuTTY (SSH 클라이언트).lnk" "$INSTDIR\putty.exe" "" "$INSTDIR\putty.exe" 0
  CreateShortCut "${SMPATH}\Pageant (SSH 세션관리자).lnk" "$INSTDIR\pageant.exe" "" "$INSTDIR\pageant.exe" 0
  CreateShortCut "${SMPATH}\PuTTYgen (SSH 키 생성/관리자).lnk" "$INSTDIR\puttygen.exe" "" "$INSTDIR\puttygen.exe" 0
  CreateShortCut "${SMPATH}\PuTTYtel (텔넷 클라이언트).lnk" "$INSTDIR\puttytel.exe" "" "$INSTDIR\puttytel.exe" 0
  CreateShortCut "${SMPATH}\psftp (SFTP 콘솔 클라이언트).lnk" "$INSTDIR\psftp.exe" "" "$INSTDIR\psftp.exe" 0
  CreateShortCut "${SMPATH}\PuTTY 제거.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_FUNCTION_DESCRIPTION_END
 

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
  DeleteRegKey ${REGISTRY_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NEUTRAL}-${VERSION_NAME}"

  DeleteRegKey ${REGISTRY_ROOT} "${REGISTRY_PATH}\InstallPath\InstallGroup"
  DeleteRegKey ${REGISTRY_ROOT} "${REGISTRY_PATH}\InstallPath"

SectionEnd
