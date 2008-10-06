YogoCopy, Copyright 1999, Smaller Animals Software
-------------------------------------------------
  This code may be modifed and distributed free of charge or restrictions.
  This code is provided as-is. If you use this code in any application,
any bugs in the code are your responsibility.


General Info
------------
  YogoCopy demonstrates a simple Explorer context menu extension. This allows
you to add right-click functionality to Explorer, My Computer or any other
standard file browse dialog. 

  This code is based, in part, on the SHELLEX example from Microsoft. 

  Yes, this is some complicated stuff. It isn't rocket science, it's just
a bit different than normal MFC apps. If you really want it to work, it will. 

  Before you release anything built with this code, you must create your own
GUID. Use GUIDGEN and insert the "DEFINE_GUID(..)" code into YogoCopy.h. You
must do this for every shell extension you build - each GUID is unique and
is used to identify a particular COM object (a shell extension is a COM object).

COM Server Registration
-----------------------
  Each time you build this DLL, or when you call regsvr32.exe /s /c YogoCopy.dll,
RegSvr32 will call the DLLRegisterServer function in the extension DLL. This 
function places entries in the system Registry which will tell Explorer that 
our context menu extension should be activated every time a file is selected 
with a right-click. You can limit the file extensions for which this DLL is 
activated by changing the strings in the RegisterFileMenu (ShellExtReg.cpp) 
function. The DLLUnregisterServer function removes these entries from the system 
Registry. It is important that you remove any Registry entries that you add, so 
be sure to keep these functions in-sync as you make changes.

  The project should be set up to call RegSvr32 each time a build is performed.
This is done in Project / Settings / Post-Build Step. The 'Post-build Command'
is "regsvr32.exe /s /c $(OUTDIR)\YogoCopy.dll". If this doesn't happen, you will
not be able to use your shell extension.

  If you release a shell extension, you will have to insure that the installer
registers the DLL. Most installer packages have options to do this.

General Operation
-----------------
  By way of a lot of COM magic, and using the Registry entries described 
above, Explorer will use this DLL to create an instance of an object with a 
IContextMenu / IShellExtInit interface. This object is an instance of our 
CShellExt class.

  The specific menu items for this extension are added when Explorer calls 
CShellExt::QueryContextMenu. You should change these menu strings and the menu 
bitmap.

  Help command text is generated in CShellExt::GetCommandString. You should 
change the help text.

  The list of files that have been selected is generated in 
CShellExt::Initialize. In this example, we put these file names into a 
CStringArray called CShellExt::m_csaPaths. Warning : this array may contain 
both files and folders. Be careful...

  The real work happens when the user selects one of our menu items. Explorer 
will call out CShellExt::InvokeCommand. Once we have determined which menu 
item was selected, we can process the files in CShellExt::m_csaPaths.

Specific Operation
------------------
  Thread info
  -----------
  YogoCopy uses threads. Because data is being used by all threads, a 
mechanism to insure safe access to variables is used; these are the 
GET_SAFE and SET_SAFE templates (see ShUtils.h). These functions wrap all 
accesses to any information that can be shared by more than one thread in 
a CRITICAL_SECTION, g_critSectionBreak. You will probably never have to 
deal with the CRITICAL_SECTION code directly, just use the GET_SAFE and 
SET_SAFE functions when accessing any data that may be shared between threads.

  To minimize the possibility of deadlock, you should use GET_SAFE and SET_SAFE
sparingly. Ex. if you are repeatedly accessing a piece of shared data that will not 
change in the context of your function, it's always best to copy the data to a 
local variable and only access the local. This is because only one thread at
a time may execute a GET_SAFE or SET_SAFE call. Ex. If one thread is in a 
GET_SAFE call, all other threads are blocked from entering GET_SAFE or SET_SAFE
until the first thread is done. 

  Cancel dialog
  -------------
  A progress / cancel dialog is created at the start of 
CShellExt::InvokeCommand. This dialog is used to inform the user of the 
progress of the operation and to provide a way to abort the process if 
necessary. You should change the logo and animation icons that this dialog 
uses. Free free to change the layout and appearance of the dialog. 

  Processing files
  ----------------
  When CShellExt::InvokeCommand is called, it loops over the files in 
m_csaPaths, updating the process dialog and sending each filename off to
the file processing code.

  Note : This example assumes your extension will do a lot of lengthy file 
processing (encryption, compression, copying, etc..). If you are doing 
something very simple, you can probably skip all of the multi-threading stuff 
and just handle the work in the InvokeCommand loop. It's up to you.

   For each file in the array, a new worker thread is created. The worker 
thread function is FileProcessThreadFunc (FileProcess.cpp). The thread is 
destroyed when FileProcessThreadFunc exits. 

  Data can only be passed to worker threads by a single void * parameter. 
In this example, I've used a structure called ThreadInfo to hold the shared 
data. You can add as many members to this function as you want. Be careful 
about the kinds of data you share between threads though; it's not a good 
idea to pass window handles between threads; most other types should be safe. 
Be sure to use GET_SAFE and SET_SAFE, or your own synchronization mechanism 
to insure that only one thread is accessing the variable at a time.

  FileProcessThreadFunc is where you get to do the real work. This is also 
where you can decide the specific implementation. In this example, 
FileProcessThreadFunc sets the shared thread flags and creates an instance of a 
CFileProcess object. This object just updates the shared progress flags slowly, 
to give the illusion that some work is being done. You can use CFileProcess as 
an example of how to use the progress updates and to handle the stop/cancel 
signals.

Debugging and setup
-------------------
  Debugging shell extensions requires much more work than normal applications. 
This is because shell extensions run from within Explorer, not as isolated 
processes. So, to debug one of these, you have to force Explorer to run within 
DevStudio. Luckily, this is possible.

  Setup
  -----
  First, I _strongly_ recommend that you add the following items to your 
DevStudio "Tools" menu : 

   Explorer    : (C:\Windows\Explorer.exe or c:\Winnt\Explorer.exe) 
   PView       : (C:\Program Files\DevStudio\VC\bin\winnt\Pview.exe or
                  in a similar directory on 95/98)

  If you are using NT, I also recommend that you set the following
Registry entry to '0' : 
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon\AutoRestartShell

  This disables auto-restart of Explorer. You want Explorer to be running 
within DevStudio and if you don't change this, you won't be able to kill the
existing Explorer process long enough for you to launch your own. You can 
change this back when you're done.

  For safety, I also like to start a DOS window before debugging. If you crash 
DevStudio after Explorer has been killed, you'll be in sad shape if you can't 
start a new Explorer instance (no taskbar no start menu, no desktop icons, 
etc..). You can always start Explorer from a DOS box by typing
C:\Windows\Explorer.EXE or C:\WinNT\Explorer.exe.

  Debugging
  ---------
  To debug, first close all Explorer windows. Then, using PView, kill the
Explorer process. Your taskbar and desktop icons should disappear. Now, run 
the project. If DevStudio asks you to specify an executable for the debug 
session, enter c:\windows\explorer.exe (or c:\winnt\explorer.exe). It will
complain that there is no debug info in the EXE; acknowledge this and continue.

  DevStudio will start Explorer. You will see a bunch of "DLL Loaded" messages
and then, nothing will happen. This is because you have only started the basic
Explorer stuff - desktop and taskbar. To debug your DLL, you'll need an Explorer
window. So, go to your tools menu and choose Explorer. A normal Explorer window
will pop up. Right click on any file, and watch your debug window - a bunch of
debug messages will fly by as your context menu extension is hit with a barrage
of COM calls from Explorer. Put a breakpoint at the top of 
CShellExt::InvokeCommand. Right click a file and choose your shell extension 
menu from the Explorer context menu, DevStudio will stop on your breakpoint and
you can debug your DLL as you would any other app.

  To stop debugging, close all Explorer windows. This is when DevStudio will 
report any memory leaks that may have happened because your app's CWinApp 
object will shut down. Then, choose Debug / Stop Debugging to finish. 

  When you are done, you will want to start a normal Explorer session, so 
that you can use your desktop and taskbar, so choose Explorer from your Tools
menu. This will start the basic Explorer background stuff.

  Note, there's a good chance that you'll see other debug messages from other
shell extensions in your debug window. This is because the original Microsoft
example (SHELLEX) did not turn off debug messages for release builds. And, 
there are a lot of extensions out there that are based on SHELLEX.

File descriptions
-----------------

File                What you need to change
------------------------------------------------------------------------------
YogoCopy.h         - You will need to insert your DEFINE_GUID(...) code here
                    before you release your extension.

YogoCopy.cpp       - Contains the basic CWinApp code and some required COM
                    DLL functions. You should not have to change anything in 
                    here.

ShellExtReg.cpp   - Contains the code that sets up the Registry entries for
                    your DLL. You shouldn't have to change anything here, but
                    additional Registry entries may be added as you wish, 

ShellExt.cpp/.h   - Contains most of the core functions required by context 
                    menu extensions. You should not have to change anything 
                    in here.

ShellYogoCopy.cpp  - This contains the three routines that control menu text,
                    appearance and command handling. Change the default menu 
                    text and help text. All of the progress dialog and thread 
                    creation code is in here. Part of CShellExt.

FileProcess.cpp   - This contains the file processing thread function. This 
                    is where you will do the actual file processing work.

SHUtils.cpp       - Contains some handy utility functions. 

SHUtils.h         - Defines our thread-safe data functions, the context menu
                    name and some other macros. You should only have to change
                    SHELLEXNAME.

AboutDlg.ccp/.h   - The application's about dialog - customize as you wish.

CancelDlg.cpp/.h  - The progress dialog - the basic implementation should be
                    O.K., but customize as you wish. Change the animation
                    and logo icons.

ThreadStruct.h    - Contains the definition of ThreadInfo. This is the 
                    structure used to pass data from the UI thread to the
                    worker thread. Add members as you wish.

Priv.h            - Some OLE-specific include stuff

Notes
-----

  Folder-only extensions
  ----------------------
  If you want to be able to handle only folders in your context menu extension,
you will need to make this change.

  In RegisterFileMenu and UnregisterFileMenu, you will need to change the line 
	REGSTRUCT OtherShExEntries[] = {  
			HKEY_CLASSES_ROOT, TEXT("*\\shellex\\ContextMenuHandlers\\"SHELLEXNAME), NULL, TEXT("%s"),

  to

  	REGSTRUCT OtherShExEntries[] = {  
      // this is for folders...
	   HKEY_CLASSES_ROOT, TEXT("Folder\\shellex\\ContextMenuHandlers\\"SHELLEXNAME), NULL, TEXT("%s"),

  Server Registration
  -------------------
  If you release a shell extension, you will have to insure that the installer
registers the DLL. Most installer packages have options to do this.


  Support
  -------
  I don't know anything about the other kinds of extensions that are 
possible : property pages, namespace extensions, etc.. Don't ask. 

  I'll answer questions you may have, but understand that this code is
free - I've done a lot of the work for you already - it's up to you to do
the rest. :)

   -chris
   smallest@smalleranimals.com