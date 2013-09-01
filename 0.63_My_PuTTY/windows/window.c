/*
 * window.c - the PuTTY(tel) main program, which runs a PuTTY terminal
 * emulator and backend in a window.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#ifndef NO_MULTIMON
#define COMPILE_MULTIMON_STUBS
#endif

#define PUTTY_DO_GLOBALS	       /* actually _define_ globals */
#include "putty.h"
#include "terminal.h"
#include "storage.h"
#include "win_res.h"

#ifndef NO_MULTIMON
#include <multimon.h>
#endif

#include <imm.h>
#include <commctrl.h>
#include <richedit.h>
#include <mmsystem.h>

/* From MSDN: In the WM_SYSCOMMAND message, the four low-order bits of
 * wParam are used by Windows, and should be masked off, so we shouldn't
 * attempt to store information in them. Hence all these identifiers have
 * the low 4 bits clear. Also, identifiers should < 0xF000. */

#define IDM_SHOWLOG   0x0010
#define IDM_NEWSESS   0x0020
#define IDM_DUPSESS   0x0030
#define IDM_RESTART   0x0040
#define IDM_RECONF    0x0050
#define IDM_CLRSB     0x0060
#define IDM_RESET     0x0070
#define IDM_HELP      0x0140
#define IDM_ABOUT     0x0150
#define IDM_SAVEDSESS 0x0160
#define IDM_COPYALL   0x0170
#define IDM_FULLSCREEN	0x0180
#define IDM_PASTE     0x0190
#define IDM_SPECIALSEP 0x0200

#define IDM_SPECIAL_MIN 0x0400
#define IDM_SPECIAL_MAX 0x0800

#define IDM_SAVED_MIN 0x1000
#define IDM_SAVED_MAX 0x5000
#define MENU_SAVED_STEP 16
/* Maximum number of sessions on saved-session submenu */
#define MENU_SAVED_MAX ((IDM_SAVED_MAX-IDM_SAVED_MIN) / MENU_SAVED_STEP)

#define WM_IGNORE_CLIP (WM_APP + 2)
#define WM_FULLSCR_ON_MAX (WM_APP + 3)
#define WM_AGENT_CALLBACK (WM_APP + 4)
#define WM_GOT_CLIPDATA (WM_APP + 6)

/* Needed for Chinese support and apparently not always defined. */
#ifndef VK_PROCESSKEY
#define VK_PROCESSKEY 0xE5
#endif

/* Mouse wheel support. */
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A	       /* not defined in earlier SDKs */
#endif
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

static Mouse_Button translate_button(Mouse_Button button);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static int TranslateKey(UINT message, WPARAM wParam, LPARAM lParam,
			unsigned char *output);
static void cfgtopalette(void);
static void systopalette(void);
static void init_palette(void);
static void init_fonts(int, int);
static void another_font(int);
static void deinit_fonts(void);
static void set_input_locale(HKL);
static void update_savedsess_menu(void);
static void init_flashwindow(void);

static int is_full_screen(void);
static void make_full_screen(void);
static void clear_full_screen(void);
static void flip_full_screen(void);
static int process_clipdata(HGLOBAL clipdata, int unicode);

/* Window layout information */
static void reset_window(int);
static int extra_width, extra_height;
static int font_width, font_height, font_dualwidth, font_varpitch;
static int offset_width, offset_height;
static int was_zoomed = 0;
static int prev_rows, prev_cols;
  
static int pending_netevent = 0;
static WPARAM pend_netevent_wParam = 0;
static LPARAM pend_netevent_lParam = 0;
static void enact_pending_netevent(void);
static void flash_window(int mode);
static void sys_cursor_update(void);
static int get_fullscreen_rect(RECT * ss);

static int caret_x = -1, caret_y = -1;

static int kbd_codepage;

static void *ldisc;
static Backend *back;
static void *backhandle;

static struct unicode_data ucsdata;
static int must_close_session, session_closed;
static int reconfiguring = FALSE;

static const struct telnet_special *specials = NULL;
static HMENU specials_menu = NULL;
static int n_specials = 0;

static wchar_t *clipboard_contents;
static size_t clipboard_length;

#define TIMING_TIMER_ID 1234
static long timing_next_time;

static struct {
    HMENU menu;
} popup_menus[2];
enum { SYSMENU, CTXMENU };
static HMENU savedsess_menu;

Config cfg;			       /* exported to windlg.c */

static struct sesslist sesslist;       /* for saved-session menu */

struct agent_callback {
    void (*callback)(void *, void *, int);
    void *callback_ctx;
    void *data;
    int len;
};

#define FONT_NORMAL 0
#define FONT_BOLD 1
#define FONT_UNDERLINE 2
#define FONT_BOLDUND 3
#define FONT_WIDE	0x04
#define FONT_HIGH	0x08
#define FONT_NARROW	0x10

#define FONT_OEM 	0x20
#define FONT_OEMBOLD 	0x21
#define FONT_OEMUND 	0x22
#define FONT_OEMBOLDUND 0x23

#define FONT_MAXNO 	0x2F
#define FONT_SHIFT	5
static HFONT fonts[FONT_MAXNO];
static LOGFONT lfont;
static int fontflag[FONT_MAXNO];
static enum {
    BOLD_COLOURS, BOLD_SHADOW, BOLD_FONT
} bold_mode;
static enum {
    UND_LINE, UND_FONT
} und_mode;
static int descent;

#define NCFGCOLOURS 22
#define NEXTCOLOURS 240
#define NALLCOLOURS (NCFGCOLOURS + NEXTCOLOURS)
static COLORREF colours[NALLCOLOURS];
static HPALETTE pal;
static LPLOGPALETTE logpal;
static RGBTRIPLE defpal[NALLCOLOURS];

static HBITMAP caretbm;

#ifdef PERSOPORT
#include "../../kitty.c"
#endif

#if (defined IMAGEPORT) && (!defined FDJ)
#include "../../kitty_image.h"
#endif
#ifdef RECONNECTPORT
static time_t last_reconnect = 0;
#endif
#ifdef HYPERLINKPORT
/*
 * HACK: PuttyTray / Nutty
 */ 
#include "../../urlhack.h"
static int urlhack_cursor_is_hand = 0;
#endif

static int dbltime, lasttime, lastact;
static Mouse_Button lastbtn;

/* this allows xterm-style mouse handling. */
static int send_raw_mouse = 0;
static int wheel_accumulator = 0;

static int busy_status = BUSY_NOT;

static char *window_name, *icon_name;

static int compose_state = 0;

static UINT wm_mousewheel = WM_MOUSEWHEEL;

/* Dummy routine, only required in plink. */
void ldisc_update(void *frontend, int echo, int edit)
{
}

char *get_ttymode(void *frontend, const char *mode)
{
    return term_get_ttymode(term, mode);
}

static void start_backend(void)
{
    const char *error;
    char msg[1024], *title;
    char *realhost;
    int i;

    /*
     * Select protocol. This is farmed out into a table in a
     * separate file to enable an ssh-free variant.
     */
    back = backend_from_proto(cfg.protocol);
    if (back == NULL) {
	char *str = dupprintf("%s Internal Error", appname);
	MessageBox(NULL, "Unsupported protocol number found",
		   str, MB_OK | MB_ICONEXCLAMATION);
	sfree(str);
	cleanup_exit(1);
    }

    error = back->init(NULL, &backhandle, &cfg,
		       cfg.host, cfg.port, &realhost, cfg.tcp_nodelay,
		       cfg.tcp_keepalives);
    back->provide_logctx(backhandle, logctx);
    if (error) {
	char *str = dupprintf("%s Error", appname);
	sprintf(msg, "Unable to open connection to\n"
		"%.800s\n" "%s", cfg_dest(&cfg), error);
	MessageBox(NULL, msg, str, MB_ICONERROR | MB_OK);
	sfree(str);
	exit(0);
    }
    window_name = icon_name = NULL;
    if (*cfg.wintitle) {
	title = cfg.wintitle;
    } else {
	sprintf(msg, "%s - %s", realhost, appname);
	title = msg;
    }
    sfree(realhost);
    set_title(NULL, title);
    set_icon(NULL, title);

    /*
     * Connect the terminal to the backend for resize purposes.
     */
    term_provide_resize_fn(term, back->size, backhandle);

    /*
     * Set up a line discipline.
     */
    ldisc = ldisc_create(&cfg, term, back, backhandle, NULL);

    /*
     * Destroy the Restart Session menu item. (This will return
     * failure if it's already absent, as it will be the very first
     * time we call this function. We ignore that, because as long
     * as the menu item ends up not being there, we don't care
     * whether it was us who removed it or not!)
     */
    for (i = 0; i < lenof(popup_menus); i++) {
	DeleteMenu(popup_menus[i].menu, IDM_RESTART, MF_BYCOMMAND);
    }

    must_close_session = FALSE;
    session_closed = FALSE;
}

static void close_session(void)
{
    char morestuff[100];
    int i;

    session_closed = TRUE;
    sprintf(morestuff, "%.70s (inactive)", appname);
    set_icon(NULL, morestuff);
    set_title(NULL, morestuff);

    if (ldisc) {
	ldisc_free(ldisc);
	ldisc = NULL;
    }
    if (back) {
	back->free(backhandle);
	backhandle = NULL;
	back = NULL;
        term_provide_resize_fn(term, NULL, NULL);
	update_specials_menu(NULL);
    }

    /*
     * Show the Restart Session menu item. Do a precautionary
     * delete first to ensure we never end up with more than one.
     */
    for (i = 0; i < lenof(popup_menus); i++) {
	DeleteMenu(popup_menus[i].menu, IDM_RESTART, MF_BYCOMMAND);
	InsertMenu(popup_menus[i].menu, IDM_DUPSESS, MF_BYCOMMAND | MF_ENABLED,
		   IDM_RESTART, "&Restart Session");
    }

    /*
     * Unset the 'must_close_session' flag, or else we'll come
     * straight back here the next time we go round the main message
     * loop - which, worse still, will be immediately (without
     * blocking) because we've just triggered a WM_SETTEXT by the
     * window title change above.
     */
    must_close_session = FALSE;
}

#ifdef CYGTERMPORT
/* Copy at most n characters from src to dst or until copying a '\0'
 * character.  A pointer to the terminal '\0' in dst is returned, or if no
 * '\0' was written, dst+n is returned.  */
static char *
stpcpy_max(char *dst, const char *src, size_t n)
{
    while (n-- && (*dst = *src++))
	dst++;
    return dst;
}

#endif

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    WNDCLASS wndclass;
    MSG msg;
    HRESULT hr;
    int guess_width, guess_height;
#ifdef ZMODEMPORT
	struct netscheduler_tag *netsc;
#endif

    hinst = inst;
    hwnd = NULL;
    flags = FLAG_VERBOSE | FLAG_INTERACTIVE;

#ifdef PERSOPORT

// Initialisation specifique � KiTTY
hInstIcons = hinst ; 
InitWinMain();

#endif

    sk_init();

    InitCommonControls();

    /* Ensure a Maximize setting in Explorer doesn't maximise the
     * config box. */
    defuse_showwindow();

    if (!init_winver())
    {
	char *str = dupprintf("%s Fatal Error", appname);
	MessageBox(NULL, "Windows refuses to report a version",
		   str, MB_OK | MB_ICONEXCLAMATION);
	sfree(str);
	return 1;
    }

    /*
     * If we're running a version of Windows that doesn't support
     * WM_MOUSEWHEEL, find out what message number we should be
     * using instead.
     */
    if (osVersion.dwMajorVersion < 4 ||
	(osVersion.dwMajorVersion == 4 && 
	 osVersion.dwPlatformId != VER_PLATFORM_WIN32_NT))
	wm_mousewheel = RegisterWindowMessage("MSWHEEL_ROLLMSG");

    init_help();

    init_flashwindow();

    /*
     * Initialize COM.
     */
    hr = CoInitialize(NULL);
    if (hr != S_OK && hr != S_FALSE) {
        char *str = dupprintf("%s Fatal Error", appname);
	MessageBox(NULL, "Failed to initialize COM subsystem",
		   str, MB_OK | MB_ICONEXCLAMATION);
	sfree(str);
	return 1;
    }

    /*
     * Process the command line.
     */
    {
	char *p;
	int got_host = 0;
	/* By default, we bring up the config dialog, rather than launching
	 * a session. This gets set to TRUE if something happens to change
	 * that (e.g., a hostname is specified on the command-line). */
	int allow_launch = FALSE;

	default_protocol = be_default_protocol;
	/* Find the appropriate default port. */
	{
	    Backend *b = backend_from_proto(default_protocol);
	    default_port = 0; /* illegal */
	    if (b)
		default_port = b->default_port;
	}
	cfg.logtype = LGTYP_NONE;

	do_defaults(NULL, &cfg);

	p = cmdline;

	/*
	 * Process a couple of command-line options which are more
	 * easily dealt with before the line is broken up into words.
	 * These are the old-fashioned but convenient @sessionname and
	 * the internal-use-only &sharedmemoryhandle, neither of which
	 * are combined with anything else.
	 */
	while (*p && isspace(*p))
	    p++;
	if (*p == '@') {
            /*
             * An initial @ means that the whole of the rest of the
             * command line should be treated as the name of a saved
             * session, with _no quoting or escaping_. This makes it a
             * very convenient means of automated saved-session
             * launching, via IDM_SAVEDSESS or Windows 7 jump lists.
             */
	    int i = strlen(p);
	    while (i > 1 && isspace(p[i - 1]))
		i--;
	    p[i] = '\0';
#ifdef PERSOPORT
	if( DirectoryBrowseFlag ) {
		char * pfolder ;
		if( (pfolder=strstr(p+1," -folder \"")) ) {
			pfolder[0]='\0';
			pfolder += 10 ;
			pfolder[strlen(pfolder)-1]='\0';
			SetSessPath( pfolder ) ;
			SetInitCurrentFolder( pfolder ) ;
			}
		}
#endif
	    do_defaults(p + 1, &cfg);
	    if (!cfg_launchable(&cfg) && !do_config()) {
		cleanup_exit(0);
	    }
	    allow_launch = TRUE;    /* allow it to be launched directly */
	} else if (*p == '&') {
	    /*
	     * An initial & means we've been given a command line
	     * containing the hex value of a HANDLE for a file
	     * mapping object, which we must then extract as a
	     * config.
	     */
	    HANDLE filemap;
	    Config *cp;
	    if (sscanf(p + 1, "%p", &filemap) == 1 &&
		(cp = MapViewOfFile(filemap, FILE_MAP_READ,
				    0, 0, sizeof(Config))) != NULL) {
		cfg = *cp;
		UnmapViewOfFile(cp);
		CloseHandle(filemap);
	    } else if (!do_config()) {
		cleanup_exit(0);
	    }
	    allow_launch = TRUE;
	} else {
	    /*
	     * Otherwise, break up the command line and deal with
	     * it sensibly.
	     */
	    int argc, i;
	    char **argv;
	    
	    split_into_argv(cmdline, &argc, &argv, NULL);

	    for (i = 0; i < argc; i++) {
		char *p = argv[i];
		int ret;

		ret = cmdline_process_param(p, i+1<argc?argv[i+1]:NULL,
					    1, &cfg);
		if (ret == -2) {
		    cmdline_error("option \"%s\" requires an argument", p);
		} else if (ret == 2) {
		    i++;	       /* skip next argument */
		} else if (ret == 1) {
		    continue;	       /* nothing further needs doing */
#ifdef PERSOPORT
		} else if( !strcmp(p, "-pass") ) {
			i++ ;
			strcpy( cfg.password, argv[i] ) ;
			memset( argv[i], 0, strlen(argv[i]) ) ;
		} else if( !strcmp(p, "-cc") ) {
			strcpy( cfg.host, "cmd.exe /k" ) ;
			cfg.port = 0 ;
			cfg.protocol = PROT_CYGTERM ;
			got_host = 1 ;
		} else if( !strcmp(p, "-cmd") ) {
			i++ ;
			strcpy( cfg.autocommand, argv[i] ) ;
			//cfg.remote_cmd_ptr=argv[i] ;
		} else if( !strcmp(p, "-initdelay") ) {
			i++ ;
			init_delay = atof( argv[i] ) ;
		} else if( !strcmp(p, "-log") ) {
			i++ ;
			cfg.logfilename = filename_from_str( argv[i] ) ;
			cfg.logtype = 1 ;
			cfg.logxfovr = 1 ;
			cfg.logflush = 1 ;
		} else if( !strcmp(p, "-nofiles") ) {
			NoKittyFileFlag = 1 ;
		} else if( !strcmp(p, "-edit") ) {
			
		} else if( !strcmp(p, "-folder") ) {
			char *pfolder ;
			pfolder=p+7;
			pfolder[0]='\0';
			pfolder += 1 ;
			if( pfolder[0]=='\"' ) pfolder++;
			if( pfolder[strlen(pfolder)-1]=='\"' ) pfolder[strlen(pfolder)-1]='\0';
			if( DirectoryBrowseFlag ) SetSessPath( pfolder ) ;
			SetInitCurrentFolder( pfolder ) ;
			i++ ;
		} else if( !strcmp(p, "-version") ) {
			showabout(hwnd) ; exit( 0 ) ;
		} else if( !strcmp(p, "-noicon") ) {
			IconeFlag = -1 ;
		} else if( !strcmp(p, "-notrans") ) {
			TransparencyNumber = -1 ;
		} else if( !strcmp(p, "-nozmodem") ) {
			ZModemFlag = 0 ;
		} else if( !strcmp(p, "-xpos") ) {
			i++ ;
			if( atoi(argv[i])>=0 ) 
				{ cfg.xpos=atoi(argv[i]) ; if(cfg.ypos<0) cfg.ypos=0 ; cfg.save_windowpos=1 ; }
		} else if( !strcmp(p, "-ypos") ) {
			i++ ;
			if( atoi(argv[i])>=0 ) 
				{ cfg.ypos=atoi(argv[i]) ; if(cfg.xpos<0) cfg.xpos=0 ; cfg.save_windowpos=1 ; }
#if (defined IMAGEPORT) && (!defined FDJ)
		} else if( !strcmp(p, "-nobgimage") ) {
			BackgroundImageFlag = 0 ;
#endif
		} else if( !strcmp(p, "-putty") ) {
			NoKittyFileFlag = 1 ;
			IconeFlag = -1 ;
			hInstIcons = hinst ; 
			TransparencyNumber = -1 ;
			ShortcutsFlag = 0 ;
			SizeFlag = 0 ;
			CapsLockFlag = 0 ;
			WinHeight = -1 ;
			ConfigBoxHeight = 7 ;
#ifdef CYGTERMPORT
			cygterm_set_flag( 0 ) ;
#endif
#if (defined IMAGEPORT) && (!defined FDJ)
			BackgroundImageFlag = 0 ;
#endif
			SessionFilterFlag = 0 ;
			PuttyFlag = 1 ;
			//SetWindowPos(GetForegroundWindow(), HWND_TOP, 0, 0, 0, 252, SWP_SHOWWINDOW|SWP_NOMOVE);
		} else if( !strcmp(p, "-send-to-tray") ) {
			AutoSendToTray = 1 ;
		} else if( !strcmp(p, "-sshhandler") ) {
			CreateSSHHandler() ; return 0 ;
#ifndef FDJ
		} else if( !strcmp(p, "-key") ) {
			GenerePrivateKey( "private.key.ppk" ) ; return  0 ;
#endif
		} else if( !strcmp(p, "-ed") ) {
			return Notepad_WinMain(inst, prev, cmdline, show) ;
		} else if( !strcmp(p, "-launcher") ) {
			return Launcher_WinMain(inst, prev, cmdline, show) ;
		} else if( !strcmp(p, "-convert-dir") ) {
			return Convert2Dir( ConfigDirectory ) ;
		} else if( !strcmp(p, "-convert-reg") ) {
			return Convert2Reg( ConfigDirectory ) ;
		} else if( !strcmp(p, "-title") ) {
			i++ ;
			strcpy( cfg.wintitle, argv[i] ) ;
#ifndef FDJ
		} else if( !strcmp(p, "-classname") ) {
			i++ ;
			if( strlen( argv[i] ) > 0 ) {
				strcpy( KiTTYClassName, argv[i] ) ;
				appname = KiTTYClassName ;
				}
#endif
		} else if( !strcmp(p, "-sendcmd") ) {
			i++ ;
			if( strlen(argv[i])>0 )
				SendCommandAllWindows( hwnd, argv[i] ) ;
			exit(0);
#endif
		} else if (!strcmp(p, "-cleanup") ||
			   !strcmp(p, "-cleanup-during-uninstall")) {
		    /*
		     * `putty -cleanup'. Remove all registry
		     * entries associated with PuTTY, and also find
		     * and delete the random seed file.
		     */
		    char *s1, *s2;
		    /* Are we being invoked from an uninstaller? */
		    if (!strcmp(p, "-cleanup-during-uninstall")) {
			s1 = dupprintf("Remove saved sessions and random seed file?\n"
				       "\n"
				       "If you hit Yes, ALL Registry entries associated\n"
				       "with %s will be removed, as well as the\n"
				       "random seed file. THIS PROCESS WILL\n"
				       "DESTROY YOUR SAVED SESSIONS.\n"
				       "(This only affects the currently logged-in user.)\n"
				       "\n"
				       "If you hit No, uninstallation will proceed, but\n"
				       "saved sessions etc will be left on the machine.",
				       appname);
			s2 = dupprintf("%s Uninstallation", appname);
		    } else {
			s1 = dupprintf("This procedure will remove ALL Registry entries\n"
				       "associated with %s, and will also remove\n"
				       "the random seed file. (This only affects the\n"
				       "currently logged-in user.)\n"
				       "\n"
				       "THIS PROCESS WILL DESTROY YOUR SAVED SESSIONS.\n"
				       "Are you really sure you want to continue?",
				       appname);
			s2 = dupprintf("%s Warning", appname);
		    }
		    if (message_box(s1, s2,
				    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2,
				    HELPCTXID(option_cleanup)) == IDYES) {
			cleanup_all();
		    }
		    sfree(s1);
		    sfree(s2);
		    exit(0);
		} else if (!strcmp(p, "-pgpfp")) {
		    pgp_fingerprints();
		    exit(1);
#ifdef CYGTERMPORT
		/* A single "-" argument is interpreted as a "host name" */
		} else if (p[0] != '-' || p[1] == '\0') {
#else
		} else if (*p != '-') {
#endif
		    char *q = p;
		    if (got_host) {
			/*
			 * If we already have a host name, treat
			 * this argument as a port number. NB we
			 * have to treat this as a saved -P
			 * argument, so that it will be deferred
			 * until it's a good moment to run it.
			 */
			int ret = cmdline_process_param("-P", p, 1, &cfg);
			assert(ret == 2);
		    } else if (!strncmp(q, "telnet:", 7)) {
			/*
			 * If the hostname starts with "telnet:",
			 * set the protocol to Telnet and process
			 * the string as a Telnet URL.
			 */
			char c;

			q += 7;
			if (q[0] == '/' && q[1] == '/')
			    q += 2;
			cfg.protocol = PROT_TELNET;
			p = q;
			while (*p && *p != ':' && *p != '/')
			    p++;
			c = *p;
			if (*p)
			    *p++ = '\0';
			if (c == ':')
			    cfg.port = atoi(p);
			else
			    cfg.port = -1;
			strncpy(cfg.host, q, sizeof(cfg.host) - 1);
			cfg.host[sizeof(cfg.host) - 1] = '\0';
			got_host = 1;
#ifdef PERSOPORT
			} else if (!strncmp(q, "ssh:", 4)) {
				/*
				* If the hostname starts with "ssh:",
				* set the protocol to SSH and process
				* the string as a SSH URL
				*/
				char c;
				q += 4;
				if (q[0] == '/' && q[1] == '/')
				q += 2;
				cfg.protocol = PROT_SSH;
				p = q;
				while (*p && *p != ':' && *p != '/')
					p++;
				c = *p;
				if (*p)
					*p++ = '\0';
				if (c == ':')
					cfg.port = atoi(p);
				else if( (c == '/')&&(strlen(p)>0) ) {
					cfg.port = -1;
					if( p[0]=='#' ) {
						strcpy( cfg.autocommand, p+1 ) ;
						decryptstring( cfg.autocommand, MASTER_PASSWORD ) ;
						}
					else
						{ 
						strcpy( cfg.autocommand, p ) ;
						int i = decode64(cfg.autocommand) ; cfg.autocommand[i]='\0'; 
						}
					}
				else
					cfg.port = -1;
				strncpy(cfg.host, q, sizeof(cfg.host) - 1);
				cfg.host[sizeof(cfg.host) - 1] = '\0';
				got_host = 1;
			} else if (!strncmp(q, "putty:", 4)) {
				int ret = 0;
				q += 6;
				if (q[0] == '/' && q[1] == '/') q += 2;
				if (q[strlen(q) - 1] == '/') q[strlen(q) - 1] = '\0';
				p = q;
				ret = cmdline_process_param("-load", p, 1, &cfg);
				assert(ret == 2);
#endif
#ifdef CYGTERMPORT
                    } else if (cfg.protocol == PROT_CYGTERM) {
                        /* Concatenate all the remaining arguments separating
                         * them with spaces to get the command line to execute.
                         */
                        char *p = cfg.cygcmd;
                        char *const end = cfg.cygcmd + sizeof cfg.cygcmd;
                        for (; i < argc && p < end; i++) {
                            p = stpcpy_max(p, argv[i], end - p - 1);
                            *p++ = ' ';
                        }
                        assert(p > cfg.cygcmd && p <= end);
                        *--p = '\0';
			got_host = 1;
#endif
		    } else {
			/*
			 * Otherwise, treat this argument as a host
			 * name.
			 */
			while (*p && !isspace(*p))
			    p++;
			if (*p)
			    *p++ = '\0';
			strncpy(cfg.host, q, sizeof(cfg.host) - 1);
			cfg.host[sizeof(cfg.host) - 1] = '\0';
			got_host = 1;
		    }
		} else {
		    cmdline_error("unknown option \"%s\"", p);
		}
	    }
	}

#ifdef PERSOPORT
	// Creation du fichier kitty.ini par defaut si besoin
	CreateDefaultIniFile() ;
#endif
	
	cmdline_run_saved(&cfg);

	if (loaded_session || got_host)
	    allow_launch = TRUE;
	if ((!allow_launch || !cfg_launchable(&cfg)) && !do_config()) {
	    cleanup_exit(0);
	}

	/*
	 * Trim leading whitespace off the hostname if it's there.
	 */
	{
	    int space = strspn(cfg.host, " \t");
	    memmove(cfg.host, cfg.host+space, 1+strlen(cfg.host)-space);
	}

	/* See if host is of the form user@host */
	if (cfg.host[0] != '\0') {
	    char *atsign = strrchr(cfg.host, '@');
	    /* Make sure we're not overflowing the user field */
	    if (atsign) {
		if (atsign - cfg.host < sizeof cfg.username) {
		    strncpy(cfg.username, cfg.host, atsign - cfg.host);
		    cfg.username[atsign - cfg.host] = '\0';
		}
		memmove(cfg.host, atsign + 1, 1 + strlen(atsign + 1));
	    }
	}

	/*
	 * Trim a colon suffix off the hostname if it's there. In
	 * order to protect IPv6 address literals against this
	 * treatment, we do not do this if there's _more_ than one
	 * colon.
	 */
	{
	    char *c = strchr(cfg.host, ':');

	    if (c) {
		char *d = strchr(c+1, ':');
		if (!d)
		    *c = '\0';
	    }
	}

	/*
	 * Remove any remaining whitespace from the hostname.
	 */
	{
	    int p1 = 0, p2 = 0;
	    while (cfg.host[p2] != '\0') {
		if (cfg.host[p2] != ' ' && cfg.host[p2] != '\t') {
		    cfg.host[p1] = cfg.host[p2];
		    p1++;
		}
		p2++;
	    }
	    cfg.host[p1] = '\0';
	}
    }
#ifdef PERSOPORT
int xpos_init=0, ypos_init=0 ;
	if( (cfg.saveonexit||cfg.save_windowpos) && (cfg.xpos>=0) && (cfg.ypos>=0) ) {
		xpos_init=cfg.xpos ; if( xpos_init>(GetSystemMetrics(SM_CXSCREEN)-10) ) xpos_init = 10 ;
		ypos_init=cfg.ypos ; if( ypos_init>(GetSystemMetrics(SM_CYSCREEN)-10) ) ypos_init = 10 ;
		}

while( cfg.icone > NumberOfIcons ) cfg.icone = cfg.icone - NumberOfIcons ;
if( cfg.icone == 0 ) {
	if( IconeFlag > 0 ) IconeNum = time( NULL ) % NumberOfIcons ; else IconeNum = 0 ; 
	}
else	{
	if( IconeFlag > 0 ) IconeNum = cfg.icone - 1 ; else IconeNum = 0 ; 
	}

	MASKPASS(cfg.password) ;
#endif

   if (!prev) {
	wndclass.style = 0;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = inst;
#ifdef PERSOPORT
	wndclass.hIcon = LoadIcon( hInstIcons, MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum ) );
#else
	wndclass.hIcon = LoadIcon(inst, MAKEINTRESOURCE(IDI_MAINICON));
#endif
	wndclass.hCursor = LoadCursor(NULL, IDC_IBEAM);
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName = NULL;
#if (defined PERSOPORT) && (!defined FDJ)
	wndclass.lpszClassName = KiTTYClassName ;
#else
	wndclass.lpszClassName = appname;
#endif

	RegisterClass(&wndclass);
    }

#ifdef PERSOPORT
// Initialisation de la structure NOTIFYICONDATA
TrayIcone.cbSize = sizeof(TrayIcone);						// On alloue la taille n�cessaire pour la structure
TrayIcone.uID = IDI_MAINICON_0 ;							// On lui donne un ID
TrayIcone.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;	// On lui indique les champs valables
	// On lui dit qu'il devra "�couter" son environement (clique de souris, etc)
TrayIcone.uCallbackMessage = MYWM_NOTIFYICON;
TrayIcone.hIcon = LoadIcon(NULL, NULL);					// On ne load aucune icone pour le moment
TrayIcone.szTip[1024] = "PuTTY That\'s all folks!\0" ;			// Le tooltip par d�faut, soit rien
TrayIcone.hWnd = hwnd ;
#endif

    memset(&ucsdata, 0, sizeof(ucsdata));

    cfgtopalette();

    /*
     * Guess some defaults for the window size. This all gets
     * updated later, so we don't really care too much. However, we
     * do want the font width/height guesses to correspond to a
     * large font rather than a small one...
     */

    font_width = 10;
    font_height = 20;
    extra_width = 25;
    extra_height = 28;
    guess_width = extra_width + font_width * cfg.width;
    guess_height = extra_height + font_height * cfg.height;
    {
	RECT r;
	get_fullscreen_rect(&r);
	if (guess_width > r.right - r.left)
	    guess_width = r.right - r.left;
	if (guess_height > r.bottom - r.top)
	    guess_height = r.bottom - r.top;
    }
#if (defined IMAGEPORT) && (!defined FDJ)
    	const char* winname = appname;
#endif

    {
	int winmode = WS_OVERLAPPEDWINDOW | WS_VSCROLL;
	int exwinmode = 0;
	if (!cfg.scrollbar)
	    winmode &= ~(WS_VSCROLL);
	if (cfg.resize_action == RESIZE_DISABLED)
	    winmode &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	if (cfg.alwaysontop)
	    exwinmode |= WS_EX_TOPMOST;
	if (cfg.sunken_edge)
	    exwinmode |= WS_EX_CLIENTEDGE;
#ifdef PERSOPORT
#if (defined IMAGEPORT) && (!defined FDJ)
	// TODO: This is the beginning of some work to have windows with fancy
	// no-client-edge borders.  It's not ready yet.
	if( BackgroundImageFlag && (!PuttyFlag) )
	if(0)
	{
		winmode = WS_POPUP;
		exwinmode = 0;
		winname = NULL;

		// TODO: This is proof-of-concept.  For this to really work, we'll
		// have to do some additional mods, like creating our own title/move
		// window to glue to the top, and some kind of drag-resizing window
		// to glue to the bottom-right.  Otherwise there'll be no way to move
		// or resize the window, which will get old extremely quickly.  Finally,
		// this won't work as written anyway, b/c when you call SetWindowText
		// anywhere Win32 forces a window border to appear anyway.  So, we'll
		// want to create a new function, set_window_text, that checks whether
		// to really call SetWindowText or to set the window text in whatever
		// other location is necessary for our custom window text display for
		// when we're handling our own border here.
	}

	hwnd = CreateWindowEx(exwinmode|WS_EX_ACCEPTFILES, appname, winname,
			      winmode, CW_USEDEFAULT, CW_USEDEFAULT,
			      guess_width, guess_height,
			      NULL, NULL, inst, NULL);

	if( BackgroundImageFlag ) init_dc_blend();
#else
	hwnd = CreateWindowEx(exwinmode|WS_EX_ACCEPTFILES, appname, appname,
			      winmode, CW_USEDEFAULT, CW_USEDEFAULT,
			      guess_width, guess_height,
			      NULL, NULL, inst, NULL);
#endif
#else
	hwnd = CreateWindowEx(exwinmode, appname, appname,
			      winmode, CW_USEDEFAULT, CW_USEDEFAULT,
			      guess_width, guess_height,
			      NULL, NULL, inst, NULL);
#endif

    }

    /*
     * Initialise the terminal. (We have to do this _after_
     * creating the window, since the terminal is the first thing
     * which will call schedule_timer(), which will in turn call
     * timer_change_notify() which will expect hwnd to exist.)
     */
    term = term_init(&cfg, &ucsdata, NULL);
    logctx = log_init(NULL, &cfg);
    term_provide_logctx(term, logctx);
    term_size(term, cfg.height, cfg.width, cfg.savelines);

    /*
     * Initialise the fonts, simultaneously correcting the guesses
     * for font_{width,height}.
     */
    init_fonts(0,0);

    /*
     * Correct the guesses for extra_{width,height}.
     */
    {
	RECT cr, wr;
	GetWindowRect(hwnd, &wr);
	GetClientRect(hwnd, &cr);
	offset_width = offset_height = cfg.window_border;
	extra_width = wr.right - wr.left - cr.right + cr.left + offset_width*2;
	extra_height = wr.bottom - wr.top - cr.bottom + cr.top +offset_height*2;
    }

    /*
     * Resize the window, now we know what size we _really_ want it
     * to be.
     */
    guess_width = extra_width + font_width * term->cols;
    guess_height = extra_height + font_height * term->rows;
    SetWindowPos(hwnd, NULL, 0, 0, guess_width, guess_height,
		 SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
#ifdef PERSOPORT
    if( !PuttyFlag )
    if( (cfg.saveonexit||cfg.save_windowpos) && (xpos_init>=0) && (ypos_init>=0) ) {
	MoveWindow(hwnd, xpos_init, ypos_init, guess_width, guess_height, TRUE );
	}
#endif

    /*
     * Set up a caret bitmap, with no content.
     */
    {
	char *bits;
	int size = (font_width + 15) / 16 * 2 * font_height;
	bits = snewn(size, char);
	memset(bits, 0, size);
	caretbm = CreateBitmap(font_width, font_height, 1, 1, bits);
	sfree(bits);
    }
    CreateCaret(hwnd, caretbm, font_width, font_height);

    /*
     * Initialise the scroll bar.
     */
    {
	SCROLLINFO si;

	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
	si.nMin = 0;
	si.nMax = term->rows - 1;
	si.nPage = term->rows;
	si.nPos = 0;
	SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
    }

    /*
     * Prepare the mouse handler.
     */
    lastact = MA_NOTHING;
    lastbtn = MBT_NOTHING;
    dbltime = GetDoubleClickTime();

    /*
     * Set up the session-control options on the system menu.
     */
    {
	HMENU m;
	int j;
	char *str;

	popup_menus[SYSMENU].menu = GetSystemMenu(hwnd, FALSE);
	popup_menus[CTXMENU].menu = CreatePopupMenu();
	AppendMenu(popup_menus[CTXMENU].menu, MF_ENABLED, IDM_PASTE, "&Paste");

	savedsess_menu = CreateMenu();
	get_sesslist(&sesslist, TRUE);
	update_savedsess_menu();

	for (j = 0; j < lenof(popup_menus); j++) {
	    m = popup_menus[j].menu;

	    AppendMenu(m, MF_SEPARATOR, 0, 0);
#ifdef PERSOPORT
		if( !PuttyFlag ) InitSpecialMenu( m, cfg ) ;
#endif
	    AppendMenu(m, MF_ENABLED, IDM_SHOWLOG, "&Event Log");
	    AppendMenu(m, MF_SEPARATOR, 0, 0);
	    AppendMenu(m, MF_ENABLED, IDM_NEWSESS, "Ne&w Session...");
	    AppendMenu(m, MF_ENABLED, IDM_DUPSESS, "&Duplicate Session");
	    AppendMenu(m, MF_POPUP | MF_ENABLED, (UINT) savedsess_menu,
		       "Sa&ved Sessions");
	    AppendMenu(m, MF_ENABLED, IDM_RECONF, "Chan&ge Settings...");
	    AppendMenu(m, MF_SEPARATOR, 0, 0);
	    AppendMenu(m, MF_ENABLED, IDM_COPYALL, "C&opy All to Clipboard");
	    AppendMenu(m, MF_ENABLED, IDM_CLRSB, "C&lear Scrollback");
	    AppendMenu(m, MF_ENABLED, IDM_RESET, "Rese&t Terminal");
	    AppendMenu(m, MF_SEPARATOR, 0, 0);
	    AppendMenu(m, (cfg.resize_action == RESIZE_DISABLED) ?
		       MF_GRAYED : MF_ENABLED, IDM_FULLSCREEN, "&Full Screen");
#ifdef PERSOPORT
	if( !PuttyFlag ) {
		AppendMenu(m, MF_ENABLED, IDM_PRINT, "Print clip&board");
		AppendMenu(m, MF_ENABLED, IDM_TOTRAY, "Send to tra&y");
		if( cfg.alwaysontop )
			AppendMenu(m, MF_ENABLED|MF_CHECKED, IDM_VISIBLE, "Always visi&ble");
		else
			AppendMenu(m, MF_ENABLED, IDM_VISIBLE, "Always visi&ble");
		AppendMenu(m, MF_ENABLED, IDM_PROTECT, "Prote&ct");
		AppendMenu(m, MF_ENABLED, IDM_WINROL, "Roll-up");
		AppendMenu(m, MF_ENABLED, IDM_SHOWPORTFWD, "Port forwarding");
		AppendMenu(m, MF_ENABLED, IDM_SCRIPTFILE, "Send script file" ) ;
		AppendMenu(m, MF_ENABLED, IDM_PSCP, "Send with pscp");
		if( WinSCPPath!=NULL ) AppendMenu(m, MF_ENABLED, IDM_WINSCP, "Start WinSCP");
		else AppendMenu(m, MF_DISABLED|MF_GRAYED, IDM_WINSCP, "Start WinSCP");
		}
#endif
#ifdef ZMODEMPORT
	if( (!PuttyFlag) && ZModemFlag ) {
	    AppendMenu(m, MF_SEPARATOR, 0, 0);
	    AppendMenu(m, term->xyz_transfering?MF_GRAYED:MF_ENABLED, IDM_XYZSTART, "&Zmodem Receive");
	    AppendMenu(m, term->xyz_transfering?MF_GRAYED:MF_ENABLED, IDM_XYZUPLOAD, "Zmodem &Upload");
	    AppendMenu(m, !term->xyz_transfering?MF_GRAYED:MF_ENABLED, IDM_XYZABORT, "Zmodem &Abort");
		}
#endif
	    AppendMenu(m, MF_SEPARATOR, 0, 0);
	    if (has_help())
		AppendMenu(m, MF_ENABLED, IDM_HELP, "&Help");
	    str = dupprintf("&About %s", appname);
	    AppendMenu(m, MF_ENABLED, IDM_ABOUT, str);
	    sfree(str);
	}
    }

#ifdef PERSOPORT
	if( !PuttyFlag ) {
		// Lancement automatique dans le Tray
		if( cfg.sendtotray ) AutoSendToTray = 1 ;
			
		// Charge le fichier de script d'initialisation si il existe
		ScriptFileContent = NULL ;
		ReadInitScript( NULL ) ;
		
		// Lancement du serveur de chat
		static char reg_buffer[256];
		if( ReadParameter( INIT_SECTION, "chat", reg_buffer ) ) 
			{
			int chat_flag = atoi( reg_buffer ) ;
			if ( chat_flag > 0 ) {
				if( chat_flag != 1 ) PORT = chat_flag ; _beginthread( routine_server, 0, NULL ) ;
				//if( chat_flag == 1 ) chat_flag = 1987 ;	server_run_routine( chat_flag, 2 ) ;
				}
			}

		// Param�trage sp�cifique � la session
		if( GetSessionField( cfg.sessionname, cfg.folder, "InitDelay", reg_buffer ) ) {
			if( init_delay != atof( reg_buffer ) ) 
				{ cfg.initdelay = atof( reg_buffer ) ; init_delay = cfg.initdelay ; }
			}
		if( GetSessionField( cfg.sessionname, cfg.folder, "BCDelay", reg_buffer ) ) {
			if( between_char_delay != atof( reg_buffer ) )
				{ cfg.bcdelay = atof( reg_buffer ) ; between_char_delay = cfg.bcdelay ; }
			}

#ifndef NO_TRANSPARENCY
		if( TransparencyNumber != -1 )
		if( cfg.transparencynumber != TransparencyNumber ) { 
			TransparencyNumber = 255 - cfg.transparencynumber ;
			SetTransparency( hwnd, TransparencyNumber ) ;
			}
#endif
		// Lancement du timer auto-command pour les connexions non SSH
		if(cfg.protocol != PROT_SSH) backend_connected = 1 ;
		SetTimer(hwnd, TIMER_INIT, (int)(init_delay*1000), NULL) ;

		if( IniFileFlag == SAVEMODE_REG ) {
			sprintf( reg_buffer, "%s@%s:%d (prot=%d) name=%s", cfg.username, cfg.host, cfg.port, cfg.protocol, cfg.sessionname ) ;
			cryptstring( reg_buffer, MASTER_PASSWORD ) ;
			WriteParameter( INIT_SECTION, "KiLastSe", reg_buffer ) ;
			}
		
		// Lancement des timer (changement image de fond, rafraichissement)
#if (defined IMAGEPORT) && (!defined FDJ)
		if( (!BackgroundImageFlag) || PuttyFlag ) cfg.bg_type = 0 ;
		if( cfg.bg_type!=0 ) {
			if(cfg.bg_slideshow>0)
			SetTimer(hwnd, TIMER_SLIDEBG, (int)(cfg.bg_slideshow*1000), NULL) ;
			else 
			if(ImageSlideDelay>0)
			SetTimer(hwnd, TIMER_SLIDEBG, (int)(ImageSlideDelay*1000), NULL) ;
			}
		if( PuttyFlag ) {
			cfg.url_underline = URLHACK_UNDERLINE_NEVER ;
			}

		// Lancement du rafraichissement toutes les 10 secondes (pour l'image de fond, pour pallier bug d'affichage)
		if( ReadParameter( INIT_SECTION, "redraw", reg_buffer ) ) {
			if( stricmp( reg_buffer, "NO" ) ) SetTimer(hwnd, TIMER_REDRAW, (int)(10*1000), NULL) ;
			}
		else SetTimer(hwnd, TIMER_REDRAW, (int)(10*1000), NULL) ;
#endif
			
		} // fin de if( !PuttyFlag )
#endif

    start_backend();
#ifdef HYPERLINKPORT
	/*
	 * HACK: PuttyTray / Nutty
	 * Hyperlink stuff: Set the regular expression
	 */
	if (term->cfg.url_defregex == 0) {
		urlhack_set_regular_expression(term->cfg.url_regex);
		}
#endif

    /*
     * Set up the initial input locale.
     */
    set_input_locale(GetKeyboardLayout(0));

    /*
     * Finally show the window!
     */
    ShowWindow(hwnd, show);
    SetForegroundWindow(hwnd);

    /*
     * Set the palette up.
     */
    pal = NULL;
    logpal = NULL;
    init_palette();

    term_set_focus(term, GetForegroundWindow() == hwnd);
    UpdateWindow(hwnd);

#ifdef ZMODEMPORT
	netsc = netscheduler_new();
#endif

    while (1) {
	HANDLE *handles;
	int nhandles, n;

	handles = handle_get_events(&nhandles);

	n = MsgWaitForMultipleObjects(nhandles, handles, FALSE, INFINITE,
				      QS_ALLINPUT);

	if ((unsigned)(n - WAIT_OBJECT_0) < (unsigned)nhandles) {
	    handle_got_event(handles[n - WAIT_OBJECT_0]);
	    sfree(handles);
	    if (must_close_session)
#ifdef PERSOPORT
		{ close_session(); must_close_session = FALSE; }
#else
		close_session();
#endif
#ifdef ZMODEMPORT
	     if( ZModemFlag ) continue;
#endif

	} else
	    sfree(handles);

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
	    if (msg.message == WM_QUIT)
		goto finished;	       /* two-level break */

	    if (!(IsWindow(logbox) && IsDialogMessage(logbox, &msg)))
		DispatchMessage(&msg);
	    /* Send the paste buffer if there's anything to send */
	    term_paste(term);
#ifdef ZMODEMPORT
	    	    if (ZModemFlag && xyz_Process(back, backhandle, term))
		    continue;
#endif
	    /* If there's nothing new in the queue then we can do everything
	     * we've delayed, reading the socket, writing, and repainting
	     * the window.
	     */
	    if (must_close_session)
#ifdef PERSOPORT
		{ close_session(); must_close_session = FALSE; }
#else
		close_session();
#endif

	}

	/* The messages seem unreliable; especially if we're being tricky */
	term_set_focus(term, GetForegroundWindow() == hwnd);

	if (pending_netevent)
	    enact_pending_netevent();

	net_pending_errors();
    }

    finished:
#ifdef ZMODEMPORT
	netscheduler_free(netsc);
#endif
    cleanup_exit(msg.wParam);	       /* this doesn't return... */
    return msg.wParam;		       /* ... but optimiser doesn't know */
}

/*
 * Clean up and exit.
 */
void cleanup_exit(int code)
{
    /*
     * Clean up.
     */
#ifdef PERSOPORT
	chdir( InitialDirectory ) ;
	if( cfg.saveonexit ) { SaveWindowCoord( cfg ) ; }

	if( IniFileFlag == SAVEMODE_REG ) { // Mode de sauvegarde registry
		//SaveRegistryKey() ; 
#ifndef FDJ
		if( RegTestKey( HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY" ) )
			RepliqueToPuTTY( PUTTY_REG_POS ) ; // Sauvegarde la conf dans Putty (uniquement si elle existe)
#endif
		}
	else if( IniFileFlag == SAVEMODE_FILE ) { // Mode de sauvegarde fichier
		int nb ;
		nb = WindowsCount( MainHwnd ) ;
		if( nb == 1 ) { // c'est la derniere instance de kitty on vide la cl� de registre
			HWND hdlg = InfoBox( hinst, NULL ) ;
			InfoBoxSetText( hdlg, "Cleaning registry" ) ;
			RegDelTree( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ;
			if( RegTestKey( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS_SAVE) ) ) {
				InfoBoxSetText( hdlg, "Restoring backup registry" ) ;
				RegRenameTree( NULL, HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS_SAVE), TEXT(PUTTY_REG_POS) ) ;
				}
			else DelRegistryKey() ;
			InfoBoxClose( hdlg ) ;
			}
		}
#endif

    deinit_fonts();
    sfree(logpal);
    if (pal)
	DeleteObject(pal);
    sk_cleanup();

    if (cfg.protocol == PROT_SSH) {
	random_save_seed();
#ifdef MSCRYPTOAPI
	crypto_wrapup();
#endif
    }
    shutdown_help();

    /* Clean up COM. */
    CoUninitialize();

    exit(code);
}

/*
 * Set up, or shut down, an AsyncSelect. Called from winnet.c.
 */
char *do_select(SOCKET skt, int startup)
{
    int msg, events;
    if (startup) {
	msg = WM_NETEVENT;
	events = (FD_CONNECT | FD_READ | FD_WRITE |
		  FD_OOB | FD_CLOSE | FD_ACCEPT);
    } else {
	msg = events = 0;
    }
    if (!hwnd)
	return "do_select(): internal error (hwnd==NULL)";
    if (p_WSAAsyncSelect(skt, hwnd, msg, events) == SOCKET_ERROR) {
	switch (p_WSAGetLastError()) {
	  case WSAENETDOWN:
	    return "Network is down";
	  default:
	    return "WSAAsyncSelect(): unknown error";
	}
    }
    return NULL;
}

/*
 * Refresh the saved-session submenu from `sesslist'.
 */
static void update_savedsess_menu(void)
{
    int i;
    while (DeleteMenu(savedsess_menu, 0, MF_BYPOSITION)) ;
    /* skip sesslist.sessions[0] == Default Settings */
    for (i = 1;
	 i < ((sesslist.nsessions <= MENU_SAVED_MAX+1) ? sesslist.nsessions
						       : MENU_SAVED_MAX+1);
	 i++)
	AppendMenu(savedsess_menu, MF_ENABLED,
		   IDM_SAVED_MIN + (i-1)*MENU_SAVED_STEP,
		   sesslist.sessions[i]);
    if (sesslist.nsessions <= 1)
	AppendMenu(savedsess_menu, MF_GRAYED, IDM_SAVED_MIN, "(No sessions)");
}

/*
 * Update the Special Commands submenu.
 */
void update_specials_menu(void *frontend)
{
    HMENU new_menu;
    int i, j;

    if (back)
	specials = back->get_specials(backhandle);
    else
	specials = NULL;

    if (specials) {
	/* We can't use Windows to provide a stack for submenus, so
	 * here's a lame "stack" that will do for now. */
	HMENU saved_menu = NULL;
	int nesting = 1;
	new_menu = CreatePopupMenu();
	for (i = 0; nesting > 0; i++) {
	    assert(IDM_SPECIAL_MIN + 0x10 * i < IDM_SPECIAL_MAX);
	    switch (specials[i].code) {
	      case TS_SEP:
		AppendMenu(new_menu, MF_SEPARATOR, 0, 0);
		break;
	      case TS_SUBMENU:
		assert(nesting < 2);
		nesting++;
		saved_menu = new_menu; /* XXX lame stacking */
		new_menu = CreatePopupMenu();
		AppendMenu(saved_menu, MF_POPUP | MF_ENABLED,
			   (UINT) new_menu, specials[i].name);
		break;
	      case TS_EXITMENU:
		nesting--;
		if (nesting) {
		    new_menu = saved_menu; /* XXX lame stacking */
		    saved_menu = NULL;
		}
		break;
	      default:
		AppendMenu(new_menu, MF_ENABLED, IDM_SPECIAL_MIN + 0x10 * i,
			   specials[i].name);
		break;
	    }
	}
	/* Squirrel the highest special. */
	n_specials = i - 1;
    } else {
	new_menu = NULL;
	n_specials = 0;
    }

    for (j = 0; j < lenof(popup_menus); j++) {
	if (specials_menu) {
	    /* XXX does this free up all submenus? */
	    DeleteMenu(popup_menus[j].menu, (UINT)specials_menu, MF_BYCOMMAND);
	    DeleteMenu(popup_menus[j].menu, IDM_SPECIALSEP, MF_BYCOMMAND);
	}
	if (new_menu) {
	    InsertMenu(popup_menus[j].menu, IDM_SHOWLOG,
		       MF_BYCOMMAND | MF_POPUP | MF_ENABLED,
		       (UINT) new_menu, "S&pecial Command");
	    InsertMenu(popup_menus[j].menu, IDM_SHOWLOG,
		       MF_BYCOMMAND | MF_SEPARATOR, IDM_SPECIALSEP, 0);
	}
    }
    specials_menu = new_menu;
}

static void update_mouse_pointer(void)
{
    LPTSTR curstype;
    int force_visible = FALSE;
    static int forced_visible = FALSE;
    switch (busy_status) {
      case BUSY_NOT:
	if (send_raw_mouse)
	    curstype = IDC_ARROW;
	else
	    curstype = IDC_IBEAM;
	break;
      case BUSY_WAITING:
	curstype = IDC_APPSTARTING; /* this may be an abuse */
	force_visible = TRUE;
	break;
      case BUSY_CPU:
	curstype = IDC_WAIT;
	force_visible = TRUE;
	break;
      default:
	assert(0);
    }
    {
	HCURSOR cursor = LoadCursor(NULL, curstype);
	SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR)cursor);
	SetCursor(cursor); /* force redraw of cursor at current posn */
    }
    if (force_visible != forced_visible) {
	/* We want some cursor shapes to be visible always.
	 * Along with show_mouseptr(), this manages the ShowCursor()
	 * counter such that if we switch back to a non-force_visible
	 * cursor, the previous visibility state is restored. */
	ShowCursor(force_visible);
	forced_visible = force_visible;
    }
}

void set_busy_status(void *frontend, int status)
{
    busy_status = status;
    update_mouse_pointer();
}

/*
 * set or clear the "raw mouse message" mode
 */
void set_raw_mouse_mode(void *frontend, int activate)
{
    activate = activate && !cfg.no_mouse_rep;
    send_raw_mouse = activate;
    update_mouse_pointer();
}

/*
 * Print a message box and close the connection.
 */
void connection_fatal(void *frontend, char *fmt, ...)
{
    va_list ap;
    char *stuff, morestuff[100];
#ifdef RECONNECTPORT
	if (cfg.failure_reconnect) {
 		time_t tnow = time(NULL);
 		close_session();
 
 		if(last_reconnect && (tnow - last_reconnect) < 5) {
 			Sleep(5000);
 		}
 
 		last_reconnect = tnow;
 		logevent(NULL, "Lost connection, reconnecting...");
 		term_pwron(term, FALSE);
		backend_connected = 0 ;
 		start_backend();
		SetTimer(hwnd, TIMER_INIT, (int)(init_delay*1000), NULL) ;
 	} else {
 		va_start(ap, fmt);
		stuff = dupvprintf(fmt, ap);
 		va_end(ap);
 		sprintf(morestuff, "%.70s Fatal Error", appname);
 		MessageBox(hwnd, stuff, morestuff, MB_ICONERROR | MB_OK);
 		sfree(stuff);
 
 		if (cfg.close_on_exit == FORCE_ON)
 		PostQuitMessage(1);
 		else {
 		must_close_session = TRUE;
 		}
 	}
#else
    va_start(ap, fmt);
    stuff = dupvprintf(fmt, ap);
    va_end(ap);
    sprintf(morestuff, "%.70s Fatal Error", appname);
    MessageBox(hwnd, stuff, morestuff, MB_ICONERROR | MB_OK);
    sfree(stuff);

    if (cfg.close_on_exit == FORCE_ON)
	PostQuitMessage(1);
    else {
	must_close_session = TRUE;
    }
#endif
}

/*
 * Report an error at the command-line parsing stage.
 */
void cmdline_error(char *fmt, ...)
{
    va_list ap;
    char *stuff, morestuff[100];

    va_start(ap, fmt);
    stuff = dupvprintf(fmt, ap);
    va_end(ap);
    sprintf(morestuff, "%.70s Command Line Error", appname);
    MessageBox(hwnd, stuff, morestuff, MB_ICONERROR | MB_OK);
    sfree(stuff);
    exit(1);
}

/*
 * Actually do the job requested by a WM_NETEVENT
 */
static void enact_pending_netevent(void)
{
    static int reentering = 0;
    extern int select_result(WPARAM, LPARAM);

    if (reentering)
	return;			       /* don't unpend the pending */

    pending_netevent = FALSE;

    reentering = 1;
    select_result(pend_netevent_wParam, pend_netevent_lParam);
    reentering = 0;
}

/*
 * Copy the colour palette from the configuration data into defpal.
 * This is non-trivial because the colour indices are different.
 */
static void cfgtopalette(void)
{
    int i;
    static const int ww[] = {
	256, 257, 258, 259, 260, 261,
	0, 8, 1, 9, 2, 10, 3, 11,
	4, 12, 5, 13, 6, 14, 7, 15
    };

    for (i = 0; i < 22; i++) {
	int w = ww[i];
	defpal[w].rgbtRed = cfg.colours[i][0];
	defpal[w].rgbtGreen = cfg.colours[i][1];
	defpal[w].rgbtBlue = cfg.colours[i][2];
    }
    for (i = 0; i < NEXTCOLOURS; i++) {
	if (i < 216) {
	    int r = i / 36, g = (i / 6) % 6, b = i % 6;
	    defpal[i+16].rgbtRed = r ? r * 40 + 55 : 0;
	    defpal[i+16].rgbtGreen = g ? g * 40 + 55 : 0;
	    defpal[i+16].rgbtBlue = b ? b * 40 + 55 : 0;
	} else {
	    int shade = i - 216;
	    shade = shade * 10 + 8;
	    defpal[i+16].rgbtRed = defpal[i+16].rgbtGreen =
		defpal[i+16].rgbtBlue = shade;
	}
    }

    /* Override with system colours if appropriate */
    if (cfg.system_colour)
        systopalette();
}

/*
 * Override bit of defpal with colours from the system.
 * (NB that this takes a copy the system colours at the time this is called,
 * so subsequent colour scheme changes don't take effect. To fix that we'd
 * probably want to be using GetSysColorBrush() and the like.)
 */
static void systopalette(void)
{
    int i;
    static const struct { int nIndex; int norm; int bold; } or[] =
    {
	{ COLOR_WINDOWTEXT,	256, 257 }, /* Default Foreground */
	{ COLOR_WINDOW,		258, 259 }, /* Default Background */
	{ COLOR_HIGHLIGHTTEXT,	260, 260 }, /* Cursor Text */
	{ COLOR_HIGHLIGHT,	261, 261 }, /* Cursor Colour */
    };

    for (i = 0; i < (sizeof(or)/sizeof(or[0])); i++) {
	COLORREF colour = GetSysColor(or[i].nIndex);
	defpal[or[i].norm].rgbtRed =
	   defpal[or[i].bold].rgbtRed = GetRValue(colour);
	defpal[or[i].norm].rgbtGreen =
	   defpal[or[i].bold].rgbtGreen = GetGValue(colour);
	defpal[or[i].norm].rgbtBlue =
	   defpal[or[i].bold].rgbtBlue = GetBValue(colour);
    }
}

/*
 * Set up the colour palette.
 */
static void init_palette(void)
{
    int i;
    HDC hdc = GetDC(hwnd);
    if (hdc) {
	if (cfg.try_palette && GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) {
	    /*
	     * This is a genuine case where we must use smalloc
	     * because the snew macros can't cope.
	     */
	    logpal = smalloc(sizeof(*logpal)
			     - sizeof(logpal->palPalEntry)
			     + NALLCOLOURS * sizeof(PALETTEENTRY));
	    logpal->palVersion = 0x300;
	    logpal->palNumEntries = NALLCOLOURS;
	    for (i = 0; i < NALLCOLOURS; i++) {
		logpal->palPalEntry[i].peRed = defpal[i].rgbtRed;
		logpal->palPalEntry[i].peGreen = defpal[i].rgbtGreen;
		logpal->palPalEntry[i].peBlue = defpal[i].rgbtBlue;
		logpal->palPalEntry[i].peFlags = PC_NOCOLLAPSE;
	    }
	    pal = CreatePalette(logpal);
	    if (pal) {
		SelectPalette(hdc, pal, FALSE);
		RealizePalette(hdc);
		SelectPalette(hdc, GetStockObject(DEFAULT_PALETTE), FALSE);
	    }
	}
	ReleaseDC(hwnd, hdc);
    }
    if (pal)
	for (i = 0; i < NALLCOLOURS; i++)
	    colours[i] = PALETTERGB(defpal[i].rgbtRed,
				    defpal[i].rgbtGreen,
				    defpal[i].rgbtBlue);
    else
	for (i = 0; i < NALLCOLOURS; i++)
	    colours[i] = RGB(defpal[i].rgbtRed,
			     defpal[i].rgbtGreen, defpal[i].rgbtBlue);
}

/*
 * This is a wrapper to ExtTextOut() to force Windows to display
 * the precise glyphs we give it. Otherwise it would do its own
 * bidi and Arabic shaping, and we would end up uncertain which
 * characters it had put where.
 */
static void exact_textout(HDC hdc, int x, int y, CONST RECT *lprc,
			  unsigned short *lpString, UINT cbCount,
			  CONST INT *lpDx, int opaque)
{
#ifdef __LCC__
    /*
     * The LCC include files apparently don't supply the
     * GCP_RESULTSW type, but we can make do with GCP_RESULTS
     * proper: the differences aren't important to us (the only
     * variable-width string parameter is one we don't use anyway).
     */
    GCP_RESULTS gcpr;
#else
    GCP_RESULTSW gcpr;
#endif
    char *buffer = snewn(cbCount*2+2, char);
    char *classbuffer = snewn(cbCount, char);
    memset(&gcpr, 0, sizeof(gcpr));
    memset(buffer, 0, cbCount*2+2);
    memset(classbuffer, GCPCLASS_NEUTRAL, cbCount);

    gcpr.lStructSize = sizeof(gcpr);
    gcpr.lpGlyphs = (void *)buffer;
    gcpr.lpClass = (void *)classbuffer;
    gcpr.nGlyphs = cbCount;
    GetCharacterPlacementW(hdc, lpString, cbCount, 0, &gcpr,
			   FLI_MASK | GCP_CLASSIN | GCP_DIACRITIC);

    ExtTextOut(hdc, x, y,
	       ETO_GLYPH_INDEX | ETO_CLIPPED | (opaque ? ETO_OPAQUE : 0),
	       lprc, buffer, cbCount, lpDx);
}

/*
 * The exact_textout() wrapper, unfortunately, destroys the useful
 * Windows `font linking' behaviour: automatic handling of Unicode
 * code points not supported in this font by falling back to a font
 * which does contain them. Therefore, we adopt a multi-layered
 * approach: for any potentially-bidi text, we use exact_textout(),
 * and for everything else we use a simple ExtTextOut as we did
 * before exact_textout() was introduced.
 */
static void general_textout(HDC hdc, int x, int y, CONST RECT *lprc,
			    unsigned short *lpString, UINT cbCount,
			    CONST INT *lpDx, int opaque)
{
    int i, j, xp, xn;
    int bkmode = 0, got_bkmode = FALSE;

    xp = xn = x;

    for (i = 0; i < (int)cbCount ;) {
	int rtl = is_rtl(lpString[i]);

	xn += lpDx[i];

	for (j = i+1; j < (int)cbCount; j++) {
	    if (rtl != is_rtl(lpString[j]))
		break;
	    xn += lpDx[j];
	}

	/*
	 * Now [i,j) indicates a maximal substring of lpString
	 * which should be displayed using the same textout
	 * function.
	 */
	if (rtl) {
	    exact_textout(hdc, xp, y, lprc, lpString+i, j-i,
                          font_varpitch ? NULL : lpDx+i, opaque);
	} else {
	    ExtTextOutW(hdc, xp, y, ETO_CLIPPED | (opaque ? ETO_OPAQUE : 0),
			lprc, lpString+i, j-i,
                        font_varpitch ? NULL : lpDx+i);
	}

	i = j;
	xp = xn;

        bkmode = GetBkMode(hdc);
        got_bkmode = TRUE;
        SetBkMode(hdc, TRANSPARENT);
        opaque = FALSE;
    }

    if (got_bkmode)
        SetBkMode(hdc, bkmode);
}

static int get_font_width(HDC hdc, const TEXTMETRIC *tm)
{
    int ret;
    /* Note that the TMPF_FIXED_PITCH bit is defined upside down :-( */
    if (!(tm->tmPitchAndFamily & TMPF_FIXED_PITCH)) {
        ret = tm->tmAveCharWidth;
    } else {
#define FIRST '0'
#define LAST '9'
        ABCFLOAT widths[LAST-FIRST + 1];
        int j;

        font_varpitch = TRUE;
        font_dualwidth = TRUE;
        if (GetCharABCWidthsFloat(hdc, FIRST, LAST, widths)) {
            ret = 0;
            for (j = 0; j < lenof(widths); j++) {
                int width = (int)(0.5 + widths[j].abcfA +
                                  widths[j].abcfB + widths[j].abcfC);
                if (ret < width)
                    ret = width;
            }
        } else {
            ret = tm->tmMaxCharWidth;
        }
#undef FIRST
#undef LAST
    }
    return ret;
}

/*
 * Initialise all the fonts we will need initially. There may be as many as
 * three or as few as one.  The other (potentially) twenty-one fonts are done
 * if/when they are needed.
 *
 * We also:
 *
 * - check the font width and height, correcting our guesses if
 *   necessary.
 *
 * - verify that the bold font is the same width as the ordinary
 *   one, and engage shadow bolding if not.
 * 
 * - verify that the underlined font is the same width as the
 *   ordinary one (manual underlining by means of line drawing can
 *   be done in a pinch).
 */
static void init_fonts(int pick_width, int pick_height)
{
    TEXTMETRIC tm;
    CPINFO cpinfo;
    int fontsize[3];
    int i;
    HDC hdc;
    int fw_dontcare, fw_bold;

    for (i = 0; i < FONT_MAXNO; i++)
	fonts[i] = NULL;

    bold_mode = cfg.bold_colour ? BOLD_COLOURS : BOLD_FONT;
    und_mode = UND_FONT;

    if (cfg.font.isbold) {
	fw_dontcare = FW_BOLD;
	fw_bold = FW_HEAVY;
    } else {
	fw_dontcare = FW_DONTCARE;
	fw_bold = FW_BOLD;
    }

    hdc = GetDC(hwnd);

    if (pick_height)
	font_height = pick_height;
    else {
	font_height = cfg.font.height;
	if (font_height > 0) {
	    font_height =
		-MulDiv(font_height, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	}
    }
    font_width = pick_width;

#define f(i,c,w,u) \
    fonts[i] = CreateFont (font_height, font_width, 0, 0, w, FALSE, u, FALSE, \
			   c, OUT_DEFAULT_PRECIS, \
		           CLIP_DEFAULT_PRECIS, FONT_QUALITY(cfg.font_quality), \
			   FIXED_PITCH | FF_DONTCARE, cfg.font.name)

    f(FONT_NORMAL, cfg.font.charset, fw_dontcare, FALSE);

    SelectObject(hdc, fonts[FONT_NORMAL]);
    GetTextMetrics(hdc, &tm);

    GetObject(fonts[FONT_NORMAL], sizeof(LOGFONT), &lfont);

    /* Note that the TMPF_FIXED_PITCH bit is defined upside down :-( */
    if (!(tm.tmPitchAndFamily & TMPF_FIXED_PITCH)) {
        font_varpitch = FALSE;
        font_dualwidth = (tm.tmAveCharWidth != tm.tmMaxCharWidth);
    } else {
        font_varpitch = TRUE;
        font_dualwidth = TRUE;
    }
    if (pick_width == 0 || pick_height == 0) {
	font_height = tm.tmHeight;
        font_width = get_font_width(hdc, &tm);
    }

#ifdef RDB_DEBUG_PATCH
    debug(23, "Primary font H=%d, AW=%d, MW=%d",
	    tm.tmHeight, tm.tmAveCharWidth, tm.tmMaxCharWidth);
#endif

    {
	CHARSETINFO info;
	DWORD cset = tm.tmCharSet;
	memset(&info, 0xFF, sizeof(info));

	/* !!! Yes the next line is right */
	if (cset == OEM_CHARSET)
	    ucsdata.font_codepage = GetOEMCP();
	else
	    if (TranslateCharsetInfo ((DWORD *) cset, &info, TCI_SRCCHARSET))
		ucsdata.font_codepage = info.ciACP;
	else
	    ucsdata.font_codepage = -1;

	GetCPInfo(ucsdata.font_codepage, &cpinfo);
	ucsdata.dbcs_screenfont = (cpinfo.MaxCharSize > 1);
    }

    f(FONT_UNDERLINE, cfg.font.charset, fw_dontcare, TRUE);

    /*
     * Some fonts, e.g. 9-pt Courier, draw their underlines
     * outside their character cell. We successfully prevent
     * screen corruption by clipping the text output, but then
     * we lose the underline completely. Here we try to work
     * out whether this is such a font, and if it is, we set a
     * flag that causes underlines to be drawn by hand.
     *
     * Having tried other more sophisticated approaches (such
     * as examining the TEXTMETRIC structure or requesting the
     * height of a string), I think we'll do this the brute
     * force way: we create a small bitmap, draw an underlined
     * space on it, and test to see whether any pixels are
     * foreground-coloured. (Since we expect the underline to
     * go all the way across the character cell, we only search
     * down a single column of the bitmap, half way across.)
     */
    {
	HDC und_dc;
	HBITMAP und_bm, und_oldbm;
	int i, gotit;
	COLORREF c;

	und_dc = CreateCompatibleDC(hdc);
	und_bm = CreateCompatibleBitmap(hdc, font_width, font_height);
	und_oldbm = SelectObject(und_dc, und_bm);
	SelectObject(und_dc, fonts[FONT_UNDERLINE]);
	SetTextAlign(und_dc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
	SetTextColor(und_dc, RGB(255, 255, 255));
	SetBkColor(und_dc, RGB(0, 0, 0));
	SetBkMode(und_dc, OPAQUE);
	ExtTextOut(und_dc, 0, 0, ETO_OPAQUE, NULL, " ", 1, NULL);
	gotit = FALSE;
	for (i = 0; i < font_height; i++) {
	    c = GetPixel(und_dc, font_width / 2, i);
	    if (c != RGB(0, 0, 0))
		gotit = TRUE;
	}
	SelectObject(und_dc, und_oldbm);
	DeleteObject(und_bm);
	DeleteDC(und_dc);
	if (!gotit) {
	    und_mode = UND_LINE;
	    DeleteObject(fonts[FONT_UNDERLINE]);
	    fonts[FONT_UNDERLINE] = 0;
	}
    }

    if (bold_mode == BOLD_FONT) {
	f(FONT_BOLD, cfg.font.charset, fw_bold, FALSE);
    }
#undef f

    descent = tm.tmAscent + 1;
    if (descent >= font_height)
	descent = font_height - 1;

    for (i = 0; i < 3; i++) {
	if (fonts[i]) {
	    if (SelectObject(hdc, fonts[i]) && GetTextMetrics(hdc, &tm))
		fontsize[i] = get_font_width(hdc, &tm) + 256 * tm.tmHeight;
	    else
		fontsize[i] = -i;
	} else
	    fontsize[i] = -i;
    }

    ReleaseDC(hwnd, hdc);

    if (fontsize[FONT_UNDERLINE] != fontsize[FONT_NORMAL]) {
	und_mode = UND_LINE;
	DeleteObject(fonts[FONT_UNDERLINE]);
	fonts[FONT_UNDERLINE] = 0;
    }

    if (bold_mode == BOLD_FONT &&
	fontsize[FONT_BOLD] != fontsize[FONT_NORMAL]) {
	bold_mode = BOLD_SHADOW;
	DeleteObject(fonts[FONT_BOLD]);
	fonts[FONT_BOLD] = 0;
    }
    fontflag[0] = fontflag[1] = fontflag[2] = 1;

    init_ucs(&cfg, &ucsdata);
}

static void another_font(int fontno)
{
    int basefont;
    int fw_dontcare, fw_bold;
    int c, u, w, x;
    char *s;

    if (fontno < 0 || fontno >= FONT_MAXNO || fontflag[fontno])
	return;

    basefont = (fontno & ~(FONT_BOLDUND));
    if (basefont != fontno && !fontflag[basefont])
	another_font(basefont);

    if (cfg.font.isbold) {
	fw_dontcare = FW_BOLD;
	fw_bold = FW_HEAVY;
    } else {
	fw_dontcare = FW_DONTCARE;
	fw_bold = FW_BOLD;
    }

    c = cfg.font.charset;
    w = fw_dontcare;
    u = FALSE;
    s = cfg.font.name;
    x = font_width;

    if (fontno & FONT_WIDE)
	x *= 2;
    if (fontno & FONT_NARROW)
	x = (x+1)/2;
    if (fontno & FONT_OEM)
	c = OEM_CHARSET;
    if (fontno & FONT_BOLD)
	w = fw_bold;
    if (fontno & FONT_UNDERLINE)
	u = TRUE;

    fonts[fontno] =
	CreateFont(font_height * (1 + !!(fontno & FONT_HIGH)), x, 0, 0, w,
		   FALSE, u, FALSE, c, OUT_DEFAULT_PRECIS,
		   CLIP_DEFAULT_PRECIS, FONT_QUALITY(cfg.font_quality),
		   DEFAULT_PITCH | FF_DONTCARE, s);

    fontflag[fontno] = 1;
}

static void deinit_fonts(void)
{
    int i;
    for (i = 0; i < FONT_MAXNO; i++) {
	if (fonts[i])
	    DeleteObject(fonts[i]);
	fonts[i] = 0;
	fontflag[i] = 0;
    }
}

void request_resize(void *frontend, int w, int h)
{
    int width, height;

    /* If the window is maximized supress resizing attempts */
    if (IsZoomed(hwnd)) {
	if (cfg.resize_action == RESIZE_TERM)
	    return;
    }

    if (cfg.resize_action == RESIZE_DISABLED) return;
    if (h == term->rows && w == term->cols) return;

    /* Sanity checks ... */
    {
	static int first_time = 1;
	static RECT ss;

	switch (first_time) {
	  case 1:
	    /* Get the size of the screen */
	    if (get_fullscreen_rect(&ss))
		/* first_time = 0 */ ;
	    else {
		first_time = 2;
		break;
	    }
	  case 0:
	    /* Make sure the values are sane */
	    width = (ss.right - ss.left - extra_width) / 4;
	    height = (ss.bottom - ss.top - extra_height) / 6;

	    if (w > width || h > height)
		return;
	    if (w < 15)
		w = 15;
	    if (h < 1)
		h = 1;
	}
    }

    term_size(term, h, w, cfg.savelines);

    if (cfg.resize_action != RESIZE_FONT && !IsZoomed(hwnd)) {
	width = extra_width + font_width * w;
	height = extra_height + font_height * h;

	SetWindowPos(hwnd, NULL, 0, 0, width, height,
	    SWP_NOACTIVATE | SWP_NOCOPYBITS |
	    SWP_NOMOVE | SWP_NOZORDER);
    } else
	reset_window(0);

    InvalidateRect(hwnd, NULL, TRUE);
}

static void reset_window(int reinit) {
    /*
     * This function decides how to resize or redraw when the 
     * user changes something. 
     *
     * This function doesn't like to change the terminal size but if the
     * font size is locked that may be it's only soluion.
     */
    int win_width, win_height;
    RECT cr, wr;

#ifdef RDB_DEBUG_PATCH
    debug((27, "reset_window()"));
#endif

    /* Current window sizes ... */
    GetWindowRect(hwnd, &wr);
    GetClientRect(hwnd, &cr);

    win_width  = cr.right - cr.left;
    win_height = cr.bottom - cr.top;

    if (cfg.resize_action == RESIZE_DISABLED) reinit = 2;

    /* Are we being forced to reload the fonts ? */
    if (reinit>1) {
#ifdef RDB_DEBUG_PATCH
	debug((27, "reset_window() -- Forced deinit"));
#endif
	deinit_fonts();
	init_fonts(0,0);
    }

    /* Oh, looks like we're minimised */
    if (win_width == 0 || win_height == 0)
	return;

    /* Is the window out of position ? */
    if ( !reinit && 
	    (offset_width != (win_width-font_width*term->cols)/2 ||
	     offset_height != (win_height-font_height*term->rows)/2) ){
	offset_width = (win_width-font_width*term->cols)/2;
	offset_height = (win_height-font_height*term->rows)/2;
	InvalidateRect(hwnd, NULL, TRUE);
#ifdef RDB_DEBUG_PATCH
	debug((27, "reset_window() -> Reposition terminal"));
#endif
    }

    if (IsZoomed(hwnd)) {
	/* We're fullscreen, this means we must not change the size of
	 * the window so it's the font size or the terminal itself.
	 */

	extra_width = wr.right - wr.left - cr.right + cr.left;
	extra_height = wr.bottom - wr.top - cr.bottom + cr.top;

	if (cfg.resize_action != RESIZE_TERM) {
	    if (  font_width != win_width/term->cols || 
		  font_height != win_height/term->rows) {
		deinit_fonts();
		init_fonts(win_width/term->cols, win_height/term->rows);
		offset_width = (win_width-font_width*term->cols)/2;
		offset_height = (win_height-font_height*term->rows)/2;
		InvalidateRect(hwnd, NULL, TRUE);
#ifdef RDB_DEBUG_PATCH
		debug((25, "reset_window() -> Z font resize to (%d, %d)",
			font_width, font_height));
#endif
	    }
	} else {
	    if (  font_width * term->cols != win_width || 
		  font_height * term->rows != win_height) {
		/* Our only choice at this point is to change the 
		 * size of the terminal; Oh well.
		 */
		term_size(term, win_height/font_height, win_width/font_width,
			  cfg.savelines);
		offset_width = (win_width-font_width*term->cols)/2;
		offset_height = (win_height-font_height*term->rows)/2;
		InvalidateRect(hwnd, NULL, TRUE);
#ifdef RDB_DEBUG_PATCH
		debug((27, "reset_window() -> Zoomed term_size"));
#endif
	    }
	}
	return;
    }

    /* Hmm, a force re-init means we should ignore the current window
     * so we resize to the default font size.
     */
    if (reinit>0) {
#ifdef RDB_DEBUG_PATCH
	debug((27, "reset_window() -> Forced re-init"));
#endif

	offset_width = offset_height = cfg.window_border;
	extra_width = wr.right - wr.left - cr.right + cr.left + offset_width*2;
	extra_height = wr.bottom - wr.top - cr.bottom + cr.top +offset_height*2;

	if (win_width != font_width*term->cols + offset_width*2 ||
	    win_height != font_height*term->rows + offset_height*2) {

	    /* If this is too large windows will resize it to the maximum
	     * allowed window size, we will then be back in here and resize
	     * the font or terminal to fit.
	     */
	    SetWindowPos(hwnd, NULL, 0, 0, 
		         font_width*term->cols + extra_width, 
			 font_height*term->rows + extra_height,
			 SWP_NOMOVE | SWP_NOZORDER);
	}

	InvalidateRect(hwnd, NULL, TRUE);
	return;
    }

    /* Okay the user doesn't want us to change the font so we try the 
     * window. But that may be too big for the screen which forces us
     * to change the terminal.
     */
    if ((cfg.resize_action == RESIZE_TERM && reinit<=0) ||
        (cfg.resize_action == RESIZE_EITHER && reinit<0) ||
	    reinit>0) {
	offset_width = offset_height = cfg.window_border;
	extra_width = wr.right - wr.left - cr.right + cr.left + offset_width*2;
	extra_height = wr.bottom - wr.top - cr.bottom + cr.top +offset_height*2;

	if (win_width != font_width*term->cols + offset_width*2 ||
	    win_height != font_height*term->rows + offset_height*2) {

	    static RECT ss;
	    int width, height;
		
		get_fullscreen_rect(&ss);

	    width = (ss.right - ss.left - extra_width) / font_width;
	    height = (ss.bottom - ss.top - extra_height) / font_height;

	    /* Grrr too big */
	    if ( term->rows > height || term->cols > width ) {
		if (cfg.resize_action == RESIZE_EITHER) {
		    /* Make the font the biggest we can */
		    if (term->cols > width)
			font_width = (ss.right - ss.left - extra_width)
			    / term->cols;
		    if (term->rows > height)
			font_height = (ss.bottom - ss.top - extra_height)
			    / term->rows;

		    deinit_fonts();
		    init_fonts(font_width, font_height);

		    width = (ss.right - ss.left - extra_width) / font_width;
		    height = (ss.bottom - ss.top - extra_height) / font_height;
		} else {
		    if ( height > term->rows ) height = term->rows;
		    if ( width > term->cols )  width = term->cols;
		    term_size(term, height, width, cfg.savelines);
#ifdef RDB_DEBUG_PATCH
		    debug((27, "reset_window() -> term resize to (%d,%d)",
			       height, width));
#endif
		}
	    }
	    
	    SetWindowPos(hwnd, NULL, 0, 0, 
		         font_width*term->cols + extra_width, 
			 font_height*term->rows + extra_height,
			 SWP_NOMOVE | SWP_NOZORDER);

	    InvalidateRect(hwnd, NULL, TRUE);
#ifdef RDB_DEBUG_PATCH
	    debug((27, "reset_window() -> window resize to (%d,%d)",
			font_width*term->cols + extra_width,
			font_height*term->rows + extra_height));
#endif
	}
	return;
    }

    /* We're allowed to or must change the font but do we want to ?  */

    if (font_width != (win_width-cfg.window_border*2)/term->cols || 
	font_height != (win_height-cfg.window_border*2)/term->rows) {

	deinit_fonts();
	init_fonts((win_width-cfg.window_border*2)/term->cols, 
		   (win_height-cfg.window_border*2)/term->rows);
	offset_width = (win_width-font_width*term->cols)/2;
	offset_height = (win_height-font_height*term->rows)/2;

	extra_width = wr.right - wr.left - cr.right + cr.left +offset_width*2;
	extra_height = wr.bottom - wr.top - cr.bottom + cr.top+offset_height*2;

	InvalidateRect(hwnd, NULL, TRUE);
#ifdef RDB_DEBUG_PATCH
	debug((25, "reset_window() -> font resize to (%d,%d)", 
		   font_width, font_height));
#endif
    }
}

static void set_input_locale(HKL kl)
{
    char lbuf[20];

    GetLocaleInfo(LOWORD(kl), LOCALE_IDEFAULTANSICODEPAGE,
		  lbuf, sizeof(lbuf));

    kbd_codepage = atoi(lbuf);
}

static void click(Mouse_Button b, int x, int y, int shift, int ctrl, int alt)
{
    int thistime = GetMessageTime();

    if (send_raw_mouse && !(cfg.mouse_override && shift)) {
	lastbtn = MBT_NOTHING;
	term_mouse(term, b, translate_button(b), MA_CLICK,
		   x, y, shift, ctrl, alt);
	return;
    }

    if (lastbtn == b && thistime - lasttime < dbltime) {
	lastact = (lastact == MA_CLICK ? MA_2CLK :
		   lastact == MA_2CLK ? MA_3CLK :
		   lastact == MA_3CLK ? MA_CLICK : MA_NOTHING);
    } else {
	lastbtn = b;
	lastact = MA_CLICK;
    }
    if (lastact != MA_NOTHING)
	term_mouse(term, b, translate_button(b), lastact,
		   x, y, shift, ctrl, alt);
    lasttime = thistime;
}

/*
 * Translate a raw mouse button designation (LEFT, MIDDLE, RIGHT)
 * into a cooked one (SELECT, EXTEND, PASTE).
 */
static Mouse_Button translate_button(Mouse_Button button)
{
    if (button == MBT_LEFT)
	return MBT_SELECT;
    if (button == MBT_MIDDLE)
	return cfg.mouse_is_xterm == 1 ? MBT_PASTE : MBT_EXTEND;
    if (button == MBT_RIGHT)
	return cfg.mouse_is_xterm == 1 ? MBT_EXTEND : MBT_PASTE;
    return 0;			       /* shouldn't happen */
}

static void show_mouseptr(int show)
{
    /* NB that the counter in ShowCursor() is also frobbed by
     * update_mouse_pointer() */
    static int cursor_visible = 1;
    if (!cfg.hide_mouseptr)	       /* override if this feature disabled */
	show = 1;
    if (cursor_visible && !show)
	ShowCursor(FALSE);
    else if (!cursor_visible && show)
	ShowCursor(TRUE);
    cursor_visible = show;
}

static int is_alt_pressed(void)
{
    BYTE keystate[256];
    int r = GetKeyboardState(keystate);
    if (!r)
	return FALSE;
    if (keystate[VK_MENU] & 0x80)
	return TRUE;
    if (keystate[VK_RMENU] & 0x80)
	return TRUE;
    return FALSE;
}
#if (!defined IMAGEPORT) || (defined FDJ)
static int resizing ;
#endif

void notify_remote_exit(void *fe)
{
    int exitcode;

    if (!session_closed &&
        (exitcode = back->exitcode(backhandle)) >= 0) {
	/* Abnormal exits will already have set session_closed and taken
	 * appropriate action. */
	if (cfg.close_on_exit == FORCE_ON ||
	    (cfg.close_on_exit == AUTO && exitcode != INT_MAX)) {
	    PostQuitMessage(0);
	} else {
	    must_close_session = TRUE;
	    session_closed = TRUE;
	    /* exitcode == INT_MAX indicates that the connection was closed
	     * by a fatal error, so an error box will be coming our way and
	     * we should not generate this informational one. */
	    if (exitcode != INT_MAX)
		MessageBox(hwnd, "Connection closed by remote host",
			   appname, MB_OK | MB_ICONINFORMATION);
	}
    }
}

void timer_change_notify(long next)
{
    long ticks = next - GETTICKCOUNT();
    if (ticks <= 0) ticks = 1;	       /* just in case */
    KillTimer(hwnd, TIMING_TIMER_ID);
    SetTimer(hwnd, TIMING_TIMER_ID, ticks, NULL);
    timing_next_time = next;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
				WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    static int ignore_clip = FALSE;
    static int need_backend_resize = FALSE;
    static int fullscr_on_max = FALSE;
    static int processed_resize = FALSE;
    static UINT last_mousemove = 0;
#ifdef HYPERLINKPORT
	/*
	 * HACK: PuttyTray / Nutty
	 */ 
	POINT cursor_pt;
#endif

    switch (message) {
      case WM_TIMER:
	if ((UINT_PTR)wParam == TIMING_TIMER_ID) {
	    long next;

	    KillTimer(hwnd, TIMING_TIMER_ID);
	    if (run_timers(timing_next_time, &next)) {
		timer_change_notify(next);
	    } else {
	    }
	}
#ifdef PERSOPORT
else if((UINT_PTR)wParam == TIMER_INIT) {  // Initialisation
	char buffer[4096] = "" ;

	if( (cfg.protocol == PROT_SSH) && (!backend_connected) ) break ; // On sort si en SSH on n'est pas connecte
	// Lancement d'une (ou plusieurs s�par�es par \\n) commande(s) automatique(s) � l'initialisation
	KillTimer( hwnd, TIMER_INIT ) ;

	if( cfg.protocol == PROT_TELNET ) {
		if( strlen( cfg.username ) > 0 ) {
			strcpy( buffer, cfg.username ) ;
			if( strlen( cfg.password ) > 0 ) {
				strcat( buffer, "\\n\\p" ) ; 
				MASKPASS(cfg.password); strcat( buffer, cfg.password ) ; MASKPASS(cfg.password);
				strcat( buffer, "\\n" ) ;
				}
			}
		if( strlen( cfg.autocommand ) > 0 ) {
			strcat( buffer, "\\n\\p" ) ; 
			strcat( buffer, cfg.autocommand ) ;
			strcat( buffer, "\\n" ) ;
			}
		if( strlen(buffer) > 0 ) strcpy( cfg.autocommand, buffer ) ;
		}

	// Envoi automatiquement dans le systeme tray si besoin
	if( AutoSendToTray ) ManageToTray( hwnd ) ;
		
	// Affichage d'une note de la session s'il y en a une
	if( GetSessionField( cfg.sessionname, cfg.folder, "Notes", buffer )  )
		{ if( strlen( buffer ) > 0 ) MessageBox( hwnd, buffer, "Notes", MB_OK ) ; }

	RenewPassword( &cfg ) ;
	
	if( strlen( cfg.autocommand ) > 0 ) {
		SetTimer(hwnd, TIMER_AUTOCOMMAND, (int)(autocommand_delay*1000), NULL) ;
		logevent(NULL, "Send automatic command" );
		}
	if( cfg.logtimerotation > 0 ) {
		SetTimer(hwnd, TIMER_LOGROTATION, (int)(cfg.logtimerotation*1000), NULL) ;
		logevent(NULL, "Start log rotation" );
		}

	RefreshBackground( hwnd ) ;
	}

else if((UINT_PTR)wParam == TIMER_AUTOCOMMAND) {  // Autocommand au d�marrage
	char buffer[4096] = "" ;
	int i = 0  ;
	KillTimer( hwnd, TIMER_AUTOCOMMAND ) ; 
	if( AutoCommand == NULL ) { 
		ValidateRect( hwnd, NULL ) ; 
		GetSessionField( cfg.sessionname, cfg.folder, "Autocommand", cfg.autocommand ) ;
		AutoCommand = cfg.autocommand ; 
//logevent(NULL, AutoCommand );
		}
	while( AutoCommand[i] != '\0' ) {
		if( AutoCommand[i]=='\n' ) { i++ ; break ; }
		else if( (AutoCommand[i] == '\\') && (AutoCommand[i+1] == 'n') ) { i += 2 ; break ; }
		else if( (AutoCommand[i] == '\\') && (AutoCommand[i+1] == 'p') ) {
			strcat( buffer, "\\p" ) ;
			//Sleep( 1000 ) ;
			i += 2 ;
			break ;
			}
		else if( (AutoCommand[i] == '\\') && (AutoCommand[i+1] == 's') ) {
			strcat( buffer, "\\s" ) ;
			i += 2 ;
			buffer[i] = AutoCommand[i] ; buffer[i+1] = '\0' ; i++ ;
			buffer[i] = AutoCommand[i] ; buffer[i+1] = '\0' ; i++ ;
			break ;
			}
		else { buffer[i] = AutoCommand[i] ; buffer[i+1] = '\0' ; i++ ; }
		}
	//if( AutoCommand[i] != '\0' ) { AutoCommand += i ; }
	AutoCommand += i ;
	if( strlen( buffer ) > 0 ) { SendAutoCommand( hwnd, buffer ) ; }
	if( AutoCommand[0] == '\0' ) { 
		AutoCommand = NULL ; 
		GetSessionField( cfg.sessionname, cfg.folder, "Autocommand", cfg.autocommand ) ;
		InvalidateRect( hwnd, NULL, TRUE ) ;
		}
	else { SetTimer(hwnd, TIMER_AUTOCOMMAND, (int)(autocommand_delay*1000), NULL) ; }
	}
else if((UINT_PTR)wParam == TIMER_AUTOPASTE) {  // AutoPaste
	char buffer[4096] = "" ;
	int i = 0, j  ;
	KillTimer( hwnd, TIMER_AUTOPASTE ) ; 
	if( PasteCommand == NULL ) { ValidateRect( hwnd, NULL ) ; }
	else if( strlen( PasteCommand ) == 0 ) { free( PasteCommand ) ; PasteCommand = NULL ; }
	else {
		while( PasteCommand[0] != '\0' ) { 
			buffer[i] = PasteCommand[0] ; buffer[i+1] = '\0' ;
			j = 0 ; do { PasteCommand[j] = PasteCommand[j+1] ; j++ ; } while( PasteCommand[j] != '\0' ) ;
			if( buffer[i] == '\n' ) { buffer[i] = '\0' ; break ; }
			i++ ;
			}
		if( strlen( buffer ) > 0 ) { 
			SendAutoCommand( hwnd, buffer ) ;
			SetTimer(hwnd, TIMER_AUTOPASTE, (int)(autocommand_delay*1000), NULL) ;
			}
		}
	}
#if (defined IMAGEPORT) && (!defined FDJ)
else if( BackgroundImageFlag && ((UINT_PTR)wParam == TIMER_SLIDEBG) ) {  // Changement automatique de l'image de fond
	NextBgImage( hwnd ) ;
	InvalidateRect( hwnd, NULL, TRUE ) ;
	}
#endif
else if((UINT_PTR)wParam == TIMER_REDRAW) {  // rafraichissement automatique (bug d'affichage)
	InvalidateRect( hwnd, NULL, TRUE ) ;
	
	// Envoi de l'anti-idle
	if( AntiIdleCount++ >= AntiIdleCountMax ) {
	//if( AntiIdleCount++ >= 0 )  {
		AntiIdleCount = 0 ;
		//if(strlen( cfg.antiidle ) > 0) SendAutoCommand( hwnd, cfg.antiidle ) ;
		//else if( strlen( AntiIdleStr ) > 0 ) SendAutoCommand( hwnd, AntiIdleStr ) ;
		if(strlen( cfg.antiidle ) > 0) SendAutoCommand( hwnd, cfg.antiidle ) ;
		else if( strlen( AntiIdleStr ) > 0 ) SendAutoCommand( hwnd, AntiIdleStr ) ;
		}
	}
else if((UINT_PTR)wParam == TIMER_BLINKTRAYICON) {  // Clignotement de l'icone dans le systeme tray sur reception d'un signal BELL (print '\007' pour simuler)
	static int BlinkingState = 0 ;
	static hBlinkingIcon = NULL ; 

	if( VisibleFlag!=VISIBLE_TRAY ) 
		{ KillTimer( hwnd, TIMER_BLINKTRAYICON ) ; TrayIcone.hIcon = hBlinkingIcon ; BlinkingState = 0 ; break ; }
	if( BlinkingState==0 ) {
		hBlinkingIcon = TrayIcone.hIcon ;
		TrayIcone.hIcon = LoadIcon(NULL, NULL) ;
		Shell_NotifyIcon(NIM_MODIFY, &TrayIcone) ;
		BlinkingState = 1 ;
		}
	else {
		if( hBlinkingIcon ) {
			TrayIcone.hIcon = hBlinkingIcon ;
			Shell_NotifyIcon(NIM_MODIFY, &TrayIcone) ;
			BlinkingState = 0 ;
			}
		}
	}
else if((UINT_PTR)wParam == TIMER_LOGROTATION) {  // log rotation
	timestamp_change_filename() ;
	}
#endif
	return 0;
      case WM_CREATE:
#ifdef PERSOPORT
	      {
		// Initialiation
		MainHwnd = hwnd ;
		SetNewIcon( hwnd, cfg, SI_INIT ) ;

		// Gestion de la transparence
		if( TransparencyNumber >= 0 ) {
			SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED ) ;
			//SetLayeredWindowAttributes(hwnd, 0, TransparencyNumber, LWA_ALPHA) ;
			SetTransparency( hwnd, TransparencyNumber ) ;
			}

#if (defined IMAGEPORT) && (!defined FDJ)
		if( IconeFlag == -1 ) cfg.bg_type = 0 ;
		if( !BackgroundImageFlag ) cfg.bg_type = 0 ;
#endif
		if( !PuttyFlag )
		if( cfg.saveonexit && cfg.windowstate ) PostMessage( hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, (LPARAM)NULL );
		}
#endif
	break;
      case WM_CLOSE:
	{
#ifdef PERSOPORT
	if( ProtectFlag ) {
		MessageBox(hwnd,
			   "You are not allowed to close a protected window",
			   "Exit warning", MB_ICONERROR | MB_OK ) ;
		return 0 ;
		}
	    char *str;
	    show_mouseptr(1);
	    str = dupprintf("%s Exit Confirmation", appname);
	    if (!cfg.warn_on_close || session_closed ||
		MessageBox(hwnd,
			   "Are you sure you want to close this session?",
			   str, MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON1)
		== IDOK) {
			if( strlen( cfg.autocommandout ) > 0 ) { //Envoie d'une command automatique en sortant
				SendAutoCommand( hwnd, cfg.autocommandout ) ;
				strcpy( cfg.autocommandout, "" );
				}
			DestroyWindow(hwnd);
			}
	    sfree(str);
#else
	    char *str;
	    show_mouseptr(1);
	    str = dupprintf("%s Exit Confirmation", appname);
	    if (!cfg.warn_on_close || session_closed ||
		MessageBox(hwnd,
			   "Are you sure you want to close this session?",
			   str, MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON1)
		== IDOK)
		DestroyWindow(hwnd);
	    sfree(str);
#endif
	}
	return 0;
      case WM_DESTROY:
	show_mouseptr(1);
	PostQuitMessage(0);
	return 0;
      case WM_INITMENUPOPUP:
	if ((HMENU)wParam == savedsess_menu) {
	    /* About to pop up Saved Sessions sub-menu.
	     * Refresh the session list. */
	    get_sesslist(&sesslist, FALSE); /* free */
	    get_sesslist(&sesslist, TRUE);
	    update_savedsess_menu();
	    return 0;
	}
	break;
      case WM_COMMAND:
      case WM_SYSCOMMAND:
#ifdef PERSOPORT
	if( strlen(cfg.folder)>0 ) SetInitCurrentFolder( cfg.folder );
	if( (wParam>=IDM_USERCMD)&&(wParam<(IDM_USERCMD+NB_MENU_MAX)) ) {
	  	ManageSpecialCommand( hwnd, wParam-IDM_USERCMD, cfg ) ;
	        break ;
		}
#endif
	switch (wParam & ~0xF) {       /* low 4 bits reserved to Windows */
	  case IDM_SHOWLOG:
	    showeventlog(hwnd);
	    break;
	  case IDM_NEWSESS:
	  case IDM_DUPSESS:
	  case IDM_SAVEDSESS:
	    {
		char b[2048];
#ifdef PERSOPORT
		char c[180], *cl;
#else
		char c[30], *cl;
#endif
		int freecl = FALSE;
		BOOL inherit_handles;
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		HANDLE filemap = NULL;

		if (wParam == IDM_DUPSESS) {
#ifdef PERSOPORT
			MASKPASS(cfg.password);
#endif
		    /*
		     * Allocate a file-mapping memory chunk for the
		     * config structure.
		     */
		    SECURITY_ATTRIBUTES sa;
		    Config *p;

		    sa.nLength = sizeof(sa);
		    sa.lpSecurityDescriptor = NULL;
		    sa.bInheritHandle = TRUE;
		    filemap = CreateFileMapping(INVALID_HANDLE_VALUE,
						&sa,
						PAGE_READWRITE,
						0, sizeof(Config), NULL);
		    if (filemap && filemap != INVALID_HANDLE_VALUE) {
			p = (Config *) MapViewOfFile(filemap,
						     FILE_MAP_WRITE,
						     0, 0, sizeof(Config));
			if (p) {
			    *p = cfg;  /* structure copy */
			    UnmapViewOfFile(p);
			}
		    }
		    inherit_handles = TRUE;
		    sprintf(c, "putty &%p", filemap);
		    cl = c;
#ifdef PERSOPORT
			MASKPASS(cfg.password);
#endif
		} else if (wParam == IDM_SAVEDSESS) {
		    unsigned int sessno = ((lParam - IDM_SAVED_MIN)
					   / MENU_SAVED_STEP) + 1;
		    if (sessno < (unsigned)sesslist.nsessions) {
			char *session = sesslist.sessions[sessno];
			cl = dupprintf("putty @%s", session);
#ifdef PERSOPORT
			GetSessionFolderName( cfg.sessionname, cfg.folder ) ;
			if( DirectoryBrowseFlag ) if( strcmp(cfg.folder, "")&&strcmp(cfg.folder,"Default") )
				{ strcat( cl, " -folder \"" ) ; strcat( cl, cfg.folder ) ; strcat( cl, "\"" ) ; }
#endif
			inherit_handles = FALSE;
			freecl = TRUE;
		    } else
			break;
#ifdef PERSOPORT
	    } else /* IDM_NEWSESS */ {
		if( strcmp(cfg.folder, "")&&strcmp(cfg.folder,"Default") )
			sprintf(c, "putty -folder \"%s\"", cfg.folder ) ;
		else c[0]='\0' ;
		cl = c ;
#else
		} else /* IDM_NEWSESS */ {
		    cl = NULL;
#endif
		inherit_handles = FALSE;
		}

		GetModuleFileName(NULL, b, sizeof(b) - 1);
		si.cb = sizeof(si);
		si.lpReserved = NULL;
		si.lpDesktop = NULL;
		si.lpTitle = NULL;
		si.dwFlags = 0;
		si.cbReserved2 = 0;
		si.lpReserved2 = NULL;
		CreateProcess(b, cl, NULL, NULL, inherit_handles,
			      NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);

		if (filemap)
		    CloseHandle(filemap);
		if (freecl)
		    sfree(cl);
	    }
	    break;
	  case IDM_RESTART:
	    if (!back) {
		logevent(NULL, "----- Session restarted -----");
		term_pwron(term, FALSE);
#ifdef PERSOPORT
		backend_connected = 0 ;
		start_backend();
		SetTimer(hwnd, TIMER_INIT, (int)(init_delay*1000), NULL) ;
#else
		start_backend();
#endif
	    }

	    break;
	  case IDM_RECONF:
	    {
		Config prev_cfg;
		int init_lvl = 1;
		int reconfig_result;

		if (reconfiguring)
		    break;
		else
		    reconfiguring = TRUE;
#ifndef PERSOPORT
		GetWindowText(hwnd, cfg.wintitle, sizeof(cfg.wintitle));
#endif

		prev_cfg = cfg;

		reconfig_result =
		    do_reconfig(hwnd, back ? back->cfg_info(backhandle) : 0);
		reconfiguring = FALSE;
		if (!reconfig_result)
		    break;
#ifdef PERSOPORT
		SaveRegistryKey() ;
#endif
#if (defined IMAGEPORT) && (!defined FDJ)
		if( BackgroundImageFlag && (!PuttyFlag) ) {
			if(textdc) {
				DeleteObject(textbm);
				DeleteDC(textdc);
				textdc = NULL;
				textbm = NULL;
			}
			if(backgrounddc) {
				DeleteObject(backgroundbm);
				DeleteDC(backgrounddc);
				backgrounddc = NULL;
				backgroundbm = NULL;
				}
			if(backgroundblenddc) {
				DeleteObject(backgroundblendbm);
				DeleteDC(backgroundblenddc);
				backgroundblenddc = NULL;
				backgroundblendbm = NULL;
				}
			}
#endif

		{
		    /* Disable full-screen if resizing forbidden */
		    int i;
		    for (i = 0; i < lenof(popup_menus); i++)
			EnableMenuItem(popup_menus[i].menu, IDM_FULLSCREEN,
				       MF_BYCOMMAND | 
				       (cfg.resize_action == RESIZE_DISABLED)
				       ? MF_GRAYED : MF_ENABLED);
		    /* Gracefully unzoom if necessary */
		    if (IsZoomed(hwnd) &&
			(cfg.resize_action == RESIZE_DISABLED)) {
			ShowWindow(hwnd, SW_RESTORE);
		    }
		}

		/* Pass new config data to the logging module */
		log_reconfig(logctx, &cfg);

		sfree(logpal);
		/*
		 * Flush the line discipline's edit buffer in the
		 * case where local editing has just been disabled.
		 */
		if (ldisc)
		    ldisc_send(ldisc, NULL, 0, 0);
		if (pal)
		    DeleteObject(pal);
		logpal = NULL;
		pal = NULL;
		cfgtopalette();
		init_palette();

		/* Pass new config data to the terminal */
		term_reconfig(term, &cfg);

		/* Pass new config data to the back end */
		if (back)
		    back->reconfig(backhandle, &cfg);

		/* Screen size changed ? */
		if (cfg.height != prev_cfg.height ||
		    cfg.width != prev_cfg.width ||
		    cfg.savelines != prev_cfg.savelines ||
		    cfg.resize_action == RESIZE_FONT ||
		    (cfg.resize_action == RESIZE_EITHER && IsZoomed(hwnd)) ||
		    cfg.resize_action == RESIZE_DISABLED)
		    term_size(term, cfg.height, cfg.width, cfg.savelines);

		/* Enable or disable the scroll bar, etc */
		{
		    LONG nflg, flag = GetWindowLongPtr(hwnd, GWL_STYLE);
		    LONG nexflag, exflag =
			GetWindowLongPtr(hwnd, GWL_EXSTYLE);

		    nexflag = exflag;
		    if (cfg.alwaysontop != prev_cfg.alwaysontop) {
			if (cfg.alwaysontop) {
			    nexflag |= WS_EX_TOPMOST;
			    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE);
			} else {
			    nexflag &= ~(WS_EX_TOPMOST);
			    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE);
			}
		    }
		    if (cfg.sunken_edge)
			nexflag |= WS_EX_CLIENTEDGE;
		    else
			nexflag &= ~(WS_EX_CLIENTEDGE);

		    nflg = flag;
		    if (is_full_screen() ?
			cfg.scrollbar_in_fullscreen : cfg.scrollbar)
			nflg |= WS_VSCROLL;
		    else
			nflg &= ~WS_VSCROLL;

		    if (cfg.resize_action == RESIZE_DISABLED ||
                        is_full_screen())
			nflg &= ~WS_THICKFRAME;
		    else
			nflg |= WS_THICKFRAME;

		    if (cfg.resize_action == RESIZE_DISABLED)
			nflg &= ~WS_MAXIMIZEBOX;
		    else
			nflg |= WS_MAXIMIZEBOX;

		    if (nflg != flag || nexflag != exflag) {
			if (nflg != flag)
			    SetWindowLongPtr(hwnd, GWL_STYLE, nflg);
			if (nexflag != exflag)
			    SetWindowLongPtr(hwnd, GWL_EXSTYLE, nexflag);

			SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
				     SWP_NOACTIVATE | SWP_NOCOPYBITS |
				     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
				     SWP_FRAMECHANGED);

			init_lvl = 2;
		    }
		}

		/* Oops */
		if (cfg.resize_action == RESIZE_DISABLED && IsZoomed(hwnd)) {
		    force_normal(hwnd);
		    init_lvl = 2;
		}

		set_title(NULL, cfg.wintitle);
		if (IsIconic(hwnd)) {
		    SetWindowText(hwnd,
				  cfg.win_name_always ? window_name :
				  icon_name);
		}

		if (strcmp(cfg.font.name, prev_cfg.font.name) != 0 ||
		    strcmp(cfg.line_codepage, prev_cfg.line_codepage) != 0 ||
		    cfg.font.isbold != prev_cfg.font.isbold ||
		    cfg.font.height != prev_cfg.font.height ||
		    cfg.font.charset != prev_cfg.font.charset ||
		    cfg.font_quality != prev_cfg.font_quality ||
		    cfg.vtmode != prev_cfg.vtmode ||
		    cfg.bold_colour != prev_cfg.bold_colour ||
		    cfg.resize_action == RESIZE_DISABLED ||
		    cfg.resize_action == RESIZE_EITHER ||
		    (cfg.resize_action != prev_cfg.resize_action))
		    init_lvl = 2;

		InvalidateRect(hwnd, NULL, TRUE);
		reset_window(init_lvl);
		net_pending_errors();
#ifdef PERSOPORT
		SetNewIcon( hwnd, cfg, SI_INIT ) ;
		//if( (cfg.icone != 0) && ( IconeFlag > 0 ) ) {
		//	IconeNum = cfg.icone - 1 ;
		//	SendMessage( hwnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon( hInstIcons, MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum ) ) );
		//	}
#endif
	    }
	    break;
	  case IDM_COPYALL:
	    term_copyall(term);
	    break;
	  case IDM_PASTE:
#ifdef PERSOPORT
	    if( !ProtectFlag ) 
#endif
	    request_paste(NULL);
	    break;
	  case IDM_CLRSB:
	    term_clrsb(term);
	    break;
	  case IDM_RESET:
	    term_pwron(term, TRUE);
	    if (ldisc)
		ldisc_send(ldisc, NULL, 0, 0);
	    break;
	  case IDM_ABOUT:
	    showabout(hwnd);
	    break;
#ifdef PERSOPORT
	  /*case IDM_USERCMD:  
	  	ManageSpecialCommand( hwnd, wParam-IDM_USERCMD, cfg ) ;
	        break ;*/
	  case IDM_QUIT:
		SendMessage( hwnd, MYWM_NOTIFYICON, 0, WM_LBUTTONDBLCLK ) ;
		PostQuitMessage( 0 ) ;
		break ;
	  case IDM_PROTECT: 
	  	ManageProtect( hwnd, cfg ) ;
		break ;
	  case IDM_VISIBLE: 
	  	ManageVisible( hwnd ) ;
		break ;
          case IDM_TOTRAY: 
		if( VisibleFlag==VISIBLE_YES ) {
			VisibleFlag = VISIBLE_TRAY ;
			return ManageToTray( hwnd ) ;
			}
		break ;
	  case IDM_FROMTRAY:
		if( VisibleFlag==VISIBLE_TRAY ) {
			VisibleFlag = VISIBLE_YES ;
			return SendMessage( hwnd, MYWM_NOTIFYICON, 0, WM_LBUTTONDBLCLK ) ;
			}
		break ;
	  case IDM_HIDE:
		if( VisibleFlag==VISIBLE_YES ) {
			/*if (IsWindowVisible(hwnd) )*/ ShowWindow(hwnd, SW_HIDE) ;
			VisibleFlag = VISIBLE_NO ;
			}
		break ;
	  case IDM_UNHIDE:
		if( VisibleFlag==VISIBLE_NO ) {
			ShowWindow(hwnd, SW_RESTORE ) ;
			VisibleFlag = VISIBLE_YES ;
			}
		break ;
	  case IDM_SWITCH_HIDE:
		if( VisibleFlag==VISIBLE_YES ) SendMessage( hwnd, WM_COMMAND, IDM_HIDE, 0 ) ;
		else if( VisibleFlag==VISIBLE_NO ) SendMessage( hwnd, WM_COMMAND, IDM_UNHIDE, 0 ) ;
		break ;
	  case IDM_GONEXT:
		GoNext( hwnd ) ;
		break ;
	  case IDM_GOPREVIOUS:
		GoPrevious( hwnd ) ;
		break ;
          case IDM_WINROL: 
	    	ManageWinrol( hwnd ) ;
		RefreshBackground( hwnd ) ;
		break ;
	  case IDM_SCRIPTFILE :
		OpenAndSendScriptFile( hwnd ) ;
		break ;
	  case IDM_PSCP:
		SendFile( hwnd ) ;
		break ;
	  case IDM_WINSCP:
		StartWinSCP( hwnd, NULL ) ;
		break ;
	  case IDM_PRINT:
		ManagePrint( hwnd ) ;
		break ;
	  case IDM_SHOWPORTFWD:
//debug_log( "%d %d %d %d\n",offset_width, offset_height, font_width, font_height ) ;
		ShowPortfwd( hwnd ) ;
		break ;
	  case IDM_TRANSPARUP: // Augmenter la transparence
		if( TransparencyNumber >= 0 ) {
			if( TransparencyNumber < 255 ) {
			if( TransparencyNumber==10 ) TransparencyNumber = TransparencyNumber + 1 ;
			TransparencyNumber = TransparencyNumber + 10 ;
			if( TransparencyNumber > 255 ) TransparencyNumber = 255 ;
			cfg.transparencynumber = 255 - TransparencyNumber ;
			//SetLayeredWindowAttributes(hwnd, 0, TransparencyNumber, LWA_ALPHA);
			SetTransparency( hwnd, TransparencyNumber ) ;
			}
		}
		break ;
	  case IDM_TRANSPARDOWN: // Dominuer la transparence
		if( TransparencyNumber > 0 ) {
			if( TransparencyNumber==10 ) MessageBox( hwnd, "      KiTTY made by      \r\nCyril Dupont\r\nLeonard Nero", "About", MB_OK ) ;
			TransparencyNumber = TransparencyNumber - 10 ;
			if( TransparencyNumber < 0 ) TransparencyNumber = 0 ;
			cfg.transparencynumber = 255 - TransparencyNumber ;
			//SetLayeredWindowAttributes(hwnd, 0, TransparencyNumber, LWA_ALPHA);
			SetTransparency( hwnd, TransparencyNumber ) ;
			}
		break ;
	  case IDM_RESIZE: //Redmensionner
		{
		int w = LOWORD( lParam ), h = HIWORD( lParam ) ;
		if (w < 1) w = 1 ; if (h < 1) h = 1 ;
		cfg.width = w ; cfg.height = h ;
		term_size( term, h, w, cfg.savelines) ;
		reset_window(0);
		}
		break ;
	  case IDM_REPOS: //Redmensionner
		{
		int x = LOWORD( lParam ), y = HIWORD( lParam ) ;
		if (x < 1) x = 1 ; if (y < 1) y = 1 ;
		cfg.xpos = x ; cfg.ypos = y ;
		SetWindowPos( hwnd, 0, x, y, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE ) ;
		//MoveWindow(hwnd, x, y, 0, 0, TRUE ) ;
		//reset_window(0);
		}
		break ;
#endif

	  case IDM_HELP:
	    launch_help(hwnd, NULL);
	    break;
	  case SC_MOUSEMENU:
	    /*
	     * We get this if the System menu has been activated
	     * using the mouse.
	     */
	    show_mouseptr(1);
	    break;
          case SC_KEYMENU:
	    /*
	     * We get this if the System menu has been activated
	     * using the keyboard. This might happen from within
	     * TranslateKey, in which case it really wants to be
	     * followed by a `space' character to actually _bring
	     * the menu up_ rather than just sitting there in
	     * `ready to appear' state.
	     */
	    show_mouseptr(1);	       /* make sure pointer is visible */
	    if( lParam == 0 )
		PostMessage(hwnd, WM_CHAR, ' ', 0);
	    break;
	  case IDM_FULLSCREEN:
	    flip_full_screen();
	    break;
#ifdef ZMODEMPORT
	case IDM_XYZSTART:
		if( ZModemFlag ) {
		xyz_ReceiveInit(term);
		xyz_updateMenuItems(term);
		}
		break;
	case IDM_XYZUPLOAD:
		if( ZModemFlag ) {
		xyz_StartSending(term);
		xyz_updateMenuItems(term);
		}
		break;
	case IDM_XYZABORT:
		xyz_Cancel(term);
		xyz_updateMenuItems(term);
		break;
#endif
	  default:
	    if (wParam >= IDM_SAVED_MIN && wParam < IDM_SAVED_MAX) {
		SendMessage(hwnd, WM_SYSCOMMAND, IDM_SAVEDSESS, wParam);
	    }
	    if (wParam >= IDM_SPECIAL_MIN && wParam <= IDM_SPECIAL_MAX) {
		int i = (wParam - IDM_SPECIAL_MIN) / 0x10;
		/*
		 * Ensure we haven't been sent a bogus SYSCOMMAND
		 * which would cause us to reference invalid memory
		 * and crash. Perhaps I'm just too paranoid here.
		 */
		if (i >= n_specials)
		    break;
		if (back)
		    back->special(backhandle, specials[i].code);
		net_pending_errors();
	    }
	}
	break;
#ifdef PERSOPORT
	case WM_DROPFILES: {
        	OnDropFiles(hwnd, (HDROP) wParam);
        	}
         	break;
/*
	case WM_LBUTTONDBLCLK: {
		MessageBox( hwnd, "ICI","IXI",MB_OK);
		URLclick( hwnd ) ;
		}
		break;
*/
	case MYWM_NOTIFYICON :
		switch (lParam)	{
			case WM_LBUTTONDBLCLK : 
				VisibleFlag = VISIBLE_YES ;
				//ShowWindow(hwnd, SW_SHOWNORMAL);
				ShowWindow(hwnd, SW_RESTORE);
				SetForegroundWindow( hwnd ) ;
				int ResShell;
				ResShell = Shell_NotifyIcon(NIM_DELETE, &TrayIcone);
				if( ResShell ) return 1 ;
				else return 0 ;
			break ;
			case WM_RBUTTONUP:
			case WM_LBUTTONUP: 
				DisplaySystemTrayMenu( hwnd ) ;
			break ;
		}
	break ;
	
	case WM_NCLBUTTONDOWN:
		if( GetKeyState( VK_CONTROL ) & 0x8000 ) 
			if( wParam == HTCAPTION )
				ManageWinrol( hwnd ) ;
	break;
#endif

#define X_POS(l) ((int)(short)LOWORD(l))
#define Y_POS(l) ((int)(short)HIWORD(l))

#define TO_CHR_X(x) ((((x)<0 ? (x)-font_width+1 : (x))-offset_width) / font_width)
#define TO_CHR_Y(y) ((((y)<0 ? (y)-font_height+1: (y))-offset_height) / font_height)
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_MBUTTONUP:
      case WM_RBUTTONUP:
#ifdef PERSOPORT
        if(!PuttyFlag) {
	if((message == WM_LBUTTONUP) && ((wParam & MK_SHIFT)&&(wParam & MK_CONTROL) ) ) { // shift + CTRL + bouton gauche => duplicate session
		if( back ) SendMessage( hwnd, WM_COMMAND, IDM_DUPSESS, 0 ) ;
		else SendMessage( hwnd, WM_COMMAND, IDM_RESTART, 0 ) ;
		break ;
		}

	else if (message == WM_LBUTTONUP && ((wParam & MK_CONTROL) ) ) {// ctrl+bouton gauche => nouvelle icone
		SetNewIcon( hwnd, cfg, SI_NEXT ) ;
		RefreshBackground( hwnd ) ;
		//break ;
		}

	else if (message == WM_MBUTTONUP && ((wParam & MK_CONTROL) ) ) { // ctrl+bouton milieu => send to tray
		SendMessage( hwnd, WM_COMMAND, IDM_TOTRAY, 0 ) ;
		break ;
		}

	else if ( PasteCommandFlag && (message == WM_RBUTTONDOWN) && ((wParam & MK_SHIFT) ) ) {// shift+bouton droit => coller (paste) am�lior� (pour serveur lent, on utilise la methode autocommand par TIMER)
		SetPasteCommand() ;
		break ;
		}
	}
#endif

	if (message == WM_RBUTTONDOWN &&
	    ((wParam & MK_CONTROL) || (cfg.mouse_is_xterm == 2))) {
	    POINT cursorpos;

	    show_mouseptr(1);	       /* make sure pointer is visible */
	    GetCursorPos(&cursorpos);
	    TrackPopupMenu(popup_menus[CTXMENU].menu,
			   TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
			   cursorpos.x, cursorpos.y,
			   0, hwnd, NULL);
	    break;
	}
	{
	    int button, press;

	    switch (message) {
	      case WM_LBUTTONDOWN:
		button = MBT_LEFT;
		wParam |= MK_LBUTTON;
		press = 1;
		break;
	      case WM_MBUTTONDOWN:
		button = MBT_MIDDLE;
		wParam |= MK_MBUTTON;
		press = 1;
		break;
	      case WM_RBUTTONDOWN:
#ifdef PERSOPORT
		if( !ProtectFlag ) { button = MBT_RIGHT ; wParam |= MK_RBUTTON ; press = 1 ; }
#else
		button = MBT_RIGHT;
		wParam |= MK_RBUTTON;
		press = 1;
#endif
		break;
	      case WM_LBUTTONUP:
		button = MBT_LEFT;
		wParam &= ~MK_LBUTTON;
		press = 0;
		break;
	      case WM_MBUTTONUP:
		button = MBT_MIDDLE;
		wParam &= ~MK_MBUTTON;
		press = 0;
		break;
	      case WM_RBUTTONUP:
#ifdef PERSOPORT
		if( !ProtectFlag ) { button = MBT_RIGHT ; wParam &= ~MK_RBUTTON ; press = 0 ; }
#else
		button = MBT_RIGHT;
		wParam &= ~MK_RBUTTON;
		press = 0;
#endif
		break;
	      default:
		button = press = 0;    /* shouldn't happen */
	    }
	    show_mouseptr(1);
	    /*
	     * Special case: in full-screen mode, if the left
	     * button is clicked in the very top left corner of the
	     * window, we put up the System menu instead of doing
	     * selection.
	     */
	    {
		char mouse_on_hotspot = 0;
		POINT pt;

		GetCursorPos(&pt);
#ifndef NO_MULTIMON
		{
		    HMONITOR mon;
		    MONITORINFO mi;

		    mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);

		    if (mon != NULL) {
			mi.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(mon, &mi);

			if (mi.rcMonitor.left == pt.x &&
			    mi.rcMonitor.top == pt.y) {
			    mouse_on_hotspot = 1;
			}
		    }
		}
#else
		if (pt.x == 0 && pt.y == 0) {
		    mouse_on_hotspot = 1;
		}
#endif
		if (is_full_screen() && press &&
		    button == MBT_LEFT && mouse_on_hotspot) {
		    SendMessage(hwnd, WM_SYSCOMMAND, SC_MOUSEMENU,
				MAKELPARAM(pt.x, pt.y));
		    return 0;
		}
	    }

	    if (press) {
		click(button,
		      TO_CHR_X(X_POS(lParam)), TO_CHR_Y(Y_POS(lParam)),
		      wParam & MK_SHIFT, wParam & MK_CONTROL,
		      is_alt_pressed());
		SetCapture(hwnd);
	    } else {
		term_mouse(term, button, translate_button(button), MA_RELEASE,
			   TO_CHR_X(X_POS(lParam)),
			   TO_CHR_Y(Y_POS(lParam)), wParam & MK_SHIFT,
			   wParam & MK_CONTROL, is_alt_pressed());
		if (!(wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)))
		    ReleaseCapture();
	    }
	}
	return 0;
      case WM_MOUSEMOVE:
	{
	    /*
	     * Windows seems to like to occasionally send MOUSEMOVE
	     * events even if the mouse hasn't moved. Don't unhide
	     * the mouse pointer in this case.
	     */
	    static WPARAM wp = 0;
	    static LPARAM lp = 0;
	    if (wParam != wp || lParam != lp ||
		last_mousemove != WM_MOUSEMOVE) {
		show_mouseptr(1);
		wp = wParam; lp = lParam;
		last_mousemove = WM_MOUSEMOVE;
	    }
	}
	/*
	 * Add the mouse position and message time to the random
	 * number noise.
	 */
	noise_ultralight(lParam);
#ifdef HYPERLINKPORT
	/*
	 * HACK: PuttyTray / Nutty
	 * Hyperlink stuff: Change cursor type if hovering over link
	 */ 
	if( !PuttyFlag )
	if (urlhack_mouse_old_x != TO_CHR_X(X_POS(lParam)) || urlhack_mouse_old_y != TO_CHR_Y(Y_POS(lParam))) {
		urlhack_mouse_old_x = TO_CHR_X(X_POS(lParam));
		urlhack_mouse_old_y = TO_CHR_Y(Y_POS(lParam));

		if ((!term->cfg.url_ctrl_click || urlhack_is_ctrl_pressed()) &&
			urlhack_is_in_link_region(urlhack_mouse_old_x, urlhack_mouse_old_y)) {
				if (urlhack_cursor_is_hand == 0) {
					SetClassLong(hwnd, GCL_HCURSOR, (LONG)LoadCursor(NULL, IDC_HAND));
					urlhack_cursor_is_hand = 1;
					term_update(term); // Force the terminal to update, otherwise the underline will not show (bug somewhere, this is an ugly fix)
				}
		}
		else if (urlhack_cursor_is_hand == 1) {
			SetClassLong(hwnd, GCL_HCURSOR, (LONG)LoadCursor(NULL, IDC_IBEAM));
			urlhack_cursor_is_hand = 0;
			term_update(term); // Force the terminal to update, see above
		}

		// If mouse jumps from one link directly into another, we need a forced terminal update too
		if (urlhack_is_in_link_region(urlhack_mouse_old_x, urlhack_mouse_old_y) != urlhack_current_region) {
			urlhack_current_region = urlhack_is_in_link_region(urlhack_mouse_old_x, urlhack_mouse_old_y);
			term_update(term);
		}

	}
	/* HACK: PuttyTray / Nutty : END */
#endif

	if (wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON) &&
	    GetCapture() == hwnd) {
	    Mouse_Button b;
	    if (wParam & MK_LBUTTON)
		b = MBT_LEFT;
	    else if (wParam & MK_MBUTTON)
		b = MBT_MIDDLE;
	    else
		b = MBT_RIGHT;
	    term_mouse(term, b, translate_button(b), MA_DRAG,
		       TO_CHR_X(X_POS(lParam)),
		       TO_CHR_Y(Y_POS(lParam)), wParam & MK_SHIFT,
		       wParam & MK_CONTROL, is_alt_pressed());
	}
	return 0;
      case WM_NCMOUSEMOVE:
	{
	    static WPARAM wp = 0;
	    static LPARAM lp = 0;
	    if (wParam != wp || lParam != lp ||
		last_mousemove != WM_NCMOUSEMOVE) {
		show_mouseptr(1);
		wp = wParam; lp = lParam;
		last_mousemove = WM_NCMOUSEMOVE;
	    }
	}
	noise_ultralight(lParam);
	break;
      case WM_IGNORE_CLIP:
	ignore_clip = wParam;	       /* don't panic on DESTROYCLIPBOARD */
	break;
      case WM_DESTROYCLIPBOARD:
	if (!ignore_clip)
	    term_deselect(term);
	ignore_clip = FALSE;
	return 0;
      case WM_PAINT:
	{
	    PAINTSTRUCT p;

#if (defined IMAGEPORT) && (!defined FDJ)
        HDC hdccod=0, hdcScreen, hdcBack = 0;
	if( BackgroundImageFlag && (!PuttyFlag) ) {
        HideCaret(hwnd);
        hdcScreen = BeginPaint(hwnd, &p);

        // We'll draw into a temporary buffer then copy to the screen.  After
        // this point, the rest of this routine is written to use hdc and not
        // care whether hdc is a screen or back buffer, until the very end,
        // where the back buffer, if it exists, is blitted to the screen.  That
        // keeps the routine flexible for use with different drawing policies,
        // though right now the only policy we ever use is the one implied by
        // the next line, where we always use a back buffer.
        //hdc = hdcBack = CreateCompatibleDC(hdcScreen);
         hdccod = hdcScreen;

        if(pal) 
        {
            SelectPalette(hdcScreen, pal, TRUE);
            RealizePalette(hdcScreen);
        }
	}
	else {
		HideCaret(hwnd);
		hdc = BeginPaint(hwnd, &p);
		if (pal) {
			SelectPalette(hdc, pal, TRUE);
			RealizePalette(hdc);
		}
	}
#else
	    HideCaret(hwnd);
	    hdc = BeginPaint(hwnd, &p);
	    if (pal) {
		SelectPalette(hdc, pal, TRUE);
		RealizePalette(hdc);
	    }
#endif
	
	    /*
	     * We have to be careful about term_paint(). It will
	     * set a bunch of character cells to INVALID and then
	     * call do_paint(), which will redraw those cells and
	     * _then mark them as done_. This may not be accurate:
	     * when painting in WM_PAINT context we are restricted
	     * to the rectangle which has just been exposed - so if
	     * that only covers _part_ of a character cell and the
	     * rest of it was already visible, that remainder will
	     * not be redrawn at all. Accordingly, we must not
	     * paint any character cell in a WM_PAINT context which
	     * already has a pending update due to terminal output.
	     * The simplest solution to this - and many, many
	     * thanks to Hung-Te Lin for working all this out - is
	     * not to do any actual painting at _all_ if there's a
	     * pending terminal update: just mark the relevant
	     * character cells as INVALID and wait for the
	     * scheduled full update to sort it out.
	     * 
	     * I have a suspicion this isn't the _right_ solution.
	     * An alternative approach would be to have terminal.c
	     * separately track what _should_ be on the terminal
	     * screen and what _is_ on the terminal screen, and
	     * have two completely different types of redraw (one
	     * for full updates, which syncs the former with the
	     * terminal itself, and one for WM_PAINT which syncs
	     * the latter with the former); yet another possibility
	     * would be to have the Windows front end do what the
	     * GTK one already does, and maintain a bitmap of the
	     * current terminal appearance so that WM_PAINT becomes
	     * completely trivial. However, this should do for now.
	     */
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && (!PuttyFlag) ) {
        term_paint(term, hdccod,
            (p.rcPaint.left-offset_width)/font_width,
            (p.rcPaint.top-offset_height)/font_height,
            (p.rcPaint.right-offset_width-1)/font_width,
            (p.rcPaint.bottom-offset_height-1)/font_height,
            !term->window_update_pending);
	
        SelectObject(hdccod, GetStockObject(SYSTEM_FONT));
        SelectObject(hdccod, GetStockObject(WHITE_PEN));

        if(hdcBack)
        {
            // Blit the back buffer to the real DC.
            BitBlt(
                hdcScreen,
                p.rcPaint.left - offset_width,
                p.rcPaint.top - offset_height,
                p.rcPaint.right - p.rcPaint.left + offset_width,
                p.rcPaint.bottom - p.rcPaint.top + offset_height,
                hdcBack, p.rcPaint.left, p.rcPaint.top, SRCCOPY
            );
            
            DeleteDC(hdcBack);
            hdccod = hdcScreen;
        }
        
        // Last paint edges
        paint_term_edges(hdccod, p.rcPaint.left, p.rcPaint.top, p.rcPaint.right, p.rcPaint.bottom);
	
        EndPaint(hwnd, &p);
        ShowCaret(hwnd);
	}
	else {
#endif
	    term_paint(term, hdc, 
		       (p.rcPaint.left-offset_width)/font_width,
		       (p.rcPaint.top-offset_height)/font_height,
		       (p.rcPaint.right-offset_width-1)/font_width,
		       (p.rcPaint.bottom-offset_height-1)/font_height,
		       !term->window_update_pending);

	    if (p.fErase ||
	        p.rcPaint.left  < offset_width  ||
		p.rcPaint.top   < offset_height ||
		p.rcPaint.right >= offset_width + font_width*term->cols ||
		p.rcPaint.bottom>= offset_height + font_height*term->rows)
	    {
		HBRUSH fillcolour, oldbrush;
		HPEN   edge, oldpen;
		fillcolour = CreateSolidBrush (
				    colours[ATTR_DEFBG>>ATTR_BGSHIFT]);
		oldbrush = SelectObject(hdc, fillcolour);
		edge = CreatePen(PS_SOLID, 0, 
				    colours[ATTR_DEFBG>>ATTR_BGSHIFT]);
		oldpen = SelectObject(hdc, edge);

		/*
		 * Jordan Russell reports that this apparently
		 * ineffectual IntersectClipRect() call masks a
		 * Windows NT/2K bug causing strange display
		 * problems when the PuTTY window is taller than
		 * the primary monitor. It seems harmless enough...
		 */
		IntersectClipRect(hdc,
			p.rcPaint.left, p.rcPaint.top,
			p.rcPaint.right, p.rcPaint.bottom);

		ExcludeClipRect(hdc, 
			offset_width, offset_height,
			offset_width+font_width*term->cols,
			offset_height+font_height*term->rows);

		Rectangle(hdc, p.rcPaint.left, p.rcPaint.top, 
			  p.rcPaint.right, p.rcPaint.bottom);

		/* SelectClipRgn(hdc, NULL); */

		SelectObject(hdc, oldbrush);
		DeleteObject(fillcolour);
		SelectObject(hdc, oldpen);
		DeleteObject(edge);
	    }
	    SelectObject(hdc, GetStockObject(SYSTEM_FONT));
	    SelectObject(hdc, GetStockObject(WHITE_PEN));
	    EndPaint(hwnd, &p);
	    ShowCaret(hwnd);
#if (defined IMAGEPORT) && (!defined FDJ)
    }
#endif

	}
	return 0;
      case WM_NETEVENT:
	/* Notice we can get multiple netevents, FD_READ, FD_WRITE etc
	 * but the only one that's likely to try to overload us is FD_READ.
	 * This means buffering just one is fine.
	 */
	if (pending_netevent)
	    enact_pending_netevent();

	pending_netevent = TRUE;
	pend_netevent_wParam = wParam;
	pend_netevent_lParam = lParam;
	if (WSAGETSELECTEVENT(lParam) != FD_READ)
	    enact_pending_netevent();

	net_pending_errors();
	return 0;
      case WM_SETFOCUS:
#ifdef PERSOPORT
        if( TransparencyNumber >= 0 ) { SetTransparency( hwnd, TransparencyNumber ) ; }
	RefreshBackground( hwnd ) ;
#endif

	term_set_focus(term, TRUE);
	CreateCaret(hwnd, caretbm, font_width, font_height);
	ShowCaret(hwnd);
	flash_window(0);	       /* stop */
	compose_state = 0;
	term_update(term);
	break;
      case WM_KILLFOCUS:
	show_mouseptr(1);
	term_set_focus(term, FALSE);
	DestroyCaret();
	caret_x = caret_y = -1;	       /* ensure caret is replaced next time */
	term_update(term);
	break;
      case WM_ENTERSIZEMOVE:
#ifdef RDB_DEBUG_PATCH
	debug((27, "WM_ENTERSIZEMOVE"));
#endif
	EnableSizeTip(1);
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && (!PuttyFlag) ) GetClientRect(hwnd, &size_before);
#endif

	resizing = TRUE;
	need_backend_resize = FALSE;
	break;
      case WM_EXITSIZEMOVE:
	EnableSizeTip(0);
	resizing = FALSE;
#ifdef RDB_DEBUG_PATCH
	debug((27, "WM_EXITSIZEMOVE"));
#endif
	if (need_backend_resize) {
	    term_size(term, cfg.height, cfg.width, cfg.savelines);
	    InvalidateRect(hwnd, NULL, TRUE);
	}
	break;
      case WM_SIZING:
	/*
	 * This does two jobs:
	 * 1) Keep the sizetip uptodate
	 * 2) Make sure the window size is _stepped_ in units of the font size.
	 */
        if (cfg.resize_action == RESIZE_TERM ||
            (cfg.resize_action == RESIZE_EITHER && !is_alt_pressed())) {
	    int width, height, w, h, ew, eh;
	    LPRECT r = (LPRECT) lParam;

	    if ( !need_backend_resize && cfg.resize_action == RESIZE_EITHER &&
		    (cfg.height != term->rows || cfg.width != term->cols )) {
		/* 
		 * Great! It seems that both the terminal size and the
		 * font size have been changed and the user is now dragging.
		 * 
		 * It will now be difficult to get back to the configured
		 * font size!
		 *
		 * This would be easier but it seems to be too confusing.

		term_size(term, cfg.height, cfg.width, cfg.savelines);
		reset_window(2);
		 */
	        cfg.height=term->rows; cfg.width=term->cols;

		InvalidateRect(hwnd, NULL, TRUE);
		need_backend_resize = TRUE;
	    }

	    width = r->right - r->left - extra_width;
	    height = r->bottom - r->top - extra_height;
	    w = (width + font_width / 2) / font_width;
	    if (w < 1)
		w = 1;
	    h = (height + font_height / 2) / font_height;
	    if (h < 1)
		h = 1;
	    UpdateSizeTip(hwnd, w, h);
	    ew = width - w * font_width;
	    eh = height - h * font_height;
	    if (ew != 0) {
		if (wParam == WMSZ_LEFT ||
		    wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT)
		    r->left += ew;
		else
		    r->right -= ew;
	    }
	    if (eh != 0) {
		if (wParam == WMSZ_TOP ||
		    wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOPLEFT)
		    r->top += eh;
		else
		    r->bottom -= eh;
	    }
	    if (ew || eh)
		return 1;
	    else
		return 0;
	} else {
	    int width, height, w, h, rv = 0;
	    int ex_width = extra_width + (cfg.window_border - offset_width) * 2;
	    int ex_height = extra_height + (cfg.window_border - offset_height) * 2;
	    LPRECT r = (LPRECT) lParam;

	    width = r->right - r->left - ex_width;
	    height = r->bottom - r->top - ex_height;

	    w = (width + term->cols/2)/term->cols;
	    h = (height + term->rows/2)/term->rows;
	    if ( r->right != r->left + w*term->cols + ex_width)
		rv = 1;

	    if (wParam == WMSZ_LEFT ||
		wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT)
		r->left = r->right - w*term->cols - ex_width;
	    else
		r->right = r->left + w*term->cols + ex_width;

	    if (r->bottom != r->top + h*term->rows + ex_height)
		rv = 1;

	    if (wParam == WMSZ_TOP ||
		wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOPLEFT)
		r->top = r->bottom - h*term->rows - ex_height;
	    else
		r->bottom = r->top + h*term->rows + ex_height;

	    return rv;
	}
	/* break;  (never reached) */
      case WM_FULLSCR_ON_MAX:
	fullscr_on_max = TRUE;
	break;
#ifdef PERSOPORT
#if (defined IMAGEPORT) && (!defined FDJ)
	case WM_DISPLAYCHANGE:
		if( (!BackgroundImageFlag) || PuttyFlag ) return DefWindowProc(hwnd, message, wParam, lParam) ;
	case WM_MOVE:
	      if( (!BackgroundImageFlag) || PuttyFlag ) {
		sys_cursor_update();
		if( cfg.saveonexit ) GetWindowCoord( hwnd ) ;
		break;
		}
    if(backgrounddc)
    {
        // When using a background image based on the desktop, correct display
        // may depend on the current position of the window.
        InvalidateRect(hwnd, NULL, TRUE);
    }
	sys_cursor_update();
	
	if( cfg.saveonexit ) GetWindowCoord( hwnd ) ;

	break;


	case WM_SETTINGCHANGE:
    // It's sometimes hard to tell what setting changed, but our decisions
    // regarding background drawing depends on some system settings, so force
    // it to be redone.

    if(textdc)
    {
        DeleteObject(textbm);
        DeleteDC(textdc);
        textdc = NULL;
        textbm = NULL;
    }

    if(backgrounddc)
    {
        DeleteObject(backgroundbm);
        DeleteDC(backgrounddc);
        backgrounddc = NULL;
        backgroundbm = NULL;
    }

    if(backgroundblenddc)
    {
        DeleteObject(backgroundblendbm);
        DeleteDC(backgroundblenddc);
        backgroundblenddc = NULL;
        backgroundblendbm = NULL;
    }

    if(textdc || backgrounddc || backgroundblenddc)
    {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    break;
#else
      case WM_MOVE:
	sys_cursor_update();
	if( cfg.saveonexit ) GetWindowCoord( hwnd ) ;
	break;
#endif
#else
      case WM_MOVE:
	sys_cursor_update();
	break;
#endif
      case WM_SIZE:
#ifdef RDB_DEBUG_PATCH
	debug((27, "WM_SIZE %s (%d,%d)",
		(wParam == SIZE_MINIMIZED) ? "SIZE_MINIMIZED":
		(wParam == SIZE_MAXIMIZED) ? "SIZE_MAXIMIZED":
		(wParam == SIZE_RESTORED && resizing) ? "to":
		(wParam == SIZE_RESTORED) ? "SIZE_RESTORED":
		"...",
	    LOWORD(lParam), HIWORD(lParam)));
#endif

#ifdef PERSOPORT
	if (wParam == SIZE_MINIMIZED) {
		if( GetKeyState( VK_CONTROL ) & 0x8000 ) {
			SendMessage( hwnd, WM_COMMAND, IDM_TOTRAY, 0 ) ; return 0 ;
			}
		else 
			SetWindowText(hwnd, cfg.win_name_always ? window_name : icon_name);
		}
#else
	if (wParam == SIZE_MINIMIZED)
	    SetWindowText(hwnd,
			  cfg.win_name_always ? window_name : icon_name);
#endif

	if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
	    SetWindowText(hwnd, window_name);
        if (wParam == SIZE_RESTORED) {
            processed_resize = FALSE;
            clear_full_screen();
            if (processed_resize) {
                /*
                 * Inhibit normal processing of this WM_SIZE; a
                 * secondary one was triggered just now by
                 * clear_full_screen which contained the correct
                 * client area size.
                 */
                return 0;
            }
        }
        if (wParam == SIZE_MAXIMIZED && fullscr_on_max) {
            fullscr_on_max = FALSE;
            processed_resize = FALSE;
            make_full_screen();
            if (processed_resize) {
                /*
                 * Inhibit normal processing of this WM_SIZE; a
                 * secondary one was triggered just now by
                 * make_full_screen which contained the correct client
                 * area size.
                 */
                return 0;
            }
        }

        processed_resize = TRUE;

	if (cfg.resize_action == RESIZE_DISABLED) {
	    /* A resize, well it better be a minimize. */
	    reset_window(-1);
	} else {

	    int width, height, w, h;

	    width = LOWORD(lParam);
	    height = HIWORD(lParam);

            if (wParam == SIZE_MAXIMIZED && !was_zoomed) {
                was_zoomed = 1;
                prev_rows = term->rows;
                prev_cols = term->cols;
                if (cfg.resize_action == RESIZE_TERM) {
                    w = width / font_width;
                    if (w < 1) w = 1;
                    h = height / font_height;
                    if (h < 1) h = 1;

                    term_size(term, h, w, cfg.savelines);
                }
                reset_window(0);
            } else if (wParam == SIZE_RESTORED && was_zoomed) {
                was_zoomed = 0;
                if (cfg.resize_action == RESIZE_TERM) {
                    w = (width-cfg.window_border*2) / font_width;
                    if (w < 1) w = 1;
                    h = (height-cfg.window_border*2) / font_height;
                    if (h < 1) h = 1;
                    term_size(term, h, w, cfg.savelines);
                    reset_window(2);
                } else if (cfg.resize_action != RESIZE_FONT)
                    reset_window(2);
                else
                    reset_window(0);
            } else if (wParam == SIZE_MINIMIZED) {
                /* do nothing */
	    } else if (cfg.resize_action == RESIZE_TERM ||
                       (cfg.resize_action == RESIZE_EITHER &&
                        !is_alt_pressed())) {
                w = (width-cfg.window_border*2) / font_width;
                if (w < 1) w = 1;
                h = (height-cfg.window_border*2) / font_height;
                if (h < 1) h = 1;

                if (resizing) {
#ifdef PERSOPORT
		WinHeight = -1 ;
#endif
                    /*
                     * Don't call back->size in mid-resize. (To
                     * prevent massive numbers of resize events
                     * getting sent down the connection during an NT
                     * opaque drag.)
                     */
		    need_backend_resize = TRUE;
		    cfg.height = h;
		    cfg.width = w;
                } else {
                    term_size(term, h, w, cfg.savelines);
                }
            } else {
                reset_window(0);
	    }
	}
#ifdef PERSOPORT
	if( TitleBarFlag ) set_title( NULL, cfg.wintitle ) ;
	if( cfg.saveonexit ) GetWindowCoord( hwnd ) ;
#endif
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && (cfg.bg_image_abs_fixed==1) && (cfg.bg_type!=0) ) RefreshBackground( hwnd ) ;
#endif

	sys_cursor_update();
	return 0;
      case WM_VSCROLL:
	switch (LOWORD(wParam)) {
	  case SB_BOTTOM:
	    term_scroll(term, -1, 0);
	    break;
	  case SB_TOP:
	    term_scroll(term, +1, 0);
	    break;
	  case SB_LINEDOWN:
	    term_scroll(term, 0, +1);
	    break;
	  case SB_LINEUP:
	    term_scroll(term, 0, -1);
	    break;
	  case SB_PAGEDOWN:
	    term_scroll(term, 0, +term->rows / 2);
	    break;
	  case SB_PAGEUP:
	    term_scroll(term, 0, -term->rows / 2);
	    break;
	  case SB_THUMBPOSITION:
	  case SB_THUMBTRACK:
	    term_scroll(term, 1, HIWORD(wParam));
	    break;
	}
	break;
      case WM_PALETTECHANGED:
	if ((HWND) wParam != hwnd && pal != NULL) {
	    HDC hdc = get_ctx(NULL);
	    if (hdc) {
		if (RealizePalette(hdc) > 0)
		    UpdateColors(hdc);
		free_ctx(hdc);
	    }
	}
	break;
      case WM_QUERYNEWPALETTE:
	if (pal != NULL) {
	    HDC hdc = get_ctx(NULL);
	    if (hdc) {
		if (RealizePalette(hdc) > 0)
		    UpdateColors(hdc);
		free_ctx(hdc);
		return TRUE;
	    }
	}
	return FALSE;
#ifdef HYPERLINKPORT
	/*
	 * HACK: PuttyTray / Nutty
	 * Hyperlink stuff: Change cursor if we are in ctrl+click link mode
	 *
	 * WARNING: Spans over multiple CASEs
	 */
	case WM_KEYDOWN:
		if( PuttyFlag ) goto KEY_END;
		if(wParam == VK_CONTROL && term->cfg.url_ctrl_click) {
			GetCursorPos(&cursor_pt);
			ScreenToClient(hwnd, &cursor_pt);

			if (urlhack_is_in_link_region(TO_CHR_X(cursor_pt.x), TO_CHR_Y(cursor_pt.y))) {
				SetCursor(LoadCursor(NULL, IDC_HAND));
				term_update(term);
			}
		
			goto KEY_END;
		}	

	case WM_KEYUP:
		if( PuttyFlag ) goto KEY_END;
		if (wParam == VK_CONTROL && term->cfg.url_ctrl_click) {
			SetCursor(LoadCursor(NULL, IDC_IBEAM));
			term_update(term);
		
			goto KEY_END;
		}
	KEY_END:

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	/* HACK: PuttyTray / Nutty : END */
#else
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP:
#endif
#ifdef PERSOPORT
		AntiIdleCount = 0 ;
      
		/* Permet de sauvegarder tous les caracteres tapes au clavier pour les avoir dans le /savedump */
		if( debug_flag ) addkeypressed( message, wParam, lParam, GetKeyState(VK_SHIFT)&0x8000, GetKeyState(VK_CONTROL)&0x8000, (GetKeyState(VK_MENU)&0x8000)||(GetKeyState(VK_LMENU)&0x8000),GetKeyState(VK_RMENU)&0x8000, (GetKeyState(VK_RWIN)&0x8000)||(GetKeyState(VK_LWIN)&0x8000 ) );
		
		if((wParam==VK_TAB)&&(message==WM_KEYDOWN)&&(GetKeyState(VK_CONTROL)&0x8000)&&(GetKeyState(VK_SHIFT)&0x8000)) 
			{ ShortcutsFlag=abs(ShortcutsFlag-1) ; return 0 ; }
      
		if( ShortcutsFlag ) { if ( (message==WM_KEYDOWN)||(message==WM_SYSKEYDOWN) ) {
		
		if( ManageShortcuts( hwnd, wParam
			, GetKeyState(VK_SHIFT)&0x8000
			, GetKeyState(VK_CONTROL)&0x8000
			, (GetKeyState(VK_MENU)&0x8000)||(GetKeyState(VK_LMENU)&0x8000)
			, GetKeyState(VK_RMENU)&0x8000
			//, is_alt_pressed()
			, (GetKeyState(VK_RWIN)&0x8000)||(GetKeyState(VK_LWIN)&0x8000)
			) )  
			return 0 ;
		} } // fin if( ShortcutsFlag )
		else { if( ProtectFlag == 1 ) return 0 ; }

		// Majuscule uniquement
		if( CapsLockFlag ) {
			if( ( wParam>='A' ) && ( wParam <= 'Z' ) 
					    && !(GetKeyState( VK_CAPITAL ) & 0x0001) 
					    && !(GetKeyState( VK_SHIFT ) & 0x8000) ) { 
				keybd_event( VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0 ) ;
				keybd_event( VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0) ;
				SendMessage(hwnd, WM_CHAR, wParam, 0) ;
				return 0 ;
				}
			}
#endif

	/*
	 * Add the scan code and keypress timing to the random
	 * number noise.
	 */
	noise_ultralight(lParam);

	/*
	 * We don't do TranslateMessage since it disassociates the
	 * resulting CHAR message from the KEYDOWN that sparked it,
	 * which we occasionally don't want. Instead, we process
	 * KEYDOWN, and call the Win32 translator functions so that
	 * we get the translations under _our_ control.
	 */
	{
	    unsigned char buf[20];
	    int len;

	    if (wParam == VK_PROCESSKEY) { /* IME PROCESS key */
		if (message == WM_KEYDOWN) {
		    MSG m;
		    m.hwnd = hwnd;
		    m.message = WM_KEYDOWN;
		    m.wParam = wParam;
		    m.lParam = lParam & 0xdfff;
		    TranslateMessage(&m);
		} else break; /* pass to Windows for default processing */
	    } else {
		len = TranslateKey(message, wParam, lParam, buf);
		if (len == -1)
		    return DefWindowProc(hwnd, message, wParam, lParam);

		if (len != 0) {
		    /*
		     * Interrupt an ongoing paste. I'm not sure
		     * this is sensible, but for the moment it's
		     * preferable to having to faff about buffering
		     * things.
		     */
		    term_nopaste(term);

		    /*
		     * We need not bother about stdin backlogs
		     * here, because in GUI PuTTY we can't do
		     * anything about it anyway; there's no means
		     * of asking Windows to hold off on KEYDOWN
		     * messages. We _have_ to buffer everything
		     * we're sent.
		     */
		    term_seen_key_event(term);
		    if (ldisc)
			ldisc_send(ldisc, buf, len, 1);
		    show_mouseptr(0);
		}
	    }
	}
	net_pending_errors();
	return 0;
      case WM_INPUTLANGCHANGE:
	/* wParam == Font number */
	/* lParam == Locale */
	set_input_locale((HKL)lParam);
	sys_cursor_update();
	break;
      case WM_IME_STARTCOMPOSITION:
	{
	    HIMC hImc = ImmGetContext(hwnd);
	    ImmSetCompositionFont(hImc, &lfont);
	    ImmReleaseContext(hwnd, hImc);
	}
	break;
      case WM_IME_COMPOSITION:
	{
	    HIMC hIMC;
	    int n;
	    char *buff;

	    if(osVersion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS || 
	        osVersion.dwPlatformId == VER_PLATFORM_WIN32s) break; /* no Unicode */

	    if ((lParam & GCS_RESULTSTR) == 0) /* Composition unfinished. */
		break; /* fall back to DefWindowProc */

	    hIMC = ImmGetContext(hwnd);
	    n = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);

	    if (n > 0) {
		int i;
		buff = snewn(n, char);
		ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buff, n);
		/*
		 * Jaeyoun Chung reports that Korean character
		 * input doesn't work correctly if we do a single
		 * luni_send() covering the whole of buff. So
		 * instead we luni_send the characters one by one.
		 */
		term_seen_key_event(term);
		for (i = 0; i < n; i += 2) {
		    if (ldisc)
			luni_send(ldisc, (unsigned short *)(buff+i), 1, 1);
		}
		free(buff);
	    }
	    ImmReleaseContext(hwnd, hIMC);
	    return 1;
	}

      case WM_IME_CHAR:
	if (wParam & 0xFF00) {
	    unsigned char buf[2];

	    buf[1] = wParam;
	    buf[0] = wParam >> 8;
	    term_seen_key_event(term);
	    if (ldisc)
		lpage_send(ldisc, kbd_codepage, buf, 2, 1);
	} else {
	    char c = (unsigned char) wParam;
	    term_seen_key_event(term);
	    if (ldisc)
		lpage_send(ldisc, kbd_codepage, &c, 1, 1);
	}
	return (0);
      case WM_CHAR:
      case WM_SYSCHAR:
	/*
	 * Nevertheless, we are prepared to deal with WM_CHAR
	 * messages, should they crop up. So if someone wants to
	 * post the things to us as part of a macro manoeuvre,
	 * we're ready to cope.
	 */
	{
	    char c = (unsigned char)wParam;
	    term_seen_key_event(term);
	    if (ldisc)
		lpage_send(ldisc, CP_ACP, &c, 1, 1);
	}
	return 0;
      case WM_SYSCOLORCHANGE:
	if (cfg.system_colour) {
	    /* Refresh palette from system colours. */
	    /* XXX actually this zaps the entire palette. */
	    systopalette();
	    init_palette();
	    /* Force a repaint of the terminal window. */
	    term_invalidate(term);
	}
	break;
      case WM_AGENT_CALLBACK:
	{
	    struct agent_callback *c = (struct agent_callback *)lParam;
	    c->callback(c->callback_ctx, c->data, c->len);
	    sfree(c);
	}
	return 0;
      case WM_GOT_CLIPDATA:
	if (process_clipdata((HGLOBAL)lParam, wParam))
	    term_do_paste(term);
	return 0;
#ifdef RECONNECTPORT
	if(cfg.wakeup_reconnect) {
		switch(wParam) {
			case PBT_APMRESUMESUSPEND:
			case PBT_APMRESUMEAUTOMATIC:
			case PBT_APMRESUMECRITICAL:
			case PBT_APMQUERYSUSPENDFAILED:
				if(session_closed && !back) {
					time_t tnow = time(NULL);
					
					if(last_reconnect && (tnow - last_reconnect) < 5) {
						Sleep(1000);
					}
					last_reconnect = tnow;
					logevent(NULL, "Woken up from suspend, reconnecting...");
					term_pwron(term, FALSE);
					backend_connected = 0 ;
					start_backend();
					SetTimer(hwnd, TIMER_INIT, (int)(init_delay*1000), NULL) ;
				}
				break;
			case PBT_APMSUSPEND:
				if(!session_closed && back) {
					logevent(NULL, "Suspend detected, disconnecting cleanly...");
					close_session();
				}
				break;
		}
	}
	break;	
#endif

      default:
	if (message == wm_mousewheel || message == WM_MOUSEWHEEL) {
	    int shift_pressed=0, control_pressed=0;

	    if (message == WM_MOUSEWHEEL) {
		wheel_accumulator += (short)HIWORD(wParam);
		shift_pressed=LOWORD(wParam) & MK_SHIFT;
		control_pressed=LOWORD(wParam) & MK_CONTROL;
	    } else {
		BYTE keys[256];
		wheel_accumulator += (int)wParam;
		if (GetKeyboardState(keys)!=0) {
		    shift_pressed=keys[VK_SHIFT]&0x80;
		    control_pressed=keys[VK_CONTROL]&0x80;
		}
	    }

	    /* process events when the threshold is reached */
	    while (abs(wheel_accumulator) >= WHEEL_DELTA) {
		int b;

		/* reduce amount for next time */
		if (wheel_accumulator > 0) {
		    b = MBT_WHEEL_UP;
		    wheel_accumulator -= WHEEL_DELTA;
		} else if (wheel_accumulator < 0) {
		    b = MBT_WHEEL_DOWN;
		    wheel_accumulator += WHEEL_DELTA;
		} else
		    break;

		if (send_raw_mouse &&
		    !(cfg.mouse_override && shift_pressed)) {
		    /* Mouse wheel position is in screen coordinates for
		     * some reason */
		    POINT p;
		    p.x = X_POS(lParam); p.y = Y_POS(lParam);
		    if (ScreenToClient(hwnd, &p)) {
			/* send a mouse-down followed by a mouse up */
			term_mouse(term, b, translate_button(b),
				   MA_CLICK,
				   TO_CHR_X(p.x),
				   TO_CHR_Y(p.y), shift_pressed,
				   control_pressed, is_alt_pressed());
			term_mouse(term, b, translate_button(b),
				   MA_RELEASE, TO_CHR_X(p.x),
				   TO_CHR_Y(p.y), shift_pressed,
				   control_pressed, is_alt_pressed());
		    } /* else: not sure when this can fail */
		} else {
		    /* trigger a scroll */
		    term_scroll(term, 0,
				b == MBT_WHEEL_UP ?
				-term->rows / 2 : term->rows / 2);
		}
	    }
	    return 0;
	}
    }

    /*
     * Any messages we don't process completely above are passed through to
     * DefWindowProc() for default processing.
     */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*
 * Move the system caret. (We maintain one, even though it's
 * invisible, for the benefit of blind people: apparently some
 * helper software tracks the system caret, so we should arrange to
 * have one.)
 */
void sys_cursor(void *frontend, int x, int y)
{
    int cx, cy;

    if (!term->has_focus) return;

    /*
     * Avoid gratuitously re-updating the cursor position and IMM
     * window if there's no actual change required.
     */
    cx = x * font_width + offset_width;
    cy = y * font_height + offset_height;
    if (cx == caret_x && cy == caret_y)
	return;
    caret_x = cx;
    caret_y = cy;

    sys_cursor_update();
}

static void sys_cursor_update(void)
{
    COMPOSITIONFORM cf;
    HIMC hIMC;

    if (!term->has_focus) return;

    if (caret_x < 0 || caret_y < 0)
	return;

    SetCaretPos(caret_x, caret_y);

    /* IMM calls on Win98 and beyond only */
    if(osVersion.dwPlatformId == VER_PLATFORM_WIN32s) return; /* 3.11 */
    
    if(osVersion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
	    osVersion.dwMinorVersion == 0) return; /* 95 */

    /* we should have the IMM functions */
    hIMC = ImmGetContext(hwnd);
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = caret_x;
    cf.ptCurrentPos.y = caret_y;
    ImmSetCompositionWindow(hIMC, &cf);

    ImmReleaseContext(hwnd, hIMC);
}

/*
 * Draw a line of text in the window, at given character
 * coordinates, in given attributes.
 *
 * We are allowed to fiddle with the contents of `text'.
 */
void do_text_internal(Context ctx, int x, int y, wchar_t *text, int len,
		      unsigned long attr, int lattr)
{
    COLORREF fg, bg, t;
    int nfg, nbg, nfont;
    HDC hdc = ctx;
    RECT line_box;
    int force_manual_underline = 0;
    int fnt_width, char_width;
    int text_adjust = 0;
    int xoffset = 0;
    int maxlen, remaining, opaque;
    static int *lpDx = NULL;
    static int lpDx_len = 0;
    int *lpDx_maybe;
#if (defined IMAGEPORT) && (!defined FDJ)
    int transBg = backgrounddc ? 1 : 0;
    UINT etoFlagOpaque = transBg ? 0 : ETO_OPAQUE;
#endif

    lattr &= LATTR_MODE;

    char_width = fnt_width = font_width * (1 + (lattr != LATTR_NORM));

    if (attr & ATTR_WIDE)
	char_width *= 2;

    /* Only want the left half of double width lines */
    if (lattr != LATTR_NORM && x*2 >= term->cols)
	return;

    x *= fnt_width;
    y *= font_height;
    x += offset_width;
    y += offset_height;

    if ((attr & TATTR_ACTCURS) && (cfg.cursor_type == 0 || term->big_cursor)) {
	attr &= ~(ATTR_REVERSE|ATTR_BLINK|ATTR_COLOURS);
	if (bold_mode == BOLD_COLOURS)
	    attr &= ~ATTR_BOLD;

	/* cursor fg and bg */
	attr |= (260 << ATTR_FGSHIFT) | (261 << ATTR_BGSHIFT);
    }

    nfont = 0;
    if (cfg.vtmode == VT_POORMAN && lattr != LATTR_NORM) {
	/* Assume a poorman font is borken in other ways too. */
	lattr = LATTR_WIDE;
    } else
	switch (lattr) {
	  case LATTR_NORM:
	    break;
	  case LATTR_WIDE:
	    nfont |= FONT_WIDE;
	    break;
	  default:
	    nfont |= FONT_WIDE + FONT_HIGH;
	    break;
	}
    if (attr & ATTR_NARROW)
	nfont |= FONT_NARROW;

    /* Special hack for the VT100 linedraw glyphs. */
    if (text[0] >= 0x23BA && text[0] <= 0x23BD) {
	switch ((unsigned char) (text[0])) {
	  case 0xBA:
	    text_adjust = -2 * font_height / 5;
	    break;
	  case 0xBB:
	    text_adjust = -1 * font_height / 5;
	    break;
	  case 0xBC:
	    text_adjust = font_height / 5;
	    break;
	  case 0xBD:
	    text_adjust = 2 * font_height / 5;
	    break;
	}
	if (lattr == LATTR_TOP || lattr == LATTR_BOT)
	    text_adjust *= 2;
	text[0] = ucsdata.unitab_xterm['q'];
	if (attr & ATTR_UNDER) {
	    attr &= ~ATTR_UNDER;
	    force_manual_underline = 1;
	}
    }

    /* Anything left as an original character set is unprintable. */
    if (DIRECT_CHAR(text[0])) {
	int i;
	for (i = 0; i < len; i++)
	    text[i] = 0xFFFD;
    }

    /* OEM CP */
    if ((text[0] & CSET_MASK) == CSET_OEMCP)
	nfont |= FONT_OEM;

    nfg = ((attr & ATTR_FGMASK) >> ATTR_FGSHIFT);
    nbg = ((attr & ATTR_BGMASK) >> ATTR_BGSHIFT);
    if (bold_mode == BOLD_FONT && (attr & ATTR_BOLD))
	nfont |= FONT_BOLD;
    if (und_mode == UND_FONT && (attr & ATTR_UNDER))
	nfont |= FONT_UNDERLINE;
    another_font(nfont);
    if (!fonts[nfont]) {
	if (nfont & FONT_UNDERLINE)
	    force_manual_underline = 1;
	/* Don't do the same for manual bold, it could be bad news. */

	nfont &= ~(FONT_BOLD | FONT_UNDERLINE);
    }
    another_font(nfont);
    if (!fonts[nfont])
	nfont = FONT_NORMAL;
    if (attr & ATTR_REVERSE) {
	t = nfg;
	nfg = nbg;
	nbg = t;
    }
    if (bold_mode == BOLD_COLOURS && (attr & ATTR_BOLD)) {
	if (nfg < 16) nfg |= 8;
	else if (nfg >= 256) nfg |= 1;
    }
    if (bold_mode == BOLD_COLOURS && (attr & ATTR_BLINK)) {
	if (nbg < 16) nbg |= 8;
	else if (nbg >= 256) nbg |= 1;
    }
    fg = colours[nfg];
    bg = colours[nbg];
#if (defined IMAGEPORT) && (!defined FDJ)
    if( BackgroundImageFlag && (!PuttyFlag) ) {
    line_box.left = x;
    line_box.top = y;
    line_box.right = x + char_width * len;
    line_box.bottom = y + font_height;
    
    if(textdc)
    {
        int x = line_box.left;
        int y = line_box.top;
        int width = line_box.right - line_box.left;
        int height = line_box.bottom - line_box.top;

        POINT bgloc = { x, y };
        COLORREF backgroundcolor = colours[258]; // Default Background
        
        if(!bBgRelToTerm)
            ClientToScreen(hwnd, &bgloc);
        
        if(bg == backgroundcolor) 
        {
            // Use fast screen fill for default background.
            BitBlt(textdc, x, y, width, height, backgroundblenddc, bgloc.x, bgloc.y, SRCCOPY);
        }
        else 
        {
            BitBlt(textdc, x, y, width, height, backgrounddc, bgloc.x, bgloc.y, SRCCOPY);
            
            color_blend(textdc, x, y, width, height, bg, cfg.bg_opacity);
        }
        
        hdc = textdc;
    }
    SelectObject(hdc, fonts[nfont]);
    SetTextColor(hdc, fg);
    SetBkColor(hdc, bg);
    if (transBg || attr & TATTR_COMBINING)
	SetBkMode(hdc, TRANSPARENT);
    else
	SetBkMode(hdc, OPAQUE);
    }
    else {
    SelectObject(hdc, fonts[nfont]);
    SetTextColor(hdc, fg);
    SetBkColor(hdc, bg);
    if (attr & TATTR_COMBINING)
	SetBkMode(hdc, TRANSPARENT);
    else
	SetBkMode(hdc, OPAQUE);
    line_box.left = x;
    line_box.top = y;
    line_box.right = x + char_width * len;
    line_box.bottom = y + font_height;
    }
#else
    SelectObject(hdc, fonts[nfont]);
    SetTextColor(hdc, fg);
    SetBkColor(hdc, bg);
    if (attr & TATTR_COMBINING)
	SetBkMode(hdc, TRANSPARENT);
    else
	SetBkMode(hdc, OPAQUE);
    line_box.left = x;
    line_box.top = y;
    line_box.right = x + char_width * len;
    line_box.bottom = y + font_height;
#endif

    /* Only want the left half of double width lines */
    if (line_box.right > font_width*term->cols+offset_width)
	line_box.right = font_width*term->cols+offset_width;

    if (font_varpitch) {
        /*
         * If we're using a variable-pitch font, we unconditionally
         * draw the glyphs one at a time and centre them in their
         * character cells (which means in particular that we must
         * disable the lpDx mechanism). This gives slightly odd but
         * generally reasonable results.
         */
        xoffset = char_width / 2;
        SetTextAlign(hdc, TA_TOP | TA_CENTER | TA_NOUPDATECP);
        lpDx_maybe = NULL;
        maxlen = 1;
    } else {
        /*
         * In a fixed-pitch font, we draw the whole string in one go
         * in the normal way.
         */
        xoffset = 0;
        SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
        lpDx_maybe = lpDx;
        maxlen = len;
    }

    opaque = TRUE;                     /* start by erasing the rectangle */
    for (remaining = len; remaining > 0;
         text += len, remaining -= len, x += char_width * len) {
        len = (maxlen < remaining ? maxlen : remaining);

        if (len > lpDx_len) {
            if (len > lpDx_len) {
                lpDx_len = len * 9 / 8 + 16;
                lpDx = sresize(lpDx, lpDx_len, int);
            }
        }
        {
            int i;
            for (i = 0; i < len; i++)
                lpDx[i] = char_width;
        }

        /* We're using a private area for direct to font. (512 chars.) */
        if (ucsdata.dbcs_screenfont && (text[0] & CSET_MASK) == CSET_ACP) {
            /* Ho Hum, dbcs fonts are a PITA! */
            /* To display on W9x I have to convert to UCS */
            static wchar_t *uni_buf = 0;
            static int uni_len = 0;
            int nlen, mptr;
            if (len > uni_len) {
                sfree(uni_buf);
                uni_len = len;
                uni_buf = snewn(uni_len, wchar_t);
            }

            for(nlen = mptr = 0; mptr<len; mptr++) {
                uni_buf[nlen] = 0xFFFD;
                if (IsDBCSLeadByteEx(ucsdata.font_codepage,
                                     (BYTE) text[mptr])) {
                    char dbcstext[2];
                    dbcstext[0] = text[mptr] & 0xFF;
                    dbcstext[1] = text[mptr+1] & 0xFF;
                    lpDx[nlen] += char_width;
                    MultiByteToWideChar(ucsdata.font_codepage, MB_USEGLYPHCHARS,
                                        dbcstext, 2, uni_buf+nlen, 1);
                    mptr++;
                }
                else
                {
                    char dbcstext[1];
                    dbcstext[0] = text[mptr] & 0xFF;
                    MultiByteToWideChar(ucsdata.font_codepage, MB_USEGLYPHCHARS,
                                        dbcstext, 1, uni_buf+nlen, 1);
                }
                nlen++;
            }
            if (nlen <= 0)
                return;		       /* Eeek! */
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && (!PuttyFlag) )
	// ExtTextOutW(hdc, x,
	//	    y - font_height * (lattr == LATTR_BOT) + text_adjust,
	//	    ETO_CLIPPED | etoFlagOpaque, &line_box, uni_buf, nlen, lpDx);
	ExtTextOutW(hdc, x + xoffset,
		    y - font_height * (lattr == LATTR_BOT) + text_adjust,
		    ETO_CLIPPED | etoFlagOpaque, &line_box, uni_buf, nlen, lpDx_maybe);
	else
#endif
            ExtTextOutW(hdc, x + xoffset,
                        y - font_height * (lattr == LATTR_BOT) + text_adjust,
                        ETO_CLIPPED | (opaque ? ETO_OPAQUE : 0),
                        &line_box, uni_buf, nlen,
                        lpDx_maybe);
    
            if (bold_mode == BOLD_SHADOW && (attr & ATTR_BOLD)) {
                SetBkMode(hdc, TRANSPARENT);
                ExtTextOutW(hdc, x + xoffset - 1,
                            y - font_height * (lattr ==
                                               LATTR_BOT) + text_adjust,
                            ETO_CLIPPED, &line_box, uni_buf, nlen, lpDx_maybe);
            }

            lpDx[0] = -1;
        } else if (DIRECT_FONT(text[0])) {
            static char *directbuf = NULL;
            static int directlen = 0;
            int i;
            if (len > directlen) {
                directlen = len;
                directbuf = sresize(directbuf, directlen, char);
            }

            for (i = 0; i < len; i++)
                directbuf[i] = text[i] & 0xFF;
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && (!PuttyFlag) )
	// ExtTextOut(hdc, x,
	// 	   y - font_height * (lattr == LATTR_BOT) + text_adjust,
	// 	   ETO_CLIPPED | etoFlagOpaque, &line_box, directbuf, len, lpDx);
	ExtTextOut(hdc, x + xoffset,
		   y - font_height * (lattr == LATTR_BOT) + text_adjust,
		   ETO_CLIPPED | etoFlagOpaque, &line_box, directbuf, len, lpDx_maybe);
	else
#endif
            ExtTextOut(hdc, x + xoffset,
                       y - font_height * (lattr == LATTR_BOT) + text_adjust,
                       ETO_CLIPPED | (opaque ? ETO_OPAQUE : 0),
                       &line_box, directbuf, len, lpDx_maybe);
	    
            if (bold_mode == BOLD_SHADOW && (attr & ATTR_BOLD)) {
                SetBkMode(hdc, TRANSPARENT);

                /* GRR: This draws the character outside its box and
                 * can leave 'droppings' even with the clip box! I
                 * suppose I could loop it one character at a time ...
                 * yuk.
                 * 
                 * Or ... I could do a test print with "W", and use +1
                 * or -1 for this shift depending on if the leftmost
                 * column is blank...
                 */
                ExtTextOut(hdc, x + xoffset - 1,
                           y - font_height * (lattr ==
                                              LATTR_BOT) + text_adjust,
                           ETO_CLIPPED, &line_box, directbuf, len, lpDx_maybe);
            }
        } else {
            /* And 'normal' unicode characters */
            static WCHAR *wbuf = NULL;
            static int wlen = 0;
            int i;

            if (wlen < len) {
                sfree(wbuf);
                wlen = len;
                wbuf = snewn(wlen, WCHAR);
            }

            for (i = 0; i < len; i++)
                wbuf[i] = text[i];
#if (defined IMAGEPORT) && (!defined FDJ)
 	/* print Glyphs as they are, without Windows' Shaping*/
	if( BackgroundImageFlag && (!PuttyFlag) )
 	// exact_textout(hdc, x, y - font_height * (lattr == LATTR_BOT) + text_adjust,
	// 	      &line_box, wbuf, len, lpDx, !(attr & TATTR_COMBINING) &&!transBg);
 	exact_textout(hdc, x + xoffset, y - font_height * (lattr == LATTR_BOT) + text_adjust,
		      &line_box, wbuf, len, lpDx, !(attr & TATTR_COMBINING) &&!transBg);
	else
#endif

            /* print Glyphs as they are, without Windows' Shaping*/
            general_textout(hdc, x + xoffset,
                            y - font_height * (lattr==LATTR_BOT) + text_adjust,
                            &line_box, wbuf, len, lpDx,
                            opaque && !(attr & TATTR_COMBINING));

            /* And the shadow bold hack. */
            if (bold_mode == BOLD_SHADOW && (attr & ATTR_BOLD)) {
                SetBkMode(hdc, TRANSPARENT);
                ExtTextOutW(hdc, x + xoffset - 1,
                            y - font_height * (lattr ==
                                               LATTR_BOT) + text_adjust,
                            ETO_CLIPPED, &line_box, wbuf, len, lpDx_maybe);
            }
        }

        /*
         * If we're looping round again, stop erasing the background
         * rectangle.
         */
        SetBkMode(hdc, TRANSPARENT);
        opaque = FALSE;
    }
    if (lattr != LATTR_TOP && (force_manual_underline ||
			       (und_mode == UND_LINE
				&& (attr & ATTR_UNDER)))) {
	HPEN oldpen;
	int dec = descent;
	if (lattr == LATTR_BOT)
	    dec = dec * 2 - font_height;

	oldpen = SelectObject(hdc, CreatePen(PS_SOLID, 0, fg));
	MoveToEx(hdc, line_box.left, line_box.top + dec, NULL);
	LineTo(hdc, line_box.right, line_box.top + dec);
	oldpen = SelectObject(hdc, oldpen);
	DeleteObject(oldpen);
    }

#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && (!PuttyFlag) )
    if(textdc)
    {
        int x = line_box.left;
        int y = line_box.top;
        int width = line_box.right - line_box.left;
        int height = line_box.bottom - line_box.top;

        // Copy the result to the working DC.
        BitBlt(ctx, x, y, width, height, hdc, x, y, SRCCOPY);
    }
#endif
}

/*
 * Wrapper that handles combining characters.
 */
void do_text(Context ctx, int x, int y, wchar_t *text, int len,
	     unsigned long attr, int lattr)
{
    if (attr & TATTR_COMBINING) {
	unsigned long a = 0;
	attr &= ~TATTR_COMBINING;
	while (len--) {
	    do_text_internal(ctx, x, y, text, 1, attr | a, lattr);
	    text++;
	    a = TATTR_COMBINING;
	}
    } else
	do_text_internal(ctx, x, y, text, len, attr, lattr);
}

void do_cursor(Context ctx, int x, int y, wchar_t *text, int len,
	       unsigned long attr, int lattr)
{

    int fnt_width;
    int char_width;
    HDC hdc = ctx;
    int ctype = cfg.cursor_type;

    lattr &= LATTR_MODE;

    if ((attr & TATTR_ACTCURS) && (ctype == 0 || term->big_cursor)) {
	if (*text != UCSWIDE) {
	    do_text(ctx, x, y, text, len, attr, lattr);
	    return;
	}
	ctype = 2;
	attr |= TATTR_RIGHTCURS;
    }

    fnt_width = char_width = font_width * (1 + (lattr != LATTR_NORM));
    if (attr & ATTR_WIDE)
	char_width *= 2;
    x *= fnt_width;
    y *= font_height;
    x += offset_width;
    y += offset_height;

    if ((attr & TATTR_PASCURS) && (ctype == 0 || term->big_cursor)) {
	POINT pts[5];
	HPEN oldpen;
	pts[0].x = pts[1].x = pts[4].x = x;
	pts[2].x = pts[3].x = x + char_width - 1;
	pts[0].y = pts[3].y = pts[4].y = y;
	pts[1].y = pts[2].y = y + font_height - 1;
	oldpen = SelectObject(hdc, CreatePen(PS_SOLID, 0, colours[261]));
	Polyline(hdc, pts, 5);
	oldpen = SelectObject(hdc, oldpen);
	DeleteObject(oldpen);
    } else if ((attr & (TATTR_ACTCURS | TATTR_PASCURS)) && ctype != 0) {
	int startx, starty, dx, dy, length, i;
	if (ctype == 1) {
	    startx = x;
	    starty = y + descent;
	    dx = 1;
	    dy = 0;
	    length = char_width;
	} else {
	    int xadjust = 0;
	    if (attr & TATTR_RIGHTCURS)
		xadjust = char_width - 1;
	    startx = x + xadjust;
	    starty = y;
	    dx = 0;
	    dy = 1;
	    length = font_height;
	}
	if (attr & TATTR_ACTCURS) {
	    HPEN oldpen;
	    oldpen =
		SelectObject(hdc, CreatePen(PS_SOLID, 0, colours[261]));
	    MoveToEx(hdc, startx, starty, NULL);
	    LineTo(hdc, startx + dx * length, starty + dy * length);
	    oldpen = SelectObject(hdc, oldpen);
	    DeleteObject(oldpen);
	} else {
	    for (i = 0; i < length; i++) {
		if (i % 2 == 0) {
		    SetPixel(hdc, startx, starty, colours[261]);
		}
		startx += dx;
		starty += dy;
	    }
	}
    }
}

/* This function gets the actual width of a character in the normal font.
 */
int char_width(Context ctx, int uc) {
    HDC hdc = ctx;
    int ibuf = 0;

    /* If the font max is the same as the font ave width then this
     * function is a no-op.
     */
    if (!font_dualwidth) return 1;

    switch (uc & CSET_MASK) {
      case CSET_ASCII:
	uc = ucsdata.unitab_line[uc & 0xFF];
	break;
      case CSET_LINEDRW:
	uc = ucsdata.unitab_xterm[uc & 0xFF];
	break;
      case CSET_SCOACS:
	uc = ucsdata.unitab_scoacs[uc & 0xFF];
	break;
    }
    if (DIRECT_FONT(uc)) {
	if (ucsdata.dbcs_screenfont) return 1;

	/* Speedup, I know of no font where ascii is the wrong width */
	if ((uc&~CSET_MASK) >= ' ' && (uc&~CSET_MASK)<= '~')
	    return 1;

	if ( (uc & CSET_MASK) == CSET_ACP ) {
	    SelectObject(hdc, fonts[FONT_NORMAL]);
	} else if ( (uc & CSET_MASK) == CSET_OEMCP ) {
	    another_font(FONT_OEM);
	    if (!fonts[FONT_OEM]) return 0;

	    SelectObject(hdc, fonts[FONT_OEM]);
	} else
	    return 0;

	if ( GetCharWidth32(hdc, uc&~CSET_MASK, uc&~CSET_MASK, &ibuf) != 1 &&
	     GetCharWidth(hdc, uc&~CSET_MASK, uc&~CSET_MASK, &ibuf) != 1)
	    return 0;
    } else {
	/* Speedup, I know of no font where ascii is the wrong width */
	if (uc >= ' ' && uc <= '~') return 1;

	SelectObject(hdc, fonts[FONT_NORMAL]);
	if ( GetCharWidth32W(hdc, uc, uc, &ibuf) == 1 )
	    /* Okay that one worked */ ;
	else if ( GetCharWidthW(hdc, uc, uc, &ibuf) == 1 )
	    /* This should work on 9x too, but it's "less accurate" */ ;
	else
	    return 0;
    }

    ibuf += font_width / 2 -1;
    ibuf /= font_width;

    return ibuf;
}

/*
 * Translate a WM_(SYS)?KEY(UP|DOWN) message into a string of ASCII
 * codes. Returns number of bytes used, zero to drop the message,
 * -1 to forward the message to Windows, or another negative number
 * to indicate a NUL-terminated "special" string.
 */
static int TranslateKey(UINT message, WPARAM wParam, LPARAM lParam,
			unsigned char *output)
{
    BYTE keystate[256];
    int scan, left_alt = 0, key_down, shift_state;
    int r, i, code;
    unsigned char *p = output;
    static int alt_sum = 0;

    HKL kbd_layout = GetKeyboardLayout(0);

    /* keys is for ToAsciiEx. There's some ick here, see below. */
    static WORD keys[3];
    static int compose_char = 0;
    static WPARAM compose_key = 0;

    r = GetKeyboardState(keystate);
    if (!r)
	memset(keystate, 0, sizeof(keystate));
    else {
#if 0
#define SHOW_TOASCII_RESULT
	{			       /* Tell us all about key events */
	    static BYTE oldstate[256];
	    static int first = 1;
	    static int scan;
	    int ch;
	    if (first)
		memcpy(oldstate, keystate, sizeof(oldstate));
	    first = 0;

	    if ((HIWORD(lParam) & (KF_UP | KF_REPEAT)) == KF_REPEAT) {
		debug(("+"));
	    } else if ((HIWORD(lParam) & KF_UP)
		       && scan == (HIWORD(lParam) & 0xFF)) {
		debug((". U"));
	    } else {
		debug((".\n"));
		if (wParam >= VK_F1 && wParam <= VK_F20)
		    debug(("K_F%d", wParam + 1 - VK_F1));
		else
		    switch (wParam) {
		      case VK_SHIFT:
			debug(("SHIFT"));
			break;
		      case VK_CONTROL:
			debug(("CTRL"));
			break;
		      case VK_MENU:
			debug(("ALT"));
			break;
		      default:
			debug(("VK_%02x", wParam));
		    }
		if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
		    debug(("*"));
		debug((", S%02x", scan = (HIWORD(lParam) & 0xFF)));

		ch = MapVirtualKeyEx(wParam, 2, kbd_layout);
		if (ch >= ' ' && ch <= '~')
		    debug((", '%c'", ch));
		else if (ch)
		    debug((", $%02x", ch));

		if (keys[0])
		    debug((", KB0=%02x", keys[0]));
		if (keys[1])
		    debug((", KB1=%02x", keys[1]));
		if (keys[2])
		    debug((", KB2=%02x", keys[2]));

		if ((keystate[VK_SHIFT] & 0x80) != 0)
		    debug((", S"));
		if ((keystate[VK_CONTROL] & 0x80) != 0)
		    debug((", C"));
		if ((HIWORD(lParam) & KF_EXTENDED))
		    debug((", E"));
		if ((HIWORD(lParam) & KF_UP))
		    debug((", U"));
	    }

	    if ((HIWORD(lParam) & (KF_UP | KF_REPEAT)) == KF_REPEAT);
	    else if ((HIWORD(lParam) & KF_UP))
		oldstate[wParam & 0xFF] ^= 0x80;
	    else
		oldstate[wParam & 0xFF] ^= 0x81;

	    for (ch = 0; ch < 256; ch++)
		if (oldstate[ch] != keystate[ch])
		    debug((", M%02x=%02x", ch, keystate[ch]));

	    memcpy(oldstate, keystate, sizeof(oldstate));
	}
#endif

	if (wParam == VK_MENU && (HIWORD(lParam) & KF_EXTENDED)) {
	    keystate[VK_RMENU] = keystate[VK_MENU];
	}


	/* Nastyness with NUMLock - Shift-NUMLock is left alone though */
	if ((cfg.funky_type == FUNKY_VT400 ||
	     (cfg.funky_type <= FUNKY_LINUX && term->app_keypad_keys &&
	      !cfg.no_applic_k))
	    && wParam == VK_NUMLOCK && !(keystate[VK_SHIFT] & 0x80)) {

	    wParam = VK_EXECUTE;

	    /* UnToggle NUMLock */
	    if ((HIWORD(lParam) & (KF_UP | KF_REPEAT)) == 0)
		keystate[VK_NUMLOCK] ^= 1;
	}

	/* And write back the 'adjusted' state */
	SetKeyboardState(keystate);
    }

    /* Disable Auto repeat if required */
    if (term->repeat_off &&
	(HIWORD(lParam) & (KF_UP | KF_REPEAT)) == KF_REPEAT)
	return 0;

    if ((HIWORD(lParam) & KF_ALTDOWN) && (keystate[VK_RMENU] & 0x80) == 0)
	left_alt = 1;

    key_down = ((HIWORD(lParam) & KF_UP) == 0);

    /* Make sure Ctrl-ALT is not the same as AltGr for ToAscii unless told. */
    if (left_alt && (keystate[VK_CONTROL] & 0x80)) {
	if (cfg.ctrlaltkeys)
	    keystate[VK_MENU] = 0;
	else {
	    keystate[VK_RMENU] = 0x80;
	    left_alt = 0;
	}
    }

    scan = (HIWORD(lParam) & (KF_UP | KF_EXTENDED | 0xFF));
    shift_state = ((keystate[VK_SHIFT] & 0x80) != 0)
	+ ((keystate[VK_CONTROL] & 0x80) != 0) * 2;

    /* Note if AltGr was pressed and if it was used as a compose key */
    if (!compose_state) {
	compose_key = 0x100;
	if (cfg.compose_key) {
	    if (wParam == VK_MENU && (HIWORD(lParam) & KF_EXTENDED))
		compose_key = wParam;
	}
	if (wParam == VK_APPS)
	    compose_key = wParam;
    }

    if (wParam == compose_key) {
	if (compose_state == 0
	    && (HIWORD(lParam) & (KF_UP | KF_REPEAT)) == 0) compose_state =
		1;
	else if (compose_state == 1 && (HIWORD(lParam) & KF_UP))
	    compose_state = 2;
	else
	    compose_state = 0;
    } else if (compose_state == 1 && wParam != VK_CONTROL)
	compose_state = 0;

    if (compose_state > 1 && left_alt)
	compose_state = 0;

    /* Sanitize the number pad if not using a PC NumPad */
    if (left_alt || (term->app_keypad_keys && !cfg.no_applic_k
		     && cfg.funky_type != FUNKY_XTERM)
	|| cfg.funky_type == FUNKY_VT400 || cfg.nethack_keypad || compose_state) {
	if ((HIWORD(lParam) & KF_EXTENDED) == 0) {
	    int nParam = 0;
	    switch (wParam) {
	      case VK_INSERT:
		nParam = VK_NUMPAD0;
		break;
	      case VK_END:
		nParam = VK_NUMPAD1;
		break;
	      case VK_DOWN:
		nParam = VK_NUMPAD2;
		break;
	      case VK_NEXT:
		nParam = VK_NUMPAD3;
		break;
	      case VK_LEFT:
		nParam = VK_NUMPAD4;
		break;
	      case VK_CLEAR:
		nParam = VK_NUMPAD5;
		break;
	      case VK_RIGHT:
		nParam = VK_NUMPAD6;
		break;
	      case VK_HOME:
		nParam = VK_NUMPAD7;
		break;
	      case VK_UP:
		nParam = VK_NUMPAD8;
		break;
	      case VK_PRIOR:
		nParam = VK_NUMPAD9;
		break;
	      case VK_DELETE:
		nParam = VK_DECIMAL;
		break;
	    }
	    if (nParam) {
		if (keystate[VK_NUMLOCK] & 1)
		    shift_state |= 1;
		wParam = nParam;
	    }
	}
    }

    /* If a key is pressed and AltGr is not active */
    if (key_down && (keystate[VK_RMENU] & 0x80) == 0 && !compose_state) {
	/* Okay, prepare for most alts then ... */
	if (left_alt)
	    *p++ = '\033';

	/* Lets see if it's a pattern we know all about ... */
	if (wParam == VK_PRIOR && shift_state == 1) {
	    SendMessage(hwnd, WM_VSCROLL, SB_PAGEUP, 0);
	    return 0;
	}
	if (wParam == VK_PRIOR && shift_state == 2) {
	    SendMessage(hwnd, WM_VSCROLL, SB_LINEUP, 0);
	    return 0;
	}
	if (wParam == VK_NEXT && shift_state == 1) {
	    SendMessage(hwnd, WM_VSCROLL, SB_PAGEDOWN, 0);
	    return 0;
	}
	if (wParam == VK_NEXT && shift_state == 2) {
	    SendMessage(hwnd, WM_VSCROLL, SB_LINEDOWN, 0);
	    return 0;
	}
	if ((wParam == VK_PRIOR || wParam == VK_NEXT) && shift_state == 3) {
	    term_scroll_to_selection(term, (wParam == VK_PRIOR ? 0 : 1));
	    return 0;
	}
	if (wParam == VK_INSERT && shift_state == 1) {
	    request_paste(NULL);
	    return 0;
	}
	if (left_alt && wParam == VK_F4 && cfg.alt_f4) {
	    return -1;
	}
	if (left_alt && wParam == VK_SPACE && cfg.alt_space) {
	    SendMessage(hwnd, WM_SYSCOMMAND, SC_KEYMENU, 0);
	    return -1;
	}
	if (left_alt && wParam == VK_RETURN && cfg.fullscreenonaltenter &&
	    (cfg.resize_action != RESIZE_DISABLED)) {
 	    if ((HIWORD(lParam) & (KF_UP | KF_REPEAT)) != KF_REPEAT)
 		flip_full_screen();
	    return -1;
	}
	/* Control-Numlock for app-keypad mode switch */
	if (wParam == VK_PAUSE && shift_state == 2) {
	    term->app_keypad_keys ^= 1;
	    return 0;
	}

	/* Nethack keypad */
	if (cfg.nethack_keypad && !left_alt) {
	    switch (wParam) {
	      case VK_NUMPAD1:
		*p++ = "bB\002\002"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD2:
		*p++ = "jJ\012\012"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD3:
		*p++ = "nN\016\016"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD4:
		*p++ = "hH\010\010"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD5:
		*p++ = shift_state ? '.' : '.';
		return p - output;
	      case VK_NUMPAD6:
		*p++ = "lL\014\014"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD7:
		*p++ = "yY\031\031"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD8:
		*p++ = "kK\013\013"[shift_state & 3];
		return p - output;
	      case VK_NUMPAD9:
		*p++ = "uU\025\025"[shift_state & 3];
		return p - output;
	    }
	}

	/* Application Keypad */
	if (!left_alt) {
	    int xkey = 0;

	    if (cfg.funky_type == FUNKY_VT400 ||
		(cfg.funky_type <= FUNKY_LINUX &&
		 term->app_keypad_keys && !cfg.no_applic_k)) switch (wParam) {
		  case VK_EXECUTE:
		    xkey = 'P';
		    break;
		  case VK_DIVIDE:
		    xkey = 'Q';
		    break;
		  case VK_MULTIPLY:
		    xkey = 'R';
		    break;
		  case VK_SUBTRACT:
		    xkey = 'S';
		    break;
		}
	    if (term->app_keypad_keys && !cfg.no_applic_k)
		switch (wParam) {
		  case VK_NUMPAD0:
		    xkey = 'p';
		    break;
		  case VK_NUMPAD1:
		    xkey = 'q';
		    break;
		  case VK_NUMPAD2:
		    xkey = 'r';
		    break;
		  case VK_NUMPAD3:
		    xkey = 's';
		    break;
		  case VK_NUMPAD4:
		    xkey = 't';
		    break;
		  case VK_NUMPAD5:
		    xkey = 'u';
		    break;
		  case VK_NUMPAD6:
		    xkey = 'v';
		    break;
		  case VK_NUMPAD7:
		    xkey = 'w';
		    break;
		  case VK_NUMPAD8:
		    xkey = 'x';
		    break;
		  case VK_NUMPAD9:
		    xkey = 'y';
		    break;

		  case VK_DECIMAL:
		    xkey = 'n';
		    break;
		  case VK_ADD:
		    if (cfg.funky_type == FUNKY_XTERM) {
			if (shift_state)
			    xkey = 'l';
			else
			    xkey = 'k';
		    } else if (shift_state)
			xkey = 'm';
		    else
			xkey = 'l';
		    break;

		  case VK_DIVIDE:
		    if (cfg.funky_type == FUNKY_XTERM)
			xkey = 'o';
		    break;
		  case VK_MULTIPLY:
		    if (cfg.funky_type == FUNKY_XTERM)
			xkey = 'j';
		    break;
		  case VK_SUBTRACT:
		    if (cfg.funky_type == FUNKY_XTERM)
			xkey = 'm';
		    break;

		  case VK_RETURN:
		    if (HIWORD(lParam) & KF_EXTENDED)
			xkey = 'M';
		    break;
		}
	    if (xkey) {
		if (term->vt52_mode) {
		    if (xkey >= 'P' && xkey <= 'S')
			p += sprintf((char *) p, "\x1B%c", xkey);
		    else
			p += sprintf((char *) p, "\x1B?%c", xkey);
		} else
		    p += sprintf((char *) p, "\x1BO%c", xkey);
		return p - output;
	    }
	}

	if (wParam == VK_BACK && shift_state == 0) {	/* Backspace */
	    *p++ = (cfg.bksp_is_delete ? 0x7F : 0x08);
	    *p++ = 0;
	    return -2;
	}
#ifdef CYGTERMPORT
	if (wParam == VK_BACK && shift_state != 0) {	/* Shift-Backspace, Ctrl-Backspace */
#else
	if (wParam == VK_BACK && shift_state == 1) {	/* Shift Backspace */
#endif
	    /* We do the opposite of what is configured */
	    *p++ = (cfg.bksp_is_delete ? 0x08 : 0x7F);
	    *p++ = 0;
	    return -2;
	}
	if (wParam == VK_TAB && shift_state == 1) {	/* Shift tab */
#ifdef CYGTERMPORT
	p = output; /* don't also pass escape */
#endif

	    *p++ = 0x1B;
	    *p++ = '[';
	    *p++ = 'Z';
	    return p - output;
	}
	if (wParam == VK_SPACE && shift_state == 2) {	/* Ctrl-Space */
	    *p++ = 0;
	    return p - output;
	}
	if (wParam == VK_SPACE && shift_state == 3) {	/* Ctrl-Shift-Space */
#ifdef CYGTERMPORT
	    p = output; /* don't also pass escape */
	    *p++ = 160; /* Latin1 NBSP */
	    return p - output;
	}
	if (wParam == '/' && shift_state == 2) {	/* Ctrl-/ sends ^_ */
	    *p++ = 037;
#else
	    *p++ = 160;
#endif
	    return p - output;
	}
	if (wParam == VK_CANCEL && shift_state == 2) {	/* Ctrl-Break */
	    if (back)
		back->special(backhandle, TS_BRK);
	    return 0;
	}
	if (wParam == VK_PAUSE) {      /* Break/Pause */
	    *p++ = 26;
	    *p++ = 0;
	    return -2;
	}
	/* Control-2 to Control-8 are special */
	if (shift_state == 2 && wParam >= '2' && wParam <= '8') {
	    *p++ = "\000\033\034\035\036\037\177"[wParam - '2'];
	    return p - output;
	}
	if (shift_state == 2 && (wParam == 0xBD || wParam == 0xBF)) {
	    *p++ = 0x1F;
	    return p - output;
	}
	if (shift_state == 2 && (wParam == 0xDF || wParam == 0xDC)) {
	    *p++ = 0x1C;
	    return p - output;
	}
	if (shift_state == 3 && wParam == 0xDE) {
	    *p++ = 0x1E;	       /* Ctrl-~ == Ctrl-^ in xterm at least */
	    return p - output;
	}
#ifdef CYGTERMPORT
	if (wParam == VK_RETURN && shift_state != 0) {	/* Shift-Return, Ctrl-Return */
	    /* send LINEFEED */
	    *p++ = 012;
 	    return p - output;
 	}
#endif

	if (shift_state == 0 && wParam == VK_RETURN && term->cr_lf_return) {
	    *p++ = '\r';
	    *p++ = '\n';
	    return p - output;
	}

	/*
	 * Next, all the keys that do tilde codes. (ESC '[' nn '~',
	 * for integer decimal nn.)
	 *
	 * We also deal with the weird ones here. Linux VCs replace F1
	 * to F5 by ESC [ [ A to ESC [ [ E. rxvt doesn't do _that_, but
	 * does replace Home and End (1~ and 4~) by ESC [ H and ESC O w
	 * respectively.
	 */
	code = 0;
	switch (wParam) {
	  case VK_F1:
	    code = (keystate[VK_SHIFT] & 0x80 ? 23 : 11);
	    break;
	  case VK_F2:
	    code = (keystate[VK_SHIFT] & 0x80 ? 24 : 12);
	    break;
	  case VK_F3:
	    code = (keystate[VK_SHIFT] & 0x80 ? 25 : 13);
	    break;
	  case VK_F4:
	    code = (keystate[VK_SHIFT] & 0x80 ? 26 : 14);
	    break;
	  case VK_F5:
	    code = (keystate[VK_SHIFT] & 0x80 ? 28 : 15);
	    break;
	  case VK_F6:
	    code = (keystate[VK_SHIFT] & 0x80 ? 29 : 17);
	    break;
	  case VK_F7:
	    code = (keystate[VK_SHIFT] & 0x80 ? 31 : 18);
	    break;
	  case VK_F8:
	    code = (keystate[VK_SHIFT] & 0x80 ? 32 : 19);
	    break;
	  case VK_F9:
	    code = (keystate[VK_SHIFT] & 0x80 ? 33 : 20);
	    break;
	  case VK_F10:
	    code = (keystate[VK_SHIFT] & 0x80 ? 34 : 21);
	    break;
	  case VK_F11:
	    code = 23;
	    break;
	  case VK_F12:
	    code = 24;
	    break;
	  case VK_F13:
	    code = 25;
	    break;
	  case VK_F14:
	    code = 26;
	    break;
	  case VK_F15:
	    code = 28;
	    break;
	  case VK_F16:
	    code = 29;
	    break;
	  case VK_F17:
	    code = 31;
	    break;
	  case VK_F18:
	    code = 32;
	    break;
	  case VK_F19:
	    code = 33;
	    break;
	  case VK_F20:
	    code = 34;
	    break;
	}
	if ((shift_state&2) == 0) switch (wParam) {
	  case VK_HOME:
	    code = 1;
	    break;
	  case VK_INSERT:
	    code = 2;
	    break;
	  case VK_DELETE:
	    code = 3;
	    break;
	  case VK_END:
	    code = 4;
	    break;
	  case VK_PRIOR:
	    code = 5;
	    break;
	  case VK_NEXT:
	    code = 6;
	    break;
	}
	/* Reorder edit keys to physical order */
	if (cfg.funky_type == FUNKY_VT400 && code <= 6)
	    code = "\0\2\1\4\5\3\6"[code];

	if (term->vt52_mode && code > 0 && code <= 6) {
	    p += sprintf((char *) p, "\x1B%c", " HLMEIG"[code]);
	    return p - output;
	}

	if (cfg.funky_type == FUNKY_SCO &&     /* SCO function keys */
	    code >= 11 && code <= 34) {
	    char codes[] = "MNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@[\\]^_`{";
	    int index = 0;
	    switch (wParam) {
	      case VK_F1: index = 0; break;
	      case VK_F2: index = 1; break;
	      case VK_F3: index = 2; break;
	      case VK_F4: index = 3; break;
	      case VK_F5: index = 4; break;
	      case VK_F6: index = 5; break;
	      case VK_F7: index = 6; break;
	      case VK_F8: index = 7; break;
	      case VK_F9: index = 8; break;
	      case VK_F10: index = 9; break;
	      case VK_F11: index = 10; break;
	      case VK_F12: index = 11; break;
	    }
	    if (keystate[VK_SHIFT] & 0x80) index += 12;
	    if (keystate[VK_CONTROL] & 0x80) index += 24;
	    p += sprintf((char *) p, "\x1B[%c", codes[index]);
	    return p - output;
	}
	if (cfg.funky_type == FUNKY_SCO &&     /* SCO small keypad */
	    code >= 1 && code <= 6) {
	    char codes[] = "HL.FIG";
	    if (code == 3) {
		*p++ = '\x7F';
	    } else {
		p += sprintf((char *) p, "\x1B[%c", codes[code-1]);
	    }
	    return p - output;
	}
	if ((term->vt52_mode || cfg.funky_type == FUNKY_VT100P) && code >= 11 && code <= 24) {
	    int offt = 0;
	    if (code > 15)
		offt++;
	    if (code > 21)
		offt++;
	    if (term->vt52_mode)
		p += sprintf((char *) p, "\x1B%c", code + 'P' - 11 - offt);
	    else
		p +=
		    sprintf((char *) p, "\x1BO%c", code + 'P' - 11 - offt);
	    return p - output;
	}
	if (cfg.funky_type == FUNKY_LINUX && code >= 11 && code <= 15) {
	    p += sprintf((char *) p, "\x1B[[%c", code + 'A' - 11);
	    return p - output;
	}
	if (cfg.funky_type == FUNKY_XTERM && code >= 11 && code <= 14) {
	    if (term->vt52_mode)
		p += sprintf((char *) p, "\x1B%c", code + 'P' - 11);
	    else
		p += sprintf((char *) p, "\x1BO%c", code + 'P' - 11);
	    return p - output;
	}
	if (cfg.rxvt_homeend && (code == 1 || code == 4)) {
	    p += sprintf((char *) p, code == 1 ? "\x1B[H" : "\x1BOw");
	    return p - output;
	}
	if (code) {
	    p += sprintf((char *) p, "\x1B[%d~", code);
	    return p - output;
	}

	/*
	 * Now the remaining keys (arrows and Keypad 5. Keypad 5 for
	 * some reason seems to send VK_CLEAR to Windows...).
	 */
	{
	    char xkey = 0;
	    switch (wParam) {
	      case VK_UP:
		xkey = 'A';
		break;
	      case VK_DOWN:
		xkey = 'B';
		break;
	      case VK_RIGHT:
		xkey = 'C';
		break;
	      case VK_LEFT:
		xkey = 'D';
		break;
	      case VK_CLEAR:
		xkey = 'G';
		break;
	    }
	    if (xkey) {
		p += format_arrow_key(p, term, xkey, shift_state);
		return p - output;
	    }
	}

	/*
	 * Finally, deal with Return ourselves. (Win95 seems to
	 * foul it up when Alt is pressed, for some reason.)
	 */
	if (wParam == VK_RETURN) {     /* Return */
	    *p++ = 0x0D;
	    *p++ = 0;
	    return -2;
	}

	if (left_alt && wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
	    alt_sum = alt_sum * 10 + wParam - VK_NUMPAD0;
	else
	    alt_sum = 0;
    }

    /* Okay we've done everything interesting; let windows deal with 
     * the boring stuff */
    {
	BOOL capsOn=0;

	/* helg: clear CAPS LOCK state if caps lock switches to cyrillic */
	if(cfg.xlat_capslockcyr && keystate[VK_CAPITAL] != 0) {
	    capsOn= !left_alt;
	    keystate[VK_CAPITAL] = 0;
	}

	/* XXX how do we know what the max size of the keys array should
	 * be is? There's indication on MS' website of an Inquire/InquireEx
	 * functioning returning a KBINFO structure which tells us. */
	if (osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) {
	    /* XXX 'keys' parameter is declared in MSDN documentation as
	     * 'LPWORD lpChar'.
	     * The experience of a French user indicates that on
	     * Win98, WORD[] should be passed in, but on Win2K, it should
	     * be BYTE[]. German WinXP and my Win2K with "US International"
	     * driver corroborate this.
	     * Experimentally I've conditionalised the behaviour on the
	     * Win9x/NT split, but I suspect it's worse than that.
	     * See wishlist item `win-dead-keys' for more horrible detail
	     * and speculations. */
	    BYTE keybs[3];
	    int i;
	    r = ToAsciiEx(wParam, scan, keystate, (LPWORD)keybs, 0, kbd_layout);
	    for (i=0; i<3; i++) keys[i] = keybs[i];
	} else {
	    r = ToAsciiEx(wParam, scan, keystate, keys, 0, kbd_layout);
	}
#ifdef SHOW_TOASCII_RESULT
	if (r == 1 && !key_down) {
	    if (alt_sum) {
		if (in_utf(term) || ucsdata.dbcs_screenfont)
		    debug((", (U+%04x)", alt_sum));
		else
		    debug((", LCH(%d)", alt_sum));
	    } else {
		debug((", ACH(%d)", keys[0]));
	    }
	} else if (r > 0) {
	    int r1;
	    debug((", ASC("));
	    for (r1 = 0; r1 < r; r1++) {
		debug(("%s%d", r1 ? "," : "", keys[r1]));
	    }
	    debug((")"));
	}
#endif
	if (r > 0) {
	    WCHAR keybuf;

	    /*
	     * Interrupt an ongoing paste. I'm not sure this is
	     * sensible, but for the moment it's preferable to
	     * having to faff about buffering things.
	     */
	    term_nopaste(term);

	    p = output;
	    for (i = 0; i < r; i++) {
		unsigned char ch = (unsigned char) keys[i];

		if (compose_state == 2 && (ch & 0x80) == 0 && ch > ' ') {
		    compose_char = ch;
		    compose_state++;
		    continue;
		}
		if (compose_state == 3 && (ch & 0x80) == 0 && ch > ' ') {
		    int nc;
		    compose_state = 0;

		    if ((nc = check_compose(compose_char, ch)) == -1) {
			MessageBeep(MB_ICONHAND);
			return 0;
		    }
		    keybuf = nc;
		    term_seen_key_event(term);
		    if (ldisc)
			luni_send(ldisc, &keybuf, 1, 1);
		    continue;
		}

		compose_state = 0;

		if (!key_down) {
		    if (alt_sum) {
			if (in_utf(term) || ucsdata.dbcs_screenfont) {
			    keybuf = alt_sum;
			    term_seen_key_event(term);
			    if (ldisc)
				luni_send(ldisc, &keybuf, 1, 1);
			} else {
			    ch = (char) alt_sum;
			    /*
			     * We need not bother about stdin
			     * backlogs here, because in GUI PuTTY
			     * we can't do anything about it
			     * anyway; there's no means of asking
			     * Windows to hold off on KEYDOWN
			     * messages. We _have_ to buffer
			     * everything we're sent.
			     */
			    term_seen_key_event(term);
			    if (ldisc)
				ldisc_send(ldisc, &ch, 1, 1);
			}
			alt_sum = 0;
		    } else {
			term_seen_key_event(term);
			if (ldisc)
			    lpage_send(ldisc, kbd_codepage, &ch, 1, 1);
		    }
		} else {
		    if(capsOn && ch < 0x80) {
			WCHAR cbuf[2];
			cbuf[0] = 27;
			cbuf[1] = xlat_uskbd2cyrllic(ch);
			term_seen_key_event(term);
			if (ldisc)
			    luni_send(ldisc, cbuf+!left_alt, 1+!!left_alt, 1);
		    } else {
			char cbuf[2];
			cbuf[0] = '\033';
#ifdef CYGTERMPORT
			cbuf[1] = ch | ((left_alt & cfg.alt_metabit) << 7);
#else
			cbuf[1] = ch;
#endif
			term_seen_key_event(term);
			if (ldisc)
			    lpage_send(ldisc, kbd_codepage,
#ifdef CYGTERMPORT
				    cbuf + !(left_alt & !cfg.alt_metabit), 
				    1 + !!(left_alt & !cfg.alt_metabit), 
				    1);
#else
				       cbuf+!left_alt, 1+!!left_alt, 1);
#endif
		    }
		}
		show_mouseptr(0);
	    }

	    /* This is so the ALT-Numpad and dead keys work correctly. */
	    keys[0] = 0;

	    return p - output;
	}
	/* If we're definitly not building up an ALT-54321 then clear it */
	if (!left_alt)
	    keys[0] = 0;
	/* If we will be using alt_sum fix the 256s */
	else if (keys[0] && (in_utf(term) || ucsdata.dbcs_screenfont))
	    keys[0] = 10;
    }

    /*
     * ALT alone may or may not want to bring up the System menu.
     * If it's not meant to, we return 0 on presses or releases of
     * ALT, to show that we've swallowed the keystroke. Otherwise
     * we return -1, which means Windows will give the keystroke
     * its default handling (i.e. bring up the System menu).
     */
    if (wParam == VK_MENU && !cfg.alt_only)
	return 0;

    return -1;
}

#ifdef PERSOPORT

void set_title_internal(void *frontend, char *title) {
    sfree(window_name);
    window_name = snewn(1 + strlen(title), char);
    strcpy(window_name, title);
    if (cfg.win_name_always || !IsIconic(hwnd))
	SetWindowText(hwnd, title);
	}

/* Creer un titre de fenetre a partir d'un schema donne
	%%s: nom de la session (vide sinon)
	%%h: le hostname
	%%u: le user
	%%f: le folder auquel apprtient le session
	%%p: le port
	%%P: le protocole
Ex: %%P://%%u@%%h:%%p
Ex: %%f / %%s
*/
void make_title( char * res, char * fmt, char * title ) {
	int p;
	char b[256] ;
	int port ;
	
	sprintf( res, fmt, title ) ;

	while( (p=poss( "%%s", res)) > 0 ) { del(res,p,3); if(strlen(cfg.sessionname)>0) insert(res,cfg.sessionname,p); }
	while( (p=poss( "%%h", res)) > 0 ) { del(res,p,3); insert(res,cfg.host,p); }
	while( (p=poss( "%%u", res)) > 0 ) { del(res,p,3); insert(res,cfg.username,p); }
	while( (p=poss( "%%f", res)) > 0 ) { del(res,p,3); if(strlen(cfg.folder)>0) insert(res,cfg.folder,p); }
	
	port = cfg.port ; 
	switch(cfg.protocol) {
		case PROT_RAW: strcpy(b,"raw"); break;
		case PROT_TELNET: strcpy(b,"telnet"); if(port==-1) port=23 ; break;
		case PROT_RLOGIN: strcpy(b,"rlogin"); break;
		case PROT_SSH: strcpy(b,"ssh"); if(port==-1) port=22 ; break;
#ifdef CYGTERMPORT
		case PROT_CYGTERM: strcpy(b,"cyg"); break;
#endif
		case PROT_SERIAL: strcpy(b,"serial"); break;
		}
	while( (p=poss( "%%P", res)) > 0 ) { del(res,p,3); insert(res,b,p); }
	
	sprintf(b,"%d", port ) ; 
	while( (p=poss( "%%p", res)) > 0 ) { del(res,p,3); insert(res,b,p); }
	}

void set_title(void *frontend, char *title) {
	char *buffer, fmt[256]="%s" ;

	if( (title[0]=='_')&&(title[1]=='_') ) { // Mode commande a distance
		if( ManageLocalCmd( MainHwnd, title+2 ) ) return ;
		}
	
	if( !TitleBarFlag ) { set_title_internal( frontend, title ) ; return ; }
		
	if( strstr(title, " (PROTECTED)")==(title+strlen(title)-12) ) 
		{ title[strlen(title)-12]='\0' ; }

#if (defined IMAGEPORT) && (!defined FDJ)
	buffer = (char*) malloc( strlen( title ) + strlen( cfg.host ) + strlen( cfg.bg_image_filename.path ) + 40 ) ; 
	if( BackgroundImageFlag && ImageViewerFlag && (!PuttyFlag) ) {	sprintf( buffer, "%s", cfg.bg_image_filename.path ) ; }
	else 
#else
	buffer = (char*) malloc( strlen( title ) + strlen( cfg.host ) + 40 ) ; 
#endif
	if( (SizeFlag) && (!IsZoomed( MainHwnd )) ) {
		if( strlen( title ) > 0 ) {
			if( title[strlen(title)-1] == ']' ) make_title( buffer, "%s", title ) ;
			else { 
				sprintf( fmt, "%%s [%dx%d]", cfg.height, cfg.width ) ;
				make_title( buffer, fmt, title ) ;
				}
			}
		else sprintf( buffer, "%s [%dx%d] - %s", cfg.host, cfg.height, cfg.width, appname ) ;
		}
	else {
		if( strlen( title ) > 0 ) make_title( buffer, "%s", title ) ;
		else sprintf( buffer, "%s - %s", cfg.host, appname ) ;
		}
	
	if( ProtectFlag ) if( strstr(buffer, " (PROTECTED)")==NULL ) strcat( buffer, " (PROTECTED)" ) ;
	set_title_internal( frontend, buffer ) ;
	
	free(buffer);
	}
	
void set_icon(void *frontend, char *title2)
{
	char title[1024] ;
	make_title( title, "%s", title2 ) ;
    sfree(icon_name);
    icon_name = snewn(1 + strlen(title), char);
    strcpy(icon_name, title);
    if (!cfg.win_name_always && IsIconic(hwnd))
	SetWindowText(hwnd, title);
}
#else
void set_title(void *frontend, char *title)
{
    sfree(window_name);
    window_name = snewn(1 + strlen(title), char);
    strcpy(window_name, title);
    if (cfg.win_name_always || !IsIconic(hwnd))
	SetWindowText(hwnd, title);
}

void set_icon(void *frontend, char *title)
{
    sfree(icon_name);
    icon_name = snewn(1 + strlen(title), char);
    strcpy(icon_name, title);
    if (!cfg.win_name_always && IsIconic(hwnd))
	SetWindowText(hwnd, title);
}
#endif

void set_sbar(void *frontend, int total, int start, int page)
{
    SCROLLINFO si;

    if (is_full_screen() ? !cfg.scrollbar_in_fullscreen : !cfg.scrollbar)
	return;

    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = total - 1;
    si.nPage = page;
    si.nPos = start;
    if (hwnd)
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

Context get_ctx(void *frontend)
{
    HDC hdc;
    if (hwnd) {
	hdc = GetDC(hwnd);
	if (hdc && pal)
	    SelectPalette(hdc, pal, FALSE);
	return hdc;
    } else
	return NULL;
}

void free_ctx(Context ctx)
{
    SelectPalette(ctx, GetStockObject(DEFAULT_PALETTE), FALSE);
    ReleaseDC(hwnd, ctx);
}

static void real_palette_set(int n, int r, int g, int b)
{
    if (pal) {
	logpal->palPalEntry[n].peRed = r;
	logpal->palPalEntry[n].peGreen = g;
	logpal->palPalEntry[n].peBlue = b;
	logpal->palPalEntry[n].peFlags = PC_NOCOLLAPSE;
	colours[n] = PALETTERGB(r, g, b);
	SetPaletteEntries(pal, 0, NALLCOLOURS, logpal->palPalEntry);
    } else
	colours[n] = RGB(r, g, b);
}

void palette_set(void *frontend, int n, int r, int g, int b)
{
    if (n >= 16)
	n += 256 - 16;
    if (n > NALLCOLOURS)
	return;
    real_palette_set(n, r, g, b);
    if (pal) {
	HDC hdc = get_ctx(frontend);
	UnrealizeObject(pal);
	RealizePalette(hdc);
	free_ctx(hdc);
    } else {
	if (n == (ATTR_DEFBG>>ATTR_BGSHIFT))
	    /* If Default Background changes, we need to ensure any
	     * space between the text area and the window border is
	     * redrawn. */
	    InvalidateRect(hwnd, NULL, TRUE);
    }
}

void palette_reset(void *frontend)
{
    int i;

    /* And this */
    for (i = 0; i < NALLCOLOURS; i++) {
	if (pal) {
	    logpal->palPalEntry[i].peRed = defpal[i].rgbtRed;
	    logpal->palPalEntry[i].peGreen = defpal[i].rgbtGreen;
	    logpal->palPalEntry[i].peBlue = defpal[i].rgbtBlue;
	    logpal->palPalEntry[i].peFlags = 0;
	    colours[i] = PALETTERGB(defpal[i].rgbtRed,
				    defpal[i].rgbtGreen,
				    defpal[i].rgbtBlue);
	} else
	    colours[i] = RGB(defpal[i].rgbtRed,
			     defpal[i].rgbtGreen, defpal[i].rgbtBlue);
    }

    if (pal) {
	HDC hdc;
	SetPaletteEntries(pal, 0, NALLCOLOURS, logpal->palPalEntry);
	hdc = get_ctx(frontend);
	RealizePalette(hdc);
	free_ctx(hdc);
    } else {
	/* Default Background may have changed. Ensure any space between
	 * text area and window border is redrawn. */
	InvalidateRect(hwnd, NULL, TRUE);
    }
}

void write_aclip(void *frontend, char *data, int len, int must_deselect)
{
    HGLOBAL clipdata;
    void *lock;

    clipdata = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, len + 1);
    if (!clipdata)
	return;
    lock = GlobalLock(clipdata);
    if (!lock)
	return;
    memcpy(lock, data, len);
    ((unsigned char *) lock)[len] = 0;
    GlobalUnlock(clipdata);

    if (!must_deselect)
	SendMessage(hwnd, WM_IGNORE_CLIP, TRUE, 0);

    if (OpenClipboard(hwnd)) {
	EmptyClipboard();
	SetClipboardData(CF_TEXT, clipdata);
	CloseClipboard();
    } else
	GlobalFree(clipdata);

    if (!must_deselect)
	SendMessage(hwnd, WM_IGNORE_CLIP, FALSE, 0);
}

#ifdef URLPORT
static void detect_and_launch_url(char * urldata) {
    char * pc;
    int len;
    int urlbegin;
    int hostend;
    int i;
 
    URLclick( MainHwnd ) ; return ;

    urlbegin = 0;
    
    len = strlen(urldata);
    pc = urldata;
    
    // "ftp://" is shortest detected begining of URL
    if(len<=6)
        return;
    
    // skip whitespaces at the begining
    while(len > 6 && isspace(*pc)) {
        len--;
        pc++;
    }
    
    // detect urls
    if(!strncmp(pc, "ftp://", 6))
        urlbegin = 6;
    else if(!strncmp(pc, "http://", 7))
        urlbegin = 7;
    else if(!strncmp(pc, "https://", 8))
        urlbegin = 8;
    else
        return;
    
    // skip whitespaces at the end
    while(len > urlbegin && isspace(pc[len-1])) {
        len--;
        pc[len]=0;
    }
    
    if(len <= urlbegin)
        return;
    
    // find first '/' or end
    for(hostend = urlbegin; pc[hostend] && pc[hostend] != '/'; hostend++);
        
    // check for spaces in hostname
    for(i = urlbegin; i < hostend; i++)
        if(isspace(pc[i]))
            return;
    
    ShellExecute(hwnd, NULL, pc, NULL, NULL, SW_SHOWDEFAULT);
}
#endif

/*
 * Note: unlike write_aclip() this will not append a nul.
 */
void write_clip(void *frontend, wchar_t * data, int *attr, int len, int must_deselect)
{
    HGLOBAL clipdata, clipdata2, clipdata3;
    int len2;
    void *lock, *lock2, *lock3;
#ifdef URLPORT
	char * urldata;
#endif

    len2 = WideCharToMultiByte(CP_ACP, 0, data, len, 0, 0, NULL, NULL);

    clipdata = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE,
			   len * sizeof(wchar_t));
    clipdata2 = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, len2);

    if (!clipdata || !clipdata2) {
	if (clipdata)
	    GlobalFree(clipdata);
	if (clipdata2)
	    GlobalFree(clipdata2);
	return;
    }
    if (!(lock = GlobalLock(clipdata)))
	return;
    if (!(lock2 = GlobalLock(clipdata2)))
	return;

    memcpy(lock, data, len * sizeof(wchar_t));
    WideCharToMultiByte(CP_ACP, 0, data, len, lock2, len2, NULL, NULL);
#ifdef URLPORT
    if(cfg.copy_clipbd_url_reg)
        urldata = strdup((char*)lock2);
    else
        urldata = 0;
#endif

    if (cfg.rtf_paste) {
	wchar_t unitab[256];
	char *rtf = NULL;
	unsigned char *tdata = (unsigned char *)lock2;
	wchar_t *udata = (wchar_t *)lock;
	int rtflen = 0, uindex = 0, tindex = 0;
	int rtfsize = 0;
	int multilen, blen, alen, totallen, i;
	char before[16], after[4];
	int fgcolour,  lastfgcolour  = 0;
	int bgcolour,  lastbgcolour  = 0;
	int attrBold,  lastAttrBold  = 0;
	int attrUnder, lastAttrUnder = 0;
	int palette[NALLCOLOURS];
	int numcolours;

	get_unitab(CP_ACP, unitab, 0);

	rtfsize = 100 + strlen(cfg.font.name);
	rtf = snewn(rtfsize, char);
	rtflen = sprintf(rtf, "{\\rtf1\\ansi\\deff0{\\fonttbl\\f0\\fmodern %s;}\\f0\\fs%d",
			 cfg.font.name, cfg.font.height*2);

	/*
	 * Add colour palette
	 * {\colortbl ;\red255\green0\blue0;\red0\green0\blue128;}
	 */

	/*
	 * First - Determine all colours in use
	 *    o  Foregound and background colours share the same palette
	 */
	if (attr) {
	    memset(palette, 0, sizeof(palette));
	    for (i = 0; i < (len-1); i++) {
		fgcolour = ((attr[i] & ATTR_FGMASK) >> ATTR_FGSHIFT);
		bgcolour = ((attr[i] & ATTR_BGMASK) >> ATTR_BGSHIFT);

		if (attr[i] & ATTR_REVERSE) {
		    int tmpcolour = fgcolour;	/* Swap foreground and background */
		    fgcolour = bgcolour;
		    bgcolour = tmpcolour;
		}

		if (bold_mode == BOLD_COLOURS && (attr[i] & ATTR_BOLD)) {
		    if (fgcolour  <   8)	/* ANSI colours */
			fgcolour +=   8;
		    else if (fgcolour >= 256)	/* Default colours */
			fgcolour ++;
		}

		if (attr[i] & ATTR_BLINK) {
		    if (bgcolour  <   8)	/* ANSI colours */
			bgcolour +=   8;
    		    else if (bgcolour >= 256)	/* Default colours */
			bgcolour ++;
		}

		palette[fgcolour]++;
		palette[bgcolour]++;
	    }

	    /*
	     * Next - Create a reduced palette
	     */
	    numcolours = 0;
	    for (i = 0; i < NALLCOLOURS; i++) {
		if (palette[i] != 0)
		    palette[i]  = ++numcolours;
	    }

	    /*
	     * Finally - Write the colour table
	     */
	    rtf = sresize(rtf, rtfsize + (numcolours * 25), char);
	    strcat(rtf, "{\\colortbl ;");
	    rtflen = strlen(rtf);

	    for (i = 0; i < NALLCOLOURS; i++) {
		if (palette[i] != 0) {
		    rtflen += sprintf(&rtf[rtflen], "\\red%d\\green%d\\blue%d;", defpal[i].rgbtRed, defpal[i].rgbtGreen, defpal[i].rgbtBlue);
		}
	    }
	    strcpy(&rtf[rtflen], "}");
	    rtflen ++;
	}

	/*
	 * We want to construct a piece of RTF that specifies the
	 * same Unicode text. To do this we will read back in
	 * parallel from the Unicode data in `udata' and the
	 * non-Unicode data in `tdata'. For each character in
	 * `tdata' which becomes the right thing in `udata' when
	 * looked up in `unitab', we just copy straight over from
	 * tdata. For each one that doesn't, we must WCToMB it
	 * individually and produce a \u escape sequence.
	 * 
	 * It would probably be more robust to just bite the bullet
	 * and WCToMB each individual Unicode character one by one,
	 * then MBToWC each one back to see if it was an accurate
	 * translation; but that strikes me as a horrifying number
	 * of Windows API calls so I want to see if this faster way
	 * will work. If it screws up badly we can always revert to
	 * the simple and slow way.
	 */
	while (tindex < len2 && uindex < len &&
	       tdata[tindex] && udata[uindex]) {
	    if (tindex + 1 < len2 &&
		tdata[tindex] == '\r' &&
		tdata[tindex+1] == '\n') {
		tindex++;
		uindex++;
            }

            /*
             * Set text attributes
             */
            if (attr) {
                if (rtfsize < rtflen + 64) {
		    rtfsize = rtflen + 512;
		    rtf = sresize(rtf, rtfsize, char);
                }

                /*
                 * Determine foreground and background colours
                 */
                fgcolour = ((attr[tindex] & ATTR_FGMASK) >> ATTR_FGSHIFT);
                bgcolour = ((attr[tindex] & ATTR_BGMASK) >> ATTR_BGSHIFT);

		if (attr[tindex] & ATTR_REVERSE) {
		    int tmpcolour = fgcolour;	    /* Swap foreground and background */
		    fgcolour = bgcolour;
		    bgcolour = tmpcolour;
		}

		if (bold_mode == BOLD_COLOURS && (attr[tindex] & ATTR_BOLD)) {
		    if (fgcolour  <   8)	    /* ANSI colours */
			fgcolour +=   8;
		    else if (fgcolour >= 256)	    /* Default colours */
			fgcolour ++;
                }

		if (attr[tindex] & ATTR_BLINK) {
		    if (bgcolour  <   8)	    /* ANSI colours */
			bgcolour +=   8;
		    else if (bgcolour >= 256)	    /* Default colours */
			bgcolour ++;
                }

                /*
                 * Collect other attributes
                 */
		if (bold_mode != BOLD_COLOURS)
		    attrBold  = attr[tindex] & ATTR_BOLD;
		else
		    attrBold  = 0;
                
		attrUnder = attr[tindex] & ATTR_UNDER;

                /*
                 * Reverse video
		 *   o  If video isn't reversed, ignore colour attributes for default foregound
	         *	or background.
		 *   o  Special case where bolded text is displayed using the default foregound
		 *      and background colours - force to bolded RTF.
                 */
		if (!(attr[tindex] & ATTR_REVERSE)) {
		    if (bgcolour >= 256)	    /* Default color */
			bgcolour  = -1;		    /* No coloring */

		    if (fgcolour >= 256) {	    /* Default colour */
			if (bold_mode == BOLD_COLOURS && (fgcolour & 1) && bgcolour == -1)
			    attrBold = ATTR_BOLD;   /* Emphasize text with bold attribute */

			fgcolour  = -1;		    /* No coloring */
		    }
		}

                /*
                 * Write RTF text attributes
                 */
		if (lastfgcolour != fgcolour) {
                    lastfgcolour  = fgcolour;
		    rtflen       += sprintf(&rtf[rtflen], "\\cf%d ", (fgcolour >= 0) ? palette[fgcolour] : 0);
                }

                if (lastbgcolour != bgcolour) {
                    lastbgcolour  = bgcolour;
                    rtflen       += sprintf(&rtf[rtflen], "\\highlight%d ", (bgcolour >= 0) ? palette[bgcolour] : 0);
                }

		if (lastAttrBold != attrBold) {
		    lastAttrBold  = attrBold;
		    rtflen       += sprintf(&rtf[rtflen], "%s", attrBold ? "\\b " : "\\b0 ");
		}

                if (lastAttrUnder != attrUnder) {
                    lastAttrUnder  = attrUnder;
                    rtflen        += sprintf(&rtf[rtflen], "%s", attrUnder ? "\\ul " : "\\ulnone ");
                }
	    }

	    if (unitab[tdata[tindex]] == udata[uindex]) {
		multilen = 1;
		before[0] = '\0';
		after[0] = '\0';
		blen = alen = 0;
	    } else {
		multilen = WideCharToMultiByte(CP_ACP, 0, unitab+uindex, 1,
					       NULL, 0, NULL, NULL);
		if (multilen != 1) {
		    blen = sprintf(before, "{\\uc%d\\u%d", multilen,
				   udata[uindex]);
		    alen = 1; strcpy(after, "}");
		} else {
		    blen = sprintf(before, "\\u%d", udata[uindex]);
		    alen = 0; after[0] = '\0';
		}
	    }
	    assert(tindex + multilen <= len2);
	    totallen = blen + alen;
	    for (i = 0; i < multilen; i++) {
		if (tdata[tindex+i] == '\\' ||
		    tdata[tindex+i] == '{' ||
		    tdata[tindex+i] == '}')
		    totallen += 2;
		else if (tdata[tindex+i] == 0x0D || tdata[tindex+i] == 0x0A)
		    totallen += 6;     /* \par\r\n */
		else if (tdata[tindex+i] > 0x7E || tdata[tindex+i] < 0x20)
		    totallen += 4;
		else
		    totallen++;
	    }

	    if (rtfsize < rtflen + totallen + 3) {
		rtfsize = rtflen + totallen + 512;
		rtf = sresize(rtf, rtfsize, char);
	    }

	    strcpy(rtf + rtflen, before); rtflen += blen;
	    for (i = 0; i < multilen; i++) {
		if (tdata[tindex+i] == '\\' ||
		    tdata[tindex+i] == '{' ||
		    tdata[tindex+i] == '}') {
		    rtf[rtflen++] = '\\';
		    rtf[rtflen++] = tdata[tindex+i];
		} else if (tdata[tindex+i] == 0x0D || tdata[tindex+i] == 0x0A) {
		    rtflen += sprintf(rtf+rtflen, "\\par\r\n");
		} else if (tdata[tindex+i] > 0x7E || tdata[tindex+i] < 0x20) {
		    rtflen += sprintf(rtf+rtflen, "\\'%02x", tdata[tindex+i]);
		} else {
		    rtf[rtflen++] = tdata[tindex+i];
		}
	    }
	    strcpy(rtf + rtflen, after); rtflen += alen;

	    tindex += multilen;
	    uindex++;
	}

        rtf[rtflen++] = '}';	       /* Terminate RTF stream */
        rtf[rtflen++] = '\0';
        rtf[rtflen++] = '\0';

	clipdata3 = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, rtflen);
	if (clipdata3 && (lock3 = GlobalLock(clipdata3)) != NULL) {
	    memcpy(lock3, rtf, rtflen);
	    GlobalUnlock(clipdata3);
	}
	sfree(rtf);
    } else
	clipdata3 = NULL;

    GlobalUnlock(clipdata);
    GlobalUnlock(clipdata2);

    if (!must_deselect)
	SendMessage(hwnd, WM_IGNORE_CLIP, TRUE, 0);

    if (OpenClipboard(hwnd)) {
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, clipdata);
	SetClipboardData(CF_TEXT, clipdata2);
	if (clipdata3)
	    SetClipboardData(RegisterClipboardFormat(CF_RTF), clipdata3);
	CloseClipboard();
    } else {
	GlobalFree(clipdata);
	GlobalFree(clipdata2);
    }

#ifdef URLPORT
    if( !PuttyFlag &&  cfg.copy_clipbd_url_reg && urldata) {
        detect_and_launch_url(urldata);
        
        free(urldata);
    }
#endif
    if (!must_deselect)
	SendMessage(hwnd, WM_IGNORE_CLIP, FALSE, 0);
}

static DWORD WINAPI clipboard_read_threadfunc(void *param)
{
    HWND hwnd = (HWND)param;
    HGLOBAL clipdata;

    if (OpenClipboard(NULL)) {
	if ((clipdata = GetClipboardData(CF_UNICODETEXT))) {
	    SendMessage(hwnd, WM_GOT_CLIPDATA, (WPARAM)1, (LPARAM)clipdata);
	} else if ((clipdata = GetClipboardData(CF_TEXT))) {
	    SendMessage(hwnd, WM_GOT_CLIPDATA, (WPARAM)0, (LPARAM)clipdata);
	}
	CloseClipboard();
    }

    return 0;
}

static int process_clipdata(HGLOBAL clipdata, int unicode)
{
    sfree(clipboard_contents);
    clipboard_contents = NULL;
    clipboard_length = 0;

    if (unicode) {
	wchar_t *p = GlobalLock(clipdata);
	wchar_t *p2;

	if (p) {
	    /* Unwilling to rely on Windows having wcslen() */
	    for (p2 = p; *p2; p2++);
	    clipboard_length = p2 - p;
	    clipboard_contents = snewn(clipboard_length + 1, wchar_t);
	    memcpy(clipboard_contents, p, clipboard_length * sizeof(wchar_t));
	    clipboard_contents[clipboard_length] = L'\0';
	    return TRUE;
	}
    } else {
	char *s = GlobalLock(clipdata);
	int i;

	if (s) {
	    i = MultiByteToWideChar(CP_ACP, 0, s, strlen(s) + 1, 0, 0);
	    clipboard_contents = snewn(i, wchar_t);
	    MultiByteToWideChar(CP_ACP, 0, s, strlen(s) + 1,
				clipboard_contents, i);
	    clipboard_length = i - 1;
	    clipboard_contents[clipboard_length] = L'\0';
	    return TRUE;
	}
    }

    return FALSE;
}

void request_paste(void *frontend)
{
    /*
     * I always thought pasting was synchronous in Windows; the
     * clipboard access functions certainly _look_ synchronous,
     * unlike the X ones. But in fact it seems that in some
     * situations the contents of the clipboard might not be
     * immediately available, and the clipboard-reading functions
     * may block. This leads to trouble if the application
     * delivering the clipboard data has to get hold of it by -
     * for example - talking over a network connection which is
     * forwarded through this very PuTTY.
     *
     * Hence, we spawn a subthread to read the clipboard, and do
     * our paste when it's finished. The thread will send a
     * message back to our main window when it terminates, and
     * that tells us it's OK to paste.
     */
    DWORD in_threadid; /* required for Win9x */
    CreateThread(NULL, 0, clipboard_read_threadfunc,
		 hwnd, 0, &in_threadid);
}

void get_clip(void *frontend, wchar_t **p, int *len)
{
    if (p) {
	*p = clipboard_contents;
	*len = clipboard_length;
    }
}

#if 0
/*
 * Move `lines' lines from position `from' to position `to' in the
 * window.
 */
void optimised_move(void *frontend, int to, int from, int lines)
{
    RECT r;
    int min, max;

    min = (to < from ? to : from);
    max = to + from - min;

    r.left = offset_width;
    r.right = offset_width + term->cols * font_width;
    r.top = offset_height + min * font_height;
    r.bottom = offset_height + (max + lines) * font_height;
    ScrollWindow(hwnd, 0, (to - from) * font_height, &r, &r);
}
#endif

/*
 * Print a message box and perform a fatal exit.
 */
void fatalbox(char *fmt, ...)
{
    va_list ap;
    char *stuff, morestuff[100];

    va_start(ap, fmt);
    stuff = dupvprintf(fmt, ap);
    va_end(ap);
    sprintf(morestuff, "%.70s Fatal Error", appname);
    MessageBox(hwnd, stuff, morestuff, MB_ICONERROR | MB_OK);
    sfree(stuff);
    cleanup_exit(1);
}

/*
 * Print a modal (Really Bad) message box and perform a fatal exit.
 */
void modalfatalbox(char *fmt, ...)
{
    va_list ap;
    char *stuff, morestuff[100];

    va_start(ap, fmt);
    stuff = dupvprintf(fmt, ap);
    va_end(ap);
    sprintf(morestuff, "%.70s Fatal Error", appname);
    MessageBox(hwnd, stuff, morestuff,
	       MB_SYSTEMMODAL | MB_ICONERROR | MB_OK);
    sfree(stuff);
    cleanup_exit(1);
}

DECL_WINDOWS_FUNCTION(static, BOOL, FlashWindowEx, (PFLASHWINFO));

static void init_flashwindow(void)
{
    HMODULE user32_module = load_system32_dll("user32.dll");
    GET_WINDOWS_FUNCTION(user32_module, FlashWindowEx);
}

static BOOL flash_window_ex(DWORD dwFlags, UINT uCount, DWORD dwTimeout)
{
    if (p_FlashWindowEx) {
	FLASHWINFO fi;
	fi.cbSize = sizeof(fi);
	fi.hwnd = hwnd;
	fi.dwFlags = dwFlags;
	fi.uCount = uCount;
	fi.dwTimeout = dwTimeout;
	return (*p_FlashWindowEx)(&fi);
    }
    else
	return FALSE; /* shrug */
}

static void flash_window(int mode);
static long next_flash;
static int flashing = 0;

/*
 * Timer for platforms where we must maintain window flashing manually
 * (e.g., Win95).
 */
static void flash_window_timer(void *ctx, long now)
{
    if (flashing && now - next_flash >= 0) {
	flash_window(1);
    }
}

/*
 * Manage window caption / taskbar flashing, if enabled.
 * 0 = stop, 1 = maintain, 2 = start
 */
static void flash_window(int mode)
{
    if ((mode == 0) || (cfg.beep_ind == B_IND_DISABLED)) {
	/* stop */
	if (flashing) {
	    flashing = 0;
	    if (p_FlashWindowEx)
		flash_window_ex(FLASHW_STOP, 0, 0);
	    else
		FlashWindow(hwnd, FALSE);
	}

    } else if (mode == 2) {
	/* start */
	if (!flashing) {
	    flashing = 1;
	    if (p_FlashWindowEx) {
		/* For so-called "steady" mode, we use uCount=2, which
		 * seems to be the traditional number of flashes used
		 * by user notifications (e.g., by Explorer).
		 * uCount=0 appears to enable continuous flashing, per
		 * "flashing" mode, although I haven't seen this
		 * documented. */
		flash_window_ex(FLASHW_ALL | FLASHW_TIMER,
				(cfg.beep_ind == B_IND_FLASH ? 0 : 2),
				0 /* system cursor blink rate */);
		/* No need to schedule timer */
	    } else {
		FlashWindow(hwnd, TRUE);
		next_flash = schedule_timer(450, flash_window_timer, hwnd);
	    }
	}

    } else if ((mode == 1) && (cfg.beep_ind == B_IND_FLASH)) {
	/* maintain */
	if (flashing && !p_FlashWindowEx) {
	    FlashWindow(hwnd, TRUE);	/* toggle */
	    next_flash = schedule_timer(450, flash_window_timer, hwnd);
	}
    }
}

/*
 * Beep.
 */
void do_beep(void *frontend, int mode)
{
    if (mode == BELL_DEFAULT) {
	/*
	 * For MessageBeep style bells, we want to be careful of
	 * timing, because they don't have the nice property of
	 * PlaySound bells that each one cancels the previous
	 * active one. So we limit the rate to one per 50ms or so.
	 */
	static long lastbeep = 0;
	long beepdiff;

	beepdiff = GetTickCount() - lastbeep;
	if (beepdiff >= 0 && beepdiff < 50)
	    return;
	MessageBeep(MB_OK);
	/*
	 * The above MessageBeep call takes time, so we record the
	 * time _after_ it finishes rather than before it starts.
	 */
	lastbeep = GetTickCount();
    } else if (mode == BELL_WAVEFILE) {
	if (!PlaySound(cfg.bell_wavefile.path, NULL,
		       SND_ASYNC | SND_FILENAME)) {
	    char buf[sizeof(cfg.bell_wavefile.path) + 80];
	    char otherbuf[100];
	    sprintf(buf, "Unable to play sound file\n%s\n"
		    "Using default sound instead", cfg.bell_wavefile.path);
	    sprintf(otherbuf, "%.70s Sound Error", appname);
	    MessageBox(hwnd, buf, otherbuf,
		       MB_OK | MB_ICONEXCLAMATION);
	    cfg.beep = BELL_DEFAULT;
	}
    } else if (mode == BELL_PCSPEAKER) {
	static long lastbeep = 0;
	long beepdiff;

	beepdiff = GetTickCount() - lastbeep;
	if (beepdiff >= 0 && beepdiff < 50)
	    return;

	/*
	 * We must beep in different ways depending on whether this
	 * is a 95-series or NT-series OS.
	 */
	if(osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)
	    Beep(800, 100);
	else
	    MessageBeep(-1);
	lastbeep = GetTickCount();
    }
    /* Otherwise, either visual bell or disabled; do nothing here */
    if (!term->has_focus) {
#ifdef PERSOPORT
	if( VisibleFlag!=VISIBLE_TRAY ) {
		if( cfg.foreground_on_bell ) {					// Tester avec   sleep 4 ; echo -e '\a'
			if( IsIconic(hwnd) ) SwitchToThisWindow( hwnd, TRUE ) ; 
			else SetForegroundWindow( MainHwnd ) ;
			}
		else { 
			//if( IsIconic(hwnd) && (mode == BELL_VISUAL) ) 
			if( mode == BELL_VISUAL ) {
				if( IsIconic(hwnd) ) 
					FlashWindow(hwnd, TRUE) ;
				else 
					flash_window(2) ; 
				}
			}
	} else if( VisibleFlag==VISIBLE_TRAY ) {
		if( cfg.foreground_on_bell ) { SendMessage( MainHwnd, WM_COMMAND, IDM_FROMTRAY, 0 ); }
		else if(mode == BELL_VISUAL) SetTimer(hwnd, TIMER_BLINKTRAYICON, (int)500, NULL) ;
		//SendMessage( MainHwnd, WM_COMMAND, IDM_FROMTRAY, 0 );
		//flash_window(2);	       /* start */
	    	//ShowWindow( MainHwnd, SW_MINIMIZE);
	} else
#endif
	flash_window(2);	       /* start */
    }
}

/*
 * Minimise or restore the window in response to a server-side
 * request.
 */
void set_iconic(void *frontend, int iconic)
{
    if (IsIconic(hwnd)) {
	if (!iconic)
	    ShowWindow(hwnd, SW_RESTORE);
    } else {
	if (iconic)
	    ShowWindow(hwnd, SW_MINIMIZE);
    }
}

/*
 * Move the window in response to a server-side request.
 */
void move_window(void *frontend, int x, int y)
{
    if (cfg.resize_action == RESIZE_DISABLED || 
        cfg.resize_action == RESIZE_FONT ||
	IsZoomed(hwnd))
       return;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

/*
 * Move the window to the top or bottom of the z-order in response
 * to a server-side request.
 */
void set_zorder(void *frontend, int top)
{
    if (cfg.alwaysontop)
	return;			       /* ignore */
    SetWindowPos(hwnd, top ? HWND_TOP : HWND_BOTTOM, 0, 0, 0, 0,
		 SWP_NOMOVE | SWP_NOSIZE);
}

/*
 * Refresh the window in response to a server-side request.
 */
void refresh_window(void *frontend)
{
    InvalidateRect(hwnd, NULL, TRUE);
}

/*
 * Maximise or restore the window in response to a server-side
 * request.
 */
void set_zoomed(void *frontend, int zoomed)
{
    if (IsZoomed(hwnd)) {
        if (!zoomed)
	    ShowWindow(hwnd, SW_RESTORE);
    } else {
	if (zoomed)
	    ShowWindow(hwnd, SW_MAXIMIZE);
    }
}

/*
 * Report whether the window is iconic, for terminal reports.
 */
int is_iconic(void *frontend)
{
    return IsIconic(hwnd);
}

/*
 * Report the window's position, for terminal reports.
 */
void get_window_pos(void *frontend, int *x, int *y)
{
    RECT r;
    GetWindowRect(hwnd, &r);
    *x = r.left;
    *y = r.top;
}

/*
 * Report the window's pixel size, for terminal reports.
 */
void get_window_pixels(void *frontend, int *x, int *y)
{
    RECT r;
    GetWindowRect(hwnd, &r);
    *x = r.right - r.left;
    *y = r.bottom - r.top;
}

/*
 * Return the window or icon title.
 */
char *get_window_title(void *frontend, int icon)
{
    return icon ? icon_name : window_name;
}

/*
 * See if we're in full-screen mode.
 */
static int is_full_screen()
{
    if (!IsZoomed(hwnd))
	return FALSE;
    if (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CAPTION)
	return FALSE;
    return TRUE;
}

/* Get the rect/size of a full screen window using the nearest available
 * monitor in multimon systems; default to something sensible if only
 * one monitor is present. */
static int get_fullscreen_rect(RECT * ss)
{
#if defined(MONITOR_DEFAULTTONEAREST) && !defined(NO_MULTIMON)
	HMONITOR mon;
	MONITORINFO mi;
	mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(mon, &mi);

	/* structure copy */
	*ss = mi.rcMonitor;
	return TRUE;
#else
/* could also use code like this:
	ss->left = ss->top = 0;
	ss->right = GetSystemMetrics(SM_CXSCREEN);
	ss->bottom = GetSystemMetrics(SM_CYSCREEN);
*/ 
	return GetClientRect(GetDesktopWindow(), ss);
#endif
}


/*
 * Go full-screen. This should only be called when we are already
 * maximised.
 */
static void make_full_screen()
{
    DWORD style;
	RECT ss;

    assert(IsZoomed(hwnd));

	if (is_full_screen())
		return;
	
    /* Remove the window furniture. */
    style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_BORDER | WS_THICKFRAME);
    if (cfg.scrollbar_in_fullscreen)
	style |= WS_VSCROLL;
    else
	style &= ~WS_VSCROLL;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    /* Resize ourselves to exactly cover the nearest monitor. */
	get_fullscreen_rect(&ss);
    SetWindowPos(hwnd, HWND_TOP, ss.left, ss.top,
			ss.right - ss.left,
			ss.bottom - ss.top,
			SWP_FRAMECHANGED);

    /* We may have changed size as a result */

    reset_window(0);

    /* Tick the menu item in the System and context menus. */
    {
	int i;
	for (i = 0; i < lenof(popup_menus); i++)
	    CheckMenuItem(popup_menus[i].menu, IDM_FULLSCREEN, MF_CHECKED);
    }
}

/*
 * Clear the full-screen attributes.
 */
static void clear_full_screen()
{
    DWORD oldstyle, style;

    /* Reinstate the window furniture. */
    style = oldstyle = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_BORDER;
    if (cfg.resize_action == RESIZE_DISABLED)
        style &= ~WS_THICKFRAME;
    else
        style |= WS_THICKFRAME;
    if (cfg.scrollbar)
	style |= WS_VSCROLL;
    else
	style &= ~WS_VSCROLL;
    if (style != oldstyle) {
	SetWindowLongPtr(hwnd, GWL_STYLE, style);
	SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
		     SWP_FRAMECHANGED);
    }

    /* Untick the menu item in the System and context menus. */
    {
	int i;
	for (i = 0; i < lenof(popup_menus); i++)
	    CheckMenuItem(popup_menus[i].menu, IDM_FULLSCREEN, MF_UNCHECKED);
    }
}

/*
 * Toggle full-screen mode.
 */
static void flip_full_screen()
{
    if (is_full_screen()) {
	ShowWindow(hwnd, SW_RESTORE);
    } else if (IsZoomed(hwnd)) {
	make_full_screen();
    } else {
	SendMessage(hwnd, WM_FULLSCR_ON_MAX, 0, 0);
	ShowWindow(hwnd, SW_MAXIMIZE);
    }
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag&&(!PuttyFlag)&&(cfg.bg_image_abs_fixed==1)&&(cfg.bg_type!=0) ) RefreshBackground( hwnd ) ;
#endif
}

void frontend_keypress(void *handle)
{
    /*
     * Keypress termination in non-Close-On-Exit mode is not
     * currently supported in PuTTY proper, because the window
     * always has a perfectly good Close button anyway. So we do
     * nothing here.
     */
    return;
}

int from_backend(void *frontend, int is_stderr, const char *data, int len)
{
#ifdef PERSOPORT
	int res = term_data(term, is_stderr, data, len) ;
	if( (!PuttyFlag) && (ScriptFileContent!=NULL) ) ManageInitScript( data, len ) ;
	return res ;
#else
    return term_data(term, is_stderr, data, len);
#endif
}

int from_backend_untrusted(void *frontend, const char *data, int len)
{
    return term_data_untrusted(term, data, len);
}

int get_userpass_input(prompts_t *p, unsigned char *in, int inlen)
{
    int ret;
    ret = cmdline_get_passwd_input(p, in, inlen);
    if (ret == -1)
	ret = term_get_userpass_input(term, p, in, inlen);
    return ret;
}

void agent_schedule_callback(void (*callback)(void *, void *, int),
			     void *callback_ctx, void *data, int len)
{
    struct agent_callback *c = snew(struct agent_callback);
    c->callback = callback;
    c->callback_ctx = callback_ctx;
    c->data = data;
    c->len = len;
    PostMessage(hwnd, WM_AGENT_CALLBACK, 0, (LPARAM)c);
}
