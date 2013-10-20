[LangOptions]
LanguageName=English
LanguageID=$0409
LanguageCodePage=0

[CustomMessages]

MyAppName=PuTTY GottaNi Edition
MyAppVerName=PuTTY version 0.60 GottaNi Edition
MyVersionDescription=PuTTY GottaNi Edition Setup
MyVersionInfo=version 0.60 GottaNi Edition
PuTTYShortcutName=PuTTY
PuTTYComment=SSH, Telnet and Rlogin client
PuTTYManualShortcutName=PuTTY Manual
PuTTYWebSiteShortcutName=PuTTY Web Site
PSFTPShortcutName=PSFTP
PSFTPComment=Command-line interactive SFTP client
PuTTYgenShortcutName=PuTTYgen
PuTTYgenComment=PuTTY SSH key generation utility
PageantShortcutName=Pageant
PageantComment=PuTTY SSH authentication agent
AdditionalIconsGroupDescription=Additional icons:
desktopiconDescription=Create a &desktop icon for PuTTY
desktopicon_commonDescription=For all users
desktopicon_userDescription=For the current user only
quicklaunchiconDescription=Create a &Quick Launch icon for PuTTY (current user only)
OtherTasksGroupDescription=Other tasks:
associateDescription=&Associate .PPK files (PuTTY Private Key) with Pageant and PuTTYgen
PPKTypeDescription=PuTTY Private Key File
PPKEditLabel=&Edit
UninstallStatusMsg=Cleaning up saved sessions etc (optional)...
SaveiniDescription=Save settings to INI file
PfwdComponentDescription=pfwd(Port forwarder)
PlinkwComponentDescription=plinkw(Plink without console)
JpnLngComponentDescription=Japanese language files
SourceComponentDescription=Source code(patch only)

[Messages]
; Since it's possible for the user to be asked to restart their computer,
; we should override the default messages to explain exactly why, so they
; can make an informed decision. (Especially as 95% of users won't need or
; want to restart; see rant above.)
FinishedRestartLabel=One or more [name] programs are still running. Setup will not replace these program files until you restart your computer. Would you like to restart now?
; This message is popped up in a message box on a /SILENT install.
FinishedRestartMessage=One or more [name] programs are still running.%nSetup will not replace these program files until you restart your computer.%n%nWould you like to restart now?
; ...and this comes up if you try to uninstall.
UninstalledAndNeedsRestart=One or more %1 programs are still running.%nThe program files will not be removed until your computer is restarted.%n%nWould you like to restart now?
