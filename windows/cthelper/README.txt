This is the README for PuTTYcyg.


ABOUT

PuTTYcyg is a patched version of PuTTY that, in addition to telnet,
rlogin, ssh, and serial connections, can also be used as a local Cygwin
terminal instead of the Windows console or xterm.  See RATIONALE section.

PuTTYcyg is available here: http://code.google.com/p/puttycyg/


REQUIREMENTS

PuTTYcyg requires Cygwin to be installed.  The Cygwin root directory is
automagically located when PuTTYcyg is run.


INSTALLATION

No installation is necessary.  When run with the cygterm backend, PuTTYcyg
requires cthelper.exe to be in the same directory as the PuTTYcyg executable
or in the PATH.  Since version 20070207, there are four files in the binary
release zip file:
  README.txt    - this file
  putty.exe     - PuTTY with cygterm, Telnet, and SSH clients
  puttytel.exe  - PuTTY with cygterm and Telnet clients (no SSH)
  cthelper.exe  - pty helper program; required when using the cygterm backend


USAGE

The Cygterm backend (or protocol) is made available in the standard
configuration dialog or on the command line via the -cygterm option.  The
command supplied in the configuration dialog or on the command line is used by
Cygterm as a command line to execute in the pseudoterminal.  On the command
line, the first non-option argument will end argument processing and the
remaining arguments will be taken as the command line to execute.

A command consisting of a single dash '-' will instead launch the current
user's default shell in Cygwin (from /etc/passwd) as a login shell (with
argv[0] set to "-").

The port number in the configuration dialog is ignored.

The terminal size, TERM type, and erase key are set based on PuTTY's
configuration dialog.  No other configuration settings are passed to the pseudo
terminal, although terminal resizing works correctly.

The additional PuTTY utilities (PSCP, Plink, Pageant, etc) are not distributed
with PuTTYcyg.  These can be obtained from the PuTTY website.

http://www.chiark.greenend.org.uk/~sgtatham/putty/


RATIONALE

The Windows Console is an inadequate terminal emulator.  It is impossible to
resize horizontally without pulling up a dialog box.  It is impossible to send
an ASCII NUL.  Basic keyboard options do not exist such as configuring the
ASCII character sent by the Backspace key.

Some solutions for these problems already exist.  One can use xterm or rxvt
instead of the Console.  One can also telnet or ssh to the local machine over
the loopback interface using almost any terminal emulator including PuTTY.

However, one should be able to use Cygwin with a decent terminal emulator
without having to install Cygwin/X or to install telnetd or sshd.


DESIGN

It isn't possible to build the pty support directly into PuTTY because PuTTY
links to MSVCRT while the pty support requires Cygwin.  It isn't even possible
to build the pty support module as a DLL to which PuTTY links; it has to be a
separate process.  Thus, cthelper was born.

With separate processes, there is a need for interprocess communication.  My
first attempt at IPC was to use stdin/stdout to communicate between PuTTYcyg
and cthelper.  This didn't go anywhere because it would be difficult (if not
impossible) to hook this in to PuTTY's event loop.  Windows does not provide
for window message notification (a la WSAAsyncSelect) for non-sockets which is
what PuTTY uses to know when a socket is ready for reading/writing.

My second attempt at IPC was to use a TCP connection over the loopback
interface.  PuTTYcyg opens a port and passes the port number to cthelper.  The
two processes pass messages in a certain format.  There are basically two
messages: one for pty data, one one for resize events.  This works just fine,
but it is inefficient as 99.99% of messages are pty data.

My third IPC design, therefore, uses the socket for pty data only.  A second
stream is required in order to send special messages such as terminal resize
events.  An anonymous one-way pipe from PuTTYcyg to the standard input of
cthelper is used for this purpose.  It seems to work fairly well.  If anyone
has a suggestion for a different way to do this, let me know.


IMPLEMENTATION NOTES

The following PuTTY source files are modified:

Recipe
  add cygcfg to GUITERM
  add cygterm to W_BE_ALL and W_BE_NOSSH
be_all.c
  add cygterm_backend
be_nos_c.c
  add cygterm_backend
cmdline.c
  add "-cygterm" command line option
config.c
  modify host/port controls
misc.c
  cygtem support for cfg_launchable() and cfg_dest() 
network.h
  declare sk_getport()
putty.h
  define PROT_CYGTERM enumeration
  declare cygterm_backend and cygterm_setup_config_box()
  cfg.cygcmd
  cfg.alt_metabit
settings.c
  AltMetaBit
  CygtermCommand
version.c
  PUTTYCYG version
windows/wincfg.c
  add metabit option
  call cygterm_setup_config_box()
windows/window.c
  read command instead of hostname from command line when -cygterm selected
  add "-" command line option
  add alternate key sequences:
    Ctrl-Backspace, Shift-Tab, Ctrl-Shift-Space,
    Ctrl-slash, Shift-Return, Ctrl-Return
  alt_metabit support
windows/winnet.c
  sk_getport()

The following source files are added:

windows/cygcfg.c
  cygterm_setup_config_box() adds Cygterm option to protocol selector
windows/cygterm.c
  the cygterm backend
windows/cthelper/*
  the source files for the cthelper pseudoterminal manager
