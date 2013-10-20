/*
 * PLinkw - a Windows command-line (stdin/stdout) variant of PuTTY.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <malloc.h>

#define PUTTY_DO_GLOBALS	       /* actually _define_ globals */
#include "putty.h"
#include "storage.h"
#include "tree234.h"

#define APPNAME "plinkw"

#define WM_AGENT_CALLBACK (WM_APP + 4)

struct agent_callback {
    void (*callback)(void *, void *, int);
    void *callback_ctx;
    void *data;
    int len;
};

static HANDLE abort_event = NULL;
static int cmdline_error_occured = 0;

void fatalbox(char *p, ...)
{
    va_list ap;
    fprintf(stderr, "FATAL ERROR: ");
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    if (logctx) {
        log_free(logctx);
        logctx = NULL;
    }
    cleanup_exit(1);
}
void modalfatalbox(char *p, ...)
{
    va_list ap;
    fprintf(stderr, "FATAL ERROR: ");
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    if (logctx) {
        log_free(logctx);
        logctx = NULL;
    }
    cleanup_exit(1);
}
void nonfatal(char *p, ...)
{
  va_list ap;
  fprintf(stderr, "ERROR: ");
  va_start(ap, p);
  vfprintf(stderr, p, ap);
  va_end(ap);
  fputc('\n', stderr);
  if (logctx) {
    log_free(logctx);
    logctx = NULL;
  }
}
void connection_fatal(void *frontend, char *p, ...)
{
    va_list ap;
    fprintf(stderr, "FATAL ERROR: ");
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    if (logctx) {
        log_free(logctx);
        logctx = NULL;
    }
    cleanup_exit(1);
}
void cmdline_error(char *p, ...)
{
    va_list ap;
    fprintf(stderr, APPNAME ": ");
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    cmdline_error_occured = 1;
}

HANDLE inhandle, outhandle, errhandle;
struct handle *stdin_handle, *stdout_handle, *stderr_handle;
DWORD orig_console_mode;
int connopen;

WSAEVENT netevent;

static Backend *back;
static void *backhandle;
static Conf *conf;

int term_ldisc(Terminal *term, int mode)
{
    return FALSE;
}
void ldisc_update(void *frontend, int echo, int edit)
{
    /* Update stdin read mode to reflect changes in line discipline. */
    DWORD mode;

    mode = ENABLE_PROCESSED_INPUT;
    if (echo)
	mode = mode | ENABLE_ECHO_INPUT;
    else
	mode = mode & ~ENABLE_ECHO_INPUT;
    if (edit)
	mode = mode | ENABLE_LINE_INPUT;
    else
	mode = mode & ~ENABLE_LINE_INPUT;
    SetConsoleMode(inhandle, mode);
}

char *get_ttymode(void *frontend, const char *mode) { return NULL; }

int from_backend(void *frontend_handle, int is_stderr,
		 const char *data, int len)
{
    if (is_stderr) {
	handle_write(stderr_handle, data, len);
    } else {
	handle_write(stdout_handle, data, len);
    }

    return handle_backlog(stdout_handle) + handle_backlog(stderr_handle);
}

int from_backend_untrusted(void *frontend_handle, const char *data, int len)
{
    /*
     * No "untrusted" output should get here (the way the code is
     * currently, it's all diverted by FLAG_STDERR).
     */
    assert(!"Unexpected call to from_backend_untrusted()");
    return 0; /* not reached */
}

int from_backend_eof(void *frontend_handle)
{
    handle_write_eof(stdout_handle);
    return FALSE;   /* do not respond to incoming EOF with outgoing */
}

int get_userpass_input(prompts_t *p, unsigned char *in, int inlen)
{
    int ret;
    ret = cmdline_get_passwd_input(p, in, inlen);
    if (ret == -1)
	ret = console_get_userpass_input(p, in, inlen);
    return ret;
}

static DWORD main_thread_id;

void agent_schedule_callback(void (*callback)(void *, void *, int),
			     void *callback_ctx, void *data, int len)
{
    struct agent_callback *c = snew(struct agent_callback);
    c->callback = callback;
    c->callback_ctx = callback_ctx;
    c->data = data;
    c->len = len;
    PostThreadMessage(main_thread_id, WM_AGENT_CALLBACK, 0, (LPARAM)c);
}

/*
 *  Short description of parameters.
 */
static void usage(void)
{
    printf("PuTTY Link: command-line connection utility without console window\n");
    printf("%s\n", ver);
    printf("Usage: " APPNAME " [options] [user@]host [command]\n");
    printf("       (\"host\" can also be a PuTTY saved session name)\n");
    printf("Options:\n");
    printf("  -V        print version information and exit\n");
    printf("  -pgpfp    print PGP key fingerprints and exit\n");
    printf("  -v        show verbose messages\n");
    printf("  -load sessname  Load settings from saved session\n");
    printf("  -ssh -telnet -rlogin -raw -serial\n");
    printf("            force use of a particular protocol\n");
    printf("  -P port   connect to specified port\n");
    printf("  -l user   connect with specified username\n");
    printf("  -batch    disable all interactive prompts\n");
    printf("The following options only apply to SSH connections:\n");
    printf("  -pw passw login with specified password\n");
    printf("  -D [listen-IP:]listen-port\n");
    printf("            Dynamic SOCKS-based port forwarding\n");
    printf("  -L [listen-IP:]listen-port:host:port\n");
    printf("            Forward local port to remote address\n");
    printf("  -R [listen-IP:]listen-port:host:port\n");
    printf("            Forward remote port to local address\n");
    printf("  -X -x     enable / disable X11 forwarding\n");
    printf("  -A -a     enable / disable agent forwarding\n");
    printf("  -t -T     enable / disable pty allocation\n");
    printf("  -1 -2     force use of particular protocol version\n");
    printf("  -4 -6     force use of IPv4 or IPv6\n");
    printf("  -C        enable compression\n");
    printf("  -i key    private key file for authentication\n");
    printf("  -noagent  disable use of Pageant\n");
    printf("  -agent    enable use of Pageant\n");
    printf("  -m file   read remote command(s) from file\n");
    printf("  -s        remote command is an SSH subsystem (SSH-2 only)\n");
    printf("  -N        don't start a shell/command (SSH-2 only)\n");
    printf("  -nc host:port\n");
    printf("            open tunnel in place of session (SSH-2 only)\n");
    printf("  -sercfg configuration-string (e.g. 19200,8,n,1,X)\n");
    printf("            Specify the serial configuration (serial only)\n");
}

static void version(void)
{
    printf(APPNAME ": %s\n", ver);
}

char *do_select(SOCKET skt, int startup)
{
    int events;
    if (startup) {
	events = (FD_CONNECT | FD_READ | FD_WRITE |
		  FD_OOB | FD_CLOSE | FD_ACCEPT);
    } else {
	events = 0;
    }
    if (p_WSAEventSelect(skt, netevent, events) == SOCKET_ERROR) {
	switch (p_WSAGetLastError()) {
	  case WSAENETDOWN:
	    return "Network is down";
	  default:
	    return "WSAEventSelect(): unknown error";
	}
    }
    return NULL;
}

int stdin_gotdata(struct handle *h, void *data, int len)
{
    if (len < 0) {
	/*
	 * Special case: report read error.
	 */
	char buf[4096];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, -len, 0,
		      buf, lenof(buf), NULL);
	buf[lenof(buf)-1] = '\0';
	if (buf[strlen(buf)-1] == '\n')
	    buf[strlen(buf)-1] = '\0';
	fprintf(stderr, "Unable to read from standard input: %s\n", buf);
	cleanup_exit(0);
    }
    noise_ultralight(len);
    if (connopen && back->connected(backhandle)) {
	if (len > 0) {
	    return back->send(backhandle, data, len);
	} else {
	    back->special(backhandle, TS_EOF);
	    return 0;
	}
    } else
	return 0;
}

void stdouterr_sent(struct handle *h, int new_backlog)
{
    if (new_backlog < 0) {
	/*
	 * Special case: report write error.
	 */
	char buf[4096];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, -new_backlog, 0,
		      buf, lenof(buf), NULL);
	buf[lenof(buf)-1] = '\0';
	if (buf[strlen(buf)-1] == '\n')
	    buf[strlen(buf)-1] = '\0';
	fprintf(stderr, "Unable to write to standard %s: %s\n",
		(h == stdout_handle ? "output" : "error"), buf);
	cleanup_exit(0);
    }
    if (connopen && back->connected(backhandle)) {
	back->unthrottle(backhandle, (handle_backlog(stdout_handle) +
				      handle_backlog(stderr_handle)));
    }
}

void handle_cleanup() {
    if (stdout_handle != NULL) {
        handle_free(stdout_handle);
        stdout_handle = NULL;
    }
    if (stderr_handle != NULL) {
        handle_free(stderr_handle);
        stderr_handle = NULL;
    }
}

extern int use_inifile;
extern char inifile[2 * MAX_PATH + 10];
static int auto_restart = 0;

int initialize_main(int argc, char **argv)
{
    int sending;
    int portnumber = -1;
    SOCKET *sklist;
    int skcount, sksize;
    int exitcode;
    int errors;
    int got_host = FALSE;
    int use_subsystem = 0;
    unsigned long now, next, then;

    sklist = NULL;
    skcount = sksize = 0;
    /*
     * Initialise port and protocol to sensible defaults. (These
     * will be overridden by more or less anything.)
     */
    default_protocol = PROT_SSH;
    default_port = 22;

    flags = FLAG_STDERR;
    /*
     * Process the command line.
     */
    conf = conf_new();
    do_defaults(NULL, conf);
    loaded_session = FALSE;
    default_protocol = conf_get_int(conf, CONF_protocol);
    default_port = conf_get_int(conf, CONF_port);
    errors = 0;
    {
	/*
	 * Override the default protocol if PLINK_PROTOCOL is set.
	 */
	char *p = getenv("PLINK_PROTOCOL");
	if (p) {
	    const Backend *b = backend_from_name(p);
	    if (b) {
		default_protocol = b->protocol;
		default_port = b->default_port;
		conf_set_int(conf, CONF_protocol, default_protocol);
		conf_set_int(conf, CONF_port, default_port);
	    }
	}
    }
    while (argc > 1) {
        if (!use_inifile && argc > 2 && !strcmp(argv[1], "-ini") && *(argv[2]) != '\0') {
            char* dummy;
            DWORD attributes;
            GetFullPathName(argv[2], sizeof inifile, inifile, &dummy);
            attributes = GetFileAttributes(inifile);
            if (attributes != 0xFFFFFFFF && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                HANDLE handle = CreateFile(inifile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(handle);
                    use_inifile = 1;
                    argc -= 2;
                    argv += 2;
                }
            }
        } else if (!auto_restart && argc > 1 && !strcmp(argv[1], "-autoretry")) {
            auto_restart = 1;
            argc--;
            argv++;
        } else {
            break;
        }
    }
    while (--argc) {
	char *p = *++argv;
	if (*p == '-') {
	    int ret = cmdline_process_param(p, (argc > 1 ? argv[1] : NULL),
					    1, conf);
            if (cmdline_error_occured)
                return FALSE;
	    if (ret == -2) {
		fprintf(stderr,
			APPNAME ": option \"%s\" requires an argument\n", p);
		errors = 1;
	    } else if (ret == 2) {
		--argc, ++argv;
	    } else if (ret == 1) {
		continue;
	    } else if (!strcmp(p, "-batch")) {
		console_batch_mode = 1;
	    } else if (!strcmp(p, "-s")) {
		/* Save status to write to conf later. */
		use_subsystem = 1;
	    } else if (!strcmp(p, "-V") || !strcmp(p, "--version")) {
                version();
	    } else if (!strcmp(p, "--help")) {
                usage();
                return FALSE;
            } else if (!strcmp(p, "-pgpfp")) {
                pgp_fingerprints();
                return FALSE;
	    } else {
		fprintf(stderr, APPNAME ": unknown option \"%s\"\n", p);
		errors = 1;
	    }
	} else if (*p) {
	    if (!conf_launchable(conf) || !(got_host || loaded_session)) {
		char *q = p;
		/*
		 * If the hostname starts with "telnet:", set the
		 * protocol to Telnet and process the string as a
		 * Telnet URL.
		 */
		if (!strncmp(q, "telnet:", 7)) {
		    char c;

		    q += 7;
		    if (q[0] == '/' && q[1] == '/')
			q += 2;
		    conf_set_int(conf, CONF_protocol, PROT_TELNET);
		    p = q;
		    while (*p && *p != ':' && *p != '/')
			p++;
		    c = *p;
		    if (*p)
			*p++ = '\0';
		    if (c == ':')
			conf_set_int(conf, CONF_port, atoi(p));
		    else
			conf_set_int(conf, CONF_port, -1);
		    conf_set_str(conf, CONF_host, q);
		    got_host = TRUE;
		} else {
		    char *r, *user, *host;
		    /*
		     * Before we process the [user@]host string, we
		     * first check for the presence of a protocol
		     * prefix (a protocol name followed by ",").
		     */
		    r = strchr(p, ',');
		    if (r) {
			const Backend *b;
			*r = '\0';
			b = backend_from_name(p);
			if (b) {
			    default_protocol = b->protocol;
			    conf_set_int(conf, CONF_protocol,
					 default_protocol);
			    portnumber = b->default_port;
			}
			p = r + 1;
		    }

		    /*
		     * A nonzero length string followed by an @ is treated
		     * as a username. (We discount an _initial_ @.) The
		     * rest of the string (or the whole string if no @)
		     * is treated as a session name and/or hostname.
		     */
		    r = strrchr(p, '@');
		    if (r == p)
			p++, r = NULL; /* discount initial @ */
		    if (r) {
			*r++ = '\0';
			user = p, host = r;
		    } else {
			user = NULL, host = p;
		    }

		    /*
		     * Now attempt to load a saved session with the
		     * same name as the hostname.
		     */
		    {
			Conf *conf2 = conf_new();
			do_defaults(host, conf2);
			if (loaded_session || !conf_launchable(conf2)) {
			    /* No settings for this host; use defaults */
			    /* (or session was already loaded with -load) */
			    conf_set_str(conf, CONF_host, host);
			    conf_set_int(conf, CONF_port, default_port);
			    got_host = TRUE;
			} else {
			    conf_copy_into(conf, conf2);
			    loaded_session = TRUE;
			}
			conf_free(conf2);
		    }

		    if (user) {
			/* Patch in specified username. */
			conf_set_str(conf, CONF_username, user);
		    }

		}
	    } else {
		char *command;
		int cmdlen, cmdsize;
		cmdlen = cmdsize = 0;
		command = NULL;

		while (argc) {
		    while (*p) {
			if (cmdlen >= cmdsize) {
			    cmdsize = cmdlen + 512;
			    command = sresize(command, cmdsize, char);
			}
			command[cmdlen++]=*p++;
		    }
		    if (cmdlen >= cmdsize) {
			cmdsize = cmdlen + 512;
			command = sresize(command, cmdsize, char);
		    }
		    command[cmdlen++]=' '; /* always add trailing space */
		    if (--argc) p = *++argv;
		}
		if (cmdlen) command[--cmdlen]='\0';
				       /* change trailing blank to NUL */
		conf_set_str(conf, CONF_remote_cmd, command);
		conf_set_str(conf, CONF_remote_cmd2, "");
		conf_set_int(conf, CONF_nopty, TRUE);  /* command => no tty */

		break;		       /* done with cmdline */
	    }
	}
    }

    if (errors)
	return FALSE;

    if (!conf_launchable(conf) || !(got_host || loaded_session)) {
	usage();
	return FALSE;
    }

    /*
     * Muck about with the hostname in various ways.
     */
    {
	char *hostbuf = dupstr(conf_get_str(conf, CONF_host));
	char *host = hostbuf;
	char *p, *q;

	/*
	 * Trim leading whitespace.
	 */
	host += strspn(host, " \t");

	/*
	 * See if host is of the form user@host, and separate out
	 * the username if so.
	 */
	if (host[0] != '\0') {
	    char *atsign = strrchr(host, '@');
	    if (atsign) {
		*atsign = '\0';
		conf_set_str(conf, CONF_username, host);
		host = atsign + 1;
	    }
	}

	/*
	 * Trim off a colon suffix if it's there.
	 */
	host[strcspn(host, ":")] = '\0';

	/*
	 * Remove any remaining whitespace.
	 */
	p = hostbuf;
	q = host;
	while (*q) {
	    if (*q != ' ' && *q != '\t')
		*p++ = *q;
	    q++;
	}
	*p = '\0';

	conf_set_str(conf, CONF_host, hostbuf);
	sfree(hostbuf);
    }

    /*
     * Perform command-line overrides on session configuration.
     */
    cmdline_run_saved(conf);

    /*
     * Apply subsystem status.
     */
    if (use_subsystem)
        conf_set_int(conf, CONF_ssh_subsys, TRUE);

    if (!*conf_get_str(conf, CONF_remote_cmd) &&
	!*conf_get_str(conf, CONF_remote_cmd2) &&
	!*conf_get_str(conf, CONF_ssh_nc_host))
	flags |= FLAG_INTERACTIVE;

    /*
     * Select protocol. This is farmed out into a table in a
     * separate file to enable an ssh-free variant.
     */
    back = backend_from_proto(conf_get_int(conf, CONF_protocol));
    if (back == NULL) {
	fprintf(stderr,
		"Internal fault: Unsupported protocol found\n");
	return 1;
    }

    /*
     * Select port.
     */
    if (portnumber != -1)
	conf_set_int(conf, CONF_port, portnumber);

    if (conf_get_int(conf, CONF_protocol) == PROT_SSH) {
        if ((flags & FLAG_INTERACTIVE) != 0) {
            conf_set_int(conf, CONF_ssh_no_shell, TRUE);
            flags &= ~FLAG_INTERACTIVE;
        }
        conf_set_int(conf, CONF_nopty, TRUE);
    }

    return TRUE;
}

static DWORD WINAPI do_main(LPVOID arg) {
    SOCKET *sklist;
    int skcount, sksize;
    int sending;
    int connected = 0;
    int abort = 0;
    int exitcode;
    unsigned long now, next, then;

    sklist = NULL;
    skcount = sksize = 0;
    sk_init();
    if (p_WSAEventSelect == NULL) {
	fprintf(stderr, APPNAME " requires WinSock 2\n");
	return 1;
    }

    logctx = log_init(NULL, conf);
    console_provide_logctx(logctx);

    /*
     * Start up the connection.
     */
    netevent = CreateEvent(NULL, FALSE, FALSE, NULL);
    {
        void notify_basetitle(char* basetitle);
	const char *error;
	char *realhost;
	/* nodelay is only useful if stdin is a character device (console) */
	int nodelay = conf_get_int(conf, CONF_tcp_nodelay) &&
	    (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR);

	error = back->init(NULL, &backhandle, conf,
			   conf_get_str(conf, CONF_host),
			   conf_get_int(conf, CONF_port),
			   &realhost, nodelay,
			   conf_get_int(conf, CONF_tcp_keepalives));
	if (error) {
	    fprintf(stderr, "Unable to open connection:\n%s", error);
	    return 1;
	}
	back->provide_logctx(backhandle, logctx);
	notify_basetitle(dupprintf("%s - " APPNAME, realhost));
        sfree(realhost);
    }
    connopen = 1;

    inhandle = GetStdHandle(STD_INPUT_HANDLE);
    outhandle = GetStdHandle(STD_OUTPUT_HANDLE);
    errhandle = GetStdHandle(STD_ERROR_HANDLE);

    /*
     * Turn off ECHO and LINE input modes. We don't care if this
     * call fails, because we know we aren't necessarily running in
     * a console.
     */
    GetConsoleMode(inhandle, &orig_console_mode);
    SetConsoleMode(inhandle, ENABLE_PROCESSED_INPUT);

    /*
     * Pass the output handles to the handle-handling subsystem.
     * (The input one we leave until we're through the
     * authentication process.)
     */
    stdout_handle = handle_output_new(outhandle, stdouterr_sent, NULL, 0);
    stderr_handle = handle_output_new(errhandle, stdouterr_sent, NULL, 0);

    main_thread_id = GetCurrentThreadId();

    sending = FALSE;

    now = GETTICKCOUNT();

    while (1) {
	int nhandles;
	HANDLE *handles;	
	int n;
	DWORD ticks;

	if (!sending && back->sendok(backhandle)) {
	    stdin_handle = handle_input_new(inhandle, stdin_gotdata, NULL,
					    0);
	    sending = TRUE;
	}

	if (run_timers(now, &next)) {
	    then = now;
	    now = GETTICKCOUNT();
	    if (now - then > next - then)
		ticks = 0;
	    else
		ticks = next - now;
	} else {
	    ticks = INFINITE;
	}

	handles = handle_get_events(&nhandles);
	handles = sresize(handles, nhandles+2, HANDLE);
	handles[nhandles] = netevent;
        handles[nhandles+1] = abort_event;
	n = MsgWaitForMultipleObjects(nhandles+2, handles, FALSE, ticks,
				      QS_POSTMESSAGE);
	if ((unsigned)(n - WAIT_OBJECT_0) < (unsigned)nhandles) {
	    handle_got_event(handles[n - WAIT_OBJECT_0]);
	} else if (n == WAIT_OBJECT_0 + nhandles) {
	    WSANETWORKEVENTS things;
	    SOCKET socket;
	    extern SOCKET first_socket(int *), next_socket(int *);
	    extern int select_result(WPARAM, LPARAM);
	    int i, socketstate;

	    /*
	     * We must not call select_result() for any socket
	     * until we have finished enumerating within the tree.
	     * This is because select_result() may close the socket
	     * and modify the tree.
	     */
	    /* Count the active sockets. */
	    i = 0;
	    for (socket = first_socket(&socketstate);
		 socket != INVALID_SOCKET;
		 socket = next_socket(&socketstate)) i++;

	    /* Expand the buffer if necessary. */
	    if (i > sksize) {
		sksize = i + 16;
		sklist = sresize(sklist, sksize, SOCKET);
	    }

	    /* Retrieve the sockets into sklist. */
	    skcount = 0;
	    for (socket = first_socket(&socketstate);
		 socket != INVALID_SOCKET;
		 socket = next_socket(&socketstate)) {
		sklist[skcount++] = socket;
	    }

	    /* Now we're done enumerating; go through the list. */
	    for (i = 0; i < skcount; i++) {
		WPARAM wp;
		socket = sklist[i];
		wp = (WPARAM) socket;
		if (!p_WSAEnumNetworkEvents(socket, NULL, &things)) {
                    static const struct { int bit, mask; } eventtypes[] = {
                        {FD_CONNECT_BIT, FD_CONNECT},
                        {FD_READ_BIT, FD_READ},
                        {FD_CLOSE_BIT, FD_CLOSE},
                        {FD_OOB_BIT, FD_OOB},
                        {FD_WRITE_BIT, FD_WRITE},
                        {FD_ACCEPT_BIT, FD_ACCEPT},
                    };
                    int e;

		    noise_ultralight(socket);
		    noise_ultralight(things.lNetworkEvents);

                    for (e = 0; e < lenof(eventtypes); e++)
                        if (things.lNetworkEvents & eventtypes[e].mask) {
                            LPARAM lp;
                            int err = things.iErrorCode[eventtypes[e].bit];
                            lp = WSAMAKESELECTREPLY(eventtypes[e].mask, err);
                            connopen &= select_result(wp, lp);
                        }
		}
	    }
	    if (!connected && back->ldisc(backhandle, LD_ECHO)) {
                void notify_completed();
		connected = 1;
		notify_completed();
	    }
	} else if (n == WAIT_OBJECT_0 + nhandles + 1) {
	    abort = 1;
            break;
	} else if (n == WAIT_OBJECT_0 + nhandles + 2) {
	    MSG msg;
	    while (PeekMessage(&msg, INVALID_HANDLE_VALUE,
			       WM_AGENT_CALLBACK, WM_AGENT_CALLBACK,
			       PM_REMOVE)) {
		struct agent_callback *c = (struct agent_callback *)msg.lParam;
		c->callback(c->callback_ctx, c->data, c->len);
		sfree(c);
	    }
	}

	if (n == WAIT_TIMEOUT) {
	    now = next;
	} else {
	    now = GETTICKCOUNT();
	}

	sfree(handles);

	if (sending)
	    handle_unthrottle(stdin_handle, back->sendbuffer(backhandle));

	if ((!connopen || !back->connected(backhandle)) &&
	    handle_backlog(stdout_handle) + handle_backlog(stderr_handle) == 0)
	    break;		       /* we closed the connection */
    }
    exitcode = back->exitcode(backhandle);
    if (!abort && exitcode < 0) {
	fprintf(stderr, "Remote process exit code unavailable\n");
	exitcode = 1;		       /* this is an error condition */
    }
    cleanup_exit(exitcode);
    return 0;			       /* placate compiler warning */
}

#define IDD_LOGFDIALOG     102
#define IDC_LOGLIST        103
#define ID_COPY           1000
#define ID_CLEAR          1001
#define ID_TERMINATE      1002
#define ID_SHOWLOG        1003
#define ID_RESTART        1004
#define ID_CANCEL_RESTART 1005

#define WM_SYSTRAY      (WM_APP + 1)
#define WM_COMPLETED    (WM_APP + 2)
#define WM_EXITED       (WM_APP + 3)
#define WM_BASETITLE    (WM_APP + 4)
#define WM_RESTART      (WM_APP + 5)
#define WM_PRINT_LOG    (WM_APP + 6)

#define TID_CLOSE   1
#define TID_ICON    2
#define TID_RESTART 3
#define TID_FLUSH   4
#define TID_HIDE    5

#define IDI_ICON  200
#define IDI_ICOND 201

// Global variables
static HINSTANCE instance = NULL;
static DWORD initial_thread_id = 0;

static int standard_handles = 0;

static int get_standard_handles_mask(DWORD type) {
    switch (type) {
    case STD_INPUT_HANDLE:
        return 1;
    case STD_OUTPUT_HANDLE:
        return 2;
    case STD_ERROR_HANDLE:
        return 4;
    default:
        return 0;
    }
}

static void set_own_standard_handle(DWORD type) {
    standard_handles |= get_standard_handles_mask(type);
}

static int is_own_standard_handle(DWORD type) {
    return (standard_handles & get_standard_handles_mask(type)) != 0;
}

struct log_dialog_struct {
    HWND dialog;
    int timer_id;
    DWORD timer_limit;
    DWORD timer_elapse;
    int last_index;
    char* line_buffer;
    int line_buffer_length;
    int line_buffer_visible;
    HMENU menu;
    HMENU traymenu;
    int completed;
    char* basetitle;
    NOTIFYICONDATA nid;
    HICON complete_icon;
    HICON incomplete_icon;
    HANDLE thread;
    DWORD exitCode;
    int restart_count;
};

static void update_systray(struct log_dialog_struct* dialog_data) {
    HICON icon;
    char tip[sizeof dialog_data->nid.szTip];
    int modified = 0;

    if (!auto_restart)
        return;

    if (dialog_data->nid.cbSize == 0) {
        dialog_data->nid.cbSize = sizeof dialog_data->nid;
        dialog_data->nid.hWnd = dialog_data->dialog;
        dialog_data->nid.uID = IDI_ICON;
        dialog_data->nid.uFlags = NIF_MESSAGE;
        dialog_data->nid.uCallbackMessage = WM_SYSTRAY;
        dialog_data->nid.hIcon = NULL;
        dialog_data->nid.szTip[0] = '\0';
        modified = 1;
    }else{
        dialog_data->nid.uFlags = 0;
    }

    icon = dialog_data->completed ? dialog_data->complete_icon : dialog_data->incomplete_icon;
    if (dialog_data->nid.hIcon != icon) {
        dialog_data->nid.hIcon = icon;
        dialog_data->nid.uFlags |= NIF_ICON;
        modified = 1;
    }

    GetWindowText(dialog_data->dialog, tip, sizeof tip);
    if (strcmp(dialog_data->nid.szTip, tip) != 0) {
        strcpy(dialog_data->nid.szTip, tip);
        dialog_data->nid.uFlags |= NIF_TIP;
        modified = 1;
    }

    if (modified
            && !Shell_NotifyIcon(NIM_MODIFY, &dialog_data->nid)
            && !Shell_NotifyIcon(NIM_ADD, &dialog_data->nid)) {
    	SetTimer(dialog_data->dialog, TID_ICON, 1000, NULL);
    }
}

static void update_window_title(struct log_dialog_struct* dialog_data) {
    const char* title;
    const char* basetitle = dialog_data->basetitle;
    const char* format = NULL;
    char* allocated = NULL;

    if (basetitle == NULL || basetitle == '\0')
        basetitle = APPNAME;

    switch (dialog_data->timer_id) {
    case TID_CLOSE:
    case TID_RESTART:
        {
            int remain = (dialog_data->timer_limit - GetTickCount()) / 1000;
            const char* format;
            if (dialog_data->timer_id == TID_CLOSE) {
                format = remain > 1 ? "%s (close automatically after %d seconds...)"
                                    : "%s (close automatically soon...)";
            } else {
                format = remain > 1 ? "%s (connect automatically after %d seconds...)"
                                    : "%s (connect automatically soon...)";
            }
            allocated = dupprintf(format, basetitle, remain);
            title = allocated;
        }
        break;
    default:
        if (!dialog_data->completed) {
            allocated = dupprintf("%s (disconnected)", basetitle);
            title = allocated;
        } else {
            title = basetitle;
        }
        break;
    }
    SetWindowText(dialog_data->dialog, title);
    sfree(allocated);
    update_systray(dialog_data);
}

static BOOL log_dialog_init_dialog(struct log_dialog_struct* dialog_data) {
    dialog_data->complete_icon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON));
    dialog_data->incomplete_icon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICOND));
    SendMessage(dialog_data->dialog, WM_SETICON, ICON_SMALL, (LPARAM) dialog_data->complete_icon);
    SendMessage(dialog_data->dialog, WM_SETICON, ICON_BIG, (LPARAM) dialog_data->complete_icon);
    update_systray(dialog_data);
    return TRUE;
}

static void log_dialog_close(struct log_dialog_struct* dialog_data) {
    if (!dialog_data->completed && !auto_restart) {
        DestroyWindow(dialog_data->dialog);
    }else{
    	ShowWindow(dialog_data->dialog, SW_HIDE);
    }
}

static void log_dialog_destroy(struct log_dialog_struct* dialog_data) {
    if (dialog_data->menu != NULL) {
	DestroyMenu(dialog_data->menu);
	dialog_data->menu = NULL;
    }
    if (dialog_data->traymenu != NULL) {
	DestroyMenu(dialog_data->traymenu);
	dialog_data->traymenu = NULL;
    }
    SetEvent(abort_event);
    if (dialog_data->nid.cbSize != 0) {
        Shell_NotifyIcon(NIM_DELETE, &dialog_data->nid);
        dialog_data->nid.cbSize = 0;
    }
}

static void log_dialog_size(struct log_dialog_struct* dialog_data, int width, int height) {
    SetWindowPos(GetDlgItem(dialog_data->dialog, IDC_LOGLIST), NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
}

static void log_dialog_print_log(struct log_dialog_struct* dialog_data, int string_length, char* string) {
    char* strings[256] = {string};
    WPARAM string_lengths[256] = {string_length};
    int count = 1;
    int i;
    WPARAM length = string_length;
    MSG msg;
    int appended = 0;
    char* start;
    char* p;
    char* buffer;
    char* data;
    KillTimer(dialog_data->dialog, TID_FLUSH);
    SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, WM_SETREDRAW, 0, 0);
    if (dialog_data->line_buffer_visible) {
        SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_DELETESTRING, dialog_data->last_index, 0);
        dialog_data->line_buffer_visible = 0;
    }
    while (count < 256 && PeekMessage(&msg, (HWND) -1, WM_PRINT_LOG, WM_PRINT_LOG, PM_REMOVE)) {
        strings[count] = (char*) msg.lParam;
        string_lengths[count] = msg.wParam;
        length += msg.wParam;
        count++;
    }
    buffer = (char*) alloca(dialog_data->line_buffer_length + length + 1);
    data = buffer + dialog_data->line_buffer_length;
    if (dialog_data->line_buffer_length > 0)
        memcpy(buffer, dialog_data->line_buffer, dialog_data->line_buffer_length);
    sfree(dialog_data->line_buffer);
    dialog_data->line_buffer = NULL;
    dialog_data->line_buffer_length = 0;
    for (i = 0, p = data; i < count; i++) {
   	memcpy(p, (void*) strings[i], string_lengths[i]);
        p += string_lengths[i];
        sfree(strings[i]);
    }
    *p = '\0';
    start = buffer;
    p = data;
    while ( *p != '\0') {
	if (*p != '\n') {
	    p++;
	}else{
	    int index;
	    *p = '\0';
            index = SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_INSERTSTRING, dialog_data->last_index, (LPARAM) start);
            if (index != LB_ERR) {
                while (SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETCOUNT, 0, 0) > 1000) {
                    SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_DELETESTRING, 0, 0);
                    index--;
                }
	        dialog_data->last_index = index + 1;
            }
            start = ++p;
            appended = 1;
        }
    }
    if (start < p) {
        dialog_data->line_buffer_length = p - start;
        dialog_data->line_buffer = snewn(dialog_data->line_buffer_length + 1, char);
        memcpy(dialog_data->line_buffer, start, dialog_data->line_buffer_length);
        dialog_data->line_buffer[dialog_data->line_buffer_length] = '\0';
        SetTimer(dialog_data->dialog, TID_FLUSH, 1000, NULL);
    }
    if (appended) {
        SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_SETCARETINDEX, dialog_data->last_index - 1, 0);
	SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, WM_SETREDRAW, TRUE, 0);
	if (!IsWindowVisible(dialog_data->dialog)) {
	    ShowWindow(dialog_data->dialog, SW_SHOW);
	    SetForegroundWindow(dialog_data->dialog);
	}
        KillTimer(dialog_data->dialog, TID_HIDE);
        SetTimer(dialog_data->dialog, TID_HIDE, 10000, NULL);
    }
}

static void log_dialog_touch(struct log_dialog_struct* dialog_data) {
    if (dialog_data->timer_elapse != 0)
        dialog_data->timer_limit = GetTickCount() + dialog_data->timer_elapse;
    KillTimer(dialog_data->dialog, TID_HIDE);
}

static void log_dialog_completed(struct log_dialog_struct* dialog_data) {
    if (!dialog_data->completed) {
        dialog_data->completed = 1;
        dialog_data->restart_count = 0;
        update_window_title(dialog_data);
    }
}

static void log_dialog_exited(struct log_dialog_struct* dialog_data) {
    if (dialog_data->completed || dialog_data->timer_id == 0) {
        dialog_data->completed = 0;
        if (!auto_restart) {
            if (!IsWindowVisible(dialog_data->dialog)) {
                DestroyWindow(dialog_data->dialog);
                return;
            }
        } else if (dialog_data->restart_count >= 5) {
            dialog_data->timer_id = 0;
            dialog_data->timer_elapse = 0;
            dialog_data->timer_limit = 0;
            KillTimer(dialog_data->dialog, TID_RESTART);
            KillTimer(dialog_data->dialog, TID_CLOSE);
            update_window_title(dialog_data);
            return;
        }
        if (dialog_data->timer_id != 0)
            return;
        dialog_data->timer_id = auto_restart ? TID_RESTART : TID_CLOSE;
        dialog_data->timer_elapse = auto_restart ? 10000 : 30000;
	dialog_data->timer_limit = GetTickCount() + dialog_data->timer_elapse;
        update_window_title(dialog_data);
        SetTimer(dialog_data->dialog, dialog_data->timer_id, 100, NULL);
    }
}

static void log_dialog_systray(struct log_dialog_struct* dialog_data, int id, int message) {
    BOOL l10nAppendMenu(HMENU menu, UINT flags, UINT id, LPCSTR text);
    if(message == WM_RBUTTONUP) {
	POINT cursorpos;
	GetCursorPos(&cursorpos);
        if (dialog_data->traymenu == NULL) {
	    dialog_data->traymenu = CreatePopupMenu();
	    l10nAppendMenu(dialog_data->traymenu, MF_ENABLED, ID_SHOWLOG, "Show &Log Window");
            AppendMenu(dialog_data->traymenu, MF_SEPARATOR, 0, 0);
	    l10nAppendMenu(dialog_data->traymenu, MF_ENABLED, ID_RESTART, "&Reconnect");
	    l10nAppendMenu(dialog_data->traymenu, MF_ENABLED, ID_CANCEL_RESTART, "Ca&ncel Reconnect");
            AppendMenu(dialog_data->traymenu, MF_SEPARATOR, 0, 0);
	    l10nAppendMenu(dialog_data->traymenu, MF_ENABLED, ID_TERMINATE, "&Terminate");
        }
        SetForegroundWindow(dialog_data->dialog);
        PostMessage(NULL, WM_NULL, 0, 0); 
	TrackPopupMenu(dialog_data->traymenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, cursorpos.x, cursorpos.y, 0, dialog_data->dialog, NULL);
    }
}

static void log_dialog_basetitle(struct log_dialog_struct* dialog_data, char* basetitle) {
    sfree(dialog_data->basetitle);
    dialog_data->basetitle = basetitle;
}

static void log_dialog_restart(struct log_dialog_struct* dialog_data) {
    if (dialog_data->thread == NULL) {
        DWORD threadid;
        dialog_data->restart_count++;
        dialog_data->thread = CreateThread(NULL, 0, do_main, NULL, 0, &threadid);
    }
}

static void log_dialog_exit_thread(struct log_dialog_struct* dialog_data) {
    if (!GetExitCodeThread(dialog_data->thread, &dialog_data->exitCode)
            || dialog_data->exitCode != STILL_ACTIVE) {
        CloseHandle(dialog_data->thread);
        dialog_data->thread = NULL;
        log_dialog_exited(dialog_data);
    }
}

static void log_dialog_timer_auto(struct log_dialog_struct* dialog_data) {
    DWORD now = GetTickCount();
    KillTimer(dialog_data->dialog, TID_HIDE);
    if (dialog_data->timer_limit <= now) {
        if (dialog_data->timer_id == TID_CLOSE)
            SendMessage(dialog_data->dialog, WM_CLOSE, 0, 0);
        else
            PostThreadMessage(initial_thread_id, WM_RESTART, 0, 0);
        dialog_data->timer_id = 0;
        dialog_data->timer_elapse = 0;
    } else {
        update_window_title(dialog_data);
        SetTimer(dialog_data->dialog, dialog_data->timer_id, 100, NULL);
    }
}

static void log_dialog_timer_icon(struct log_dialog_struct* dialog_data) {
    update_systray(dialog_data);
}

static void log_dialog_timer_flush(struct log_dialog_struct* dialog_data) {
    if (dialog_data->line_buffer_length > 0) {
        SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_DELETESTRING, dialog_data->last_index, 0);
        SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_INSERTSTRING, dialog_data->last_index, (LPARAM) dialog_data->line_buffer);
        dialog_data->line_buffer_visible = 1;
	if (!IsWindowVisible(dialog_data->dialog)) {
	    ShowWindow(dialog_data->dialog, SW_SHOW);
	    SetForegroundWindow(dialog_data->dialog);
	}
        KillTimer(dialog_data->dialog, TID_HIDE);
        SetTimer(dialog_data->dialog, TID_HIDE, 10000, NULL);
    }
}

static void log_dialog_timer_hide(struct log_dialog_struct* dialog_data) {
    SendMessage(dialog_data->dialog, WM_CLOSE, 0, 0);
}

static void log_dialog_context_menu(struct log_dialog_struct* dialog_data, HWND ctrl, int xPos, int yPos) {
    BOOL l10nAppendMenu(HMENU menu, UINT flags, UINT id, LPCSTR text);
    if (GetDlgCtrlID(ctrl) == IDC_LOGLIST) {
	int result;
	POINT pt = {xPos, yPos};
	ScreenToClient(ctrl, &pt);
	result = SendMessage(ctrl, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
	if (HIWORD(result) == 0) {
	    if (SendMessage(ctrl, LB_GETSEL, LOWORD(result), 0) <= 0) {
		SendMessage(ctrl, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, 0xffff));
		SendMessage(ctrl, LB_SETSEL, TRUE, LOWORD(result));
	    }
	}else{
	    SendMessage(ctrl, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, 0xffff));
	}
	if (dialog_data->menu == NULL) {
	    dialog_data->menu = CreatePopupMenu();
	    l10nAppendMenu(dialog_data->menu, MF_ENABLED, ID_COPY, "&Copy");
	    l10nAppendMenu(dialog_data->menu, MF_ENABLED, ID_CLEAR, "C&lear");
            AppendMenu(dialog_data->menu, MF_SEPARATOR, 0, 0);
            if (auto_restart) {
	        l10nAppendMenu(dialog_data->menu, MF_ENABLED, ID_RESTART, "&Reconnect");
	        l10nAppendMenu(dialog_data->menu, MF_ENABLED, ID_CANCEL_RESTART, "Ca&ncel Reconnect");
                AppendMenu(dialog_data->menu, MF_SEPARATOR, 0, 0);
	    }
	    l10nAppendMenu(dialog_data->menu, MF_ENABLED, ID_TERMINATE, "&Terminate");
	}
	TrackPopupMenu(dialog_data->menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, xPos, yPos, 0, dialog_data->dialog, NULL);
    }
}

static void log_dialog_initmenu_popup(struct log_dialog_struct* dialog_data, HMENU menu, UINT position, BOOL systemMenu) {
    if (systemMenu)
        return;
    EnableMenuItem(menu, ID_COPY, MF_BYCOMMAND | (SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETSELCOUNT, 0, 0) > 0 ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, ID_CLEAR, MF_BYCOMMAND | (SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETCOUNT, 0, 0) > 0 ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, ID_TERMINATE, MF_BYCOMMAND | ((dialog_data->completed || auto_restart) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, ID_SHOWLOG, MF_BYCOMMAND | (!IsWindowVisible(dialog_data->dialog) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, ID_RESTART, MF_BYCOMMAND | ((!dialog_data->completed && auto_restart) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, ID_CANCEL_RESTART, MF_BYCOMMAND | ((!dialog_data->completed && auto_restart && dialog_data->timer_id != 0) ? MF_ENABLED : MF_GRAYED));
}

static void log_dialog_command_copy(struct log_dialog_struct* dialog_data) {
    int count = SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETSELCOUNT, 0, 0);
    if (count > 0) {
	int i;
	int length = 0;
	int* items = (int*) alloca(sizeof (int) * count);
	SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETSELITEMS, count, (LPARAM) items);
	for (i = 0; i < count; i++) {
	    length += SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETTEXTLEN, items[i], 0) + 2;
	}
	if (OpenClipboard(dialog_data->dialog)) {
	    if (EmptyClipboard()) {
		HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, length + 1);
		if (data != NULL){
		    char* buffer = (char*) GlobalLock(data);
		    char* dst = buffer;
		    for (i = 0; i < count; i++) {
			int len = SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETTEXTLEN, items[i], 0);
			SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_GETTEXT, items[i], (LPARAM) dst);
			dst[len] = '\r';
			dst[len + 1] = '\n';
			dst += len + 2;
		    }
		    *dst = '\0';
		    GlobalUnlock(data);
		    if (SetClipboardData(CF_TEXT, data) == NULL)
			GlobalFree(data);
		}
	    }
	    CloseClipboard();
	}
    }
}

static void log_dialog_command_clear(struct log_dialog_struct* dialog_data) {
    SendDlgItemMessage(dialog_data->dialog, IDC_LOGLIST, LB_RESETCONTENT, 0, 0);
    dialog_data->last_index = 0;
}

static void log_dialog_command_terminate(struct log_dialog_struct* dialog_data) {
    if (dialog_data->completed) {
        auto_restart = 0;
        SetEvent(abort_event);
    }else{
        DestroyWindow(dialog_data->dialog);
    }
}

static void log_dialog_command_showlog(struct log_dialog_struct* dialog_data) {
    ShowWindow(dialog_data->dialog, SW_SHOW);
}

static void log_dialog_command_restart(struct log_dialog_struct* dialog_data) {
    if (!dialog_data->completed && auto_restart) {
        if (dialog_data->timer_id == TID_RESTART) {
            KillTimer(dialog_data->dialog, TID_RESTART);
            dialog_data->timer_elapse = 0;
            dialog_data->timer_id = 0;
        } else {
            dialog_data->restart_count = 0;
        }
        PostThreadMessage(initial_thread_id, WM_RESTART, 0, 0);
    }
}

static void log_dialog_command_cancel_restart(struct log_dialog_struct* dialog_data) {
    if (!dialog_data->completed && auto_restart && dialog_data->timer_id == TID_RESTART) {
        KillTimer(dialog_data->dialog, TID_RESTART);
        dialog_data->timer_elapse = 0;
        dialog_data->timer_id = 0;
        update_window_title(dialog_data);
    }
}

static void log_dialog_taskbar_created(struct log_dialog_struct* dialog_data) {
    dialog_data->nid.cbSize = 0;
    update_systray(dialog_data);
}

void notify_completed() {
    PostThreadMessage(initial_thread_id, WM_COMPLETED, 0, 0);
}

void notify_exited() {
    PostThreadMessage(initial_thread_id, WM_EXITED, 0, 0);
}

void notify_basetitle(char* basetitle) {
    PostThreadMessage(initial_thread_id, WM_BASETITLE, 0, (LPARAM) basetitle);
}

static BOOL CALLBACK log_dialog_proc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    struct log_dialog_struct* dialog_data = (struct log_dialog_struct*) GetWindowLongPtr(dialog, DWLP_USER);
    static UINT WM_TASKBARCREATED = 0;

    if (WM_TASKBARCREATED == 0)
	WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");

    switch (message) {
    case WM_INITDIALOG:
        dialog_data = (struct log_dialog_struct*) lParam;
        dialog_data->dialog = dialog;
        SetWindowLongPtr(dialog, DWLP_USER, lParam);
        return log_dialog_init_dialog(dialog_data);

    case WM_CLOSE:
        log_dialog_close(dialog_data);
        return TRUE;

    case WM_DESTROY:
        log_dialog_destroy(dialog_data);
	break;

    case WM_SIZE:
        log_dialog_size(dialog_data, LOWORD(lParam), HIWORD(lParam));
	break;

    case WM_SYSTRAY:
        log_dialog_systray(dialog_data, wParam, lParam);
        return TRUE;

    case WM_TIMER:
	KillTimer(dialog, wParam);
        switch (wParam) {
        case TID_CLOSE:
        case TID_RESTART:
            log_dialog_timer_auto(dialog_data);
            return TRUE;
        case TID_ICON:
            log_dialog_timer_icon(dialog_data);
	    return TRUE;
        case TID_FLUSH:
            log_dialog_timer_flush(dialog_data);
            return TRUE;
        case TID_HIDE:
            log_dialog_timer_hide(dialog_data);
            return TRUE;
        }
	break;

    case WM_CONTEXTMENU:
        log_dialog_context_menu(dialog_data, (HWND) wParam, LOWORD(lParam), HIWORD(lParam));
	return TRUE;

    case WM_INITMENUPOPUP:
        log_dialog_initmenu_popup(dialog_data, (HMENU) wParam, LOWORD(lParam), HIWORD(lParam));
        return TRUE;

    case WM_COMMAND:
	switch (wParam) {
	case MAKEWPARAM(IDOK, BN_CLICKED): // for return key
	case MAKEWPARAM(IDCANCEL, BN_CLICKED): // for escape key and close button
            log_dialog_timer_hide(dialog_data);
            return TRUE;
	case ID_COPY:
            log_dialog_command_copy(dialog_data);
	    return TRUE;
	case ID_CLEAR:
            log_dialog_command_clear(dialog_data);
	    return TRUE;
	case ID_TERMINATE:
            log_dialog_command_terminate(dialog_data);
	    return TRUE;
        case ID_SHOWLOG:
            log_dialog_command_showlog(dialog_data);
            return TRUE;
        case ID_RESTART:
            log_dialog_command_restart(dialog_data);
            return TRUE;
        case ID_CANCEL_RESTART:
            log_dialog_command_cancel_restart(dialog_data);
            return TRUE;
	}
	break;

    default:
        if (message == WM_TASKBARCREATED) {
            log_dialog_taskbar_created(dialog_data);
            return TRUE;
        }
	break;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, char* commandLine, int showCommand)
{
    struct log_dialog_struct dialog_data = {NULL};

    if (is_own_standard_handle(STD_OUTPUT_HANDLE))
        setvbuf(stdout, NULL, _IONBF, 0);
    if (is_own_standard_handle(STD_ERROR_HANDLE))
        setvbuf(stderr, NULL, _IONBF, 0);

    instance = hInstance;
    initial_thread_id = GetCurrentThreadId();

    dialog_data.exitCode = 1;
    dialog_data.dialog  = CreateDialogParam(instance, MAKEINTRESOURCE(IDD_LOGFDIALOG), NULL, log_dialog_proc, (LPARAM) &dialog_data);
    if (dialog_data.dialog == NULL)
        return 1;

    if (initialize_main(__argc, __argv)) {
        DWORD threadid;
        abort_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        dialog_data.thread = CreateThread(NULL, 0, do_main, NULL, 0, &threadid);
    }

    while (IsWindow(dialog_data.dialog)) {
        switch (MsgWaitForMultipleObjectsEx(1, &dialog_data.thread, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE)) {
        case WAIT_OBJECT_0:
            log_dialog_exit_thread(&dialog_data);
            break;
        case WAIT_OBJECT_0 + 1:
            {
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    if (msg.hwnd == NULL) {
                        switch (msg.message) {
                        case WM_COMPLETED:
                            log_dialog_completed(&dialog_data);
                            break;
                        case WM_EXITED:
                            log_dialog_exited(&dialog_data);
                            break;
                        case WM_BASETITLE:
                            log_dialog_basetitle(&dialog_data, (char*) msg.lParam);
                            break;
                        case WM_RESTART:
                            log_dialog_restart(&dialog_data);
                            break;
                        case WM_PRINT_LOG:
                            log_dialog_print_log(&dialog_data, msg.wParam, (char*) msg.lParam);
                            break;
                        }
                        continue;
                    }
                    if (WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST
                        || WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST)
                        log_dialog_touch(&dialog_data);
                    if (!IsDialogMessage(dialog_data.dialog, &msg)) {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
            break;
        }
    }
    return dialog_data.exitCode;
}

static void set_valid_handle(DWORD type, HANDLE (*prepareHandle)()) {
    if (GetFileType(GetStdHandle(type)) == FILE_TYPE_UNKNOWN
           && GetLastError() == ERROR_INVALID_HANDLE) {
        HANDLE handle = prepareHandle();
        if (handle != INVALID_HANDLE_VALUE) {
            set_own_standard_handle(type);
            SetStdHandle(type, handle);
        }
    }
}

static HANDLE prepare_nul() {
    return CreateFile("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

static DWORD WINAPI read_output_thread_proc(LPVOID arg) {
    HANDLE in = (HANDLE) arg;
    DWORD read;
    char* buffer;
    DWORD buffer_size;
    GetNamedPipeInfo(in, NULL, NULL, &buffer_size, NULL);
    buffer = (char*) alloca(buffer_size);
    WaitForInputIdle(GetCurrentProcess(), INFINITE);
    while (ReadFile(in, buffer, sizeof buffer, &read, NULL) && read > 0) {
	char* data = snewn(read, char);
        memcpy(data, buffer, read);
	PostThreadMessage(initial_thread_id, WM_PRINT_LOG, read, (LPARAM) data);
    }
    CloseHandle(in);
    return 0;
}

static HANDLE prepare_pipe() {
    HANDLE in, out;
    if (CreatePipe(&in, &out, NULL, 0)) {
        DWORD threadid;
        HANDLE thread = CreateThread(NULL, 0, read_output_thread_proc, (LPVOID) in, 0, &threadid);
        if (thread != NULL) {
            CloseHandle(thread);
            return out;
        }
        CloseHandle(in);
        CloseHandle(out);
    }
    return INVALID_HANDLE_VALUE;
}

#pragma comment(linker, "/entry:WinStartup")
void WINAPI WinStartup() {
    void WinMainCRTStartup();
    set_valid_handle(STD_INPUT_HANDLE,  prepare_nul);
    set_valid_handle(STD_OUTPUT_HANDLE, prepare_pipe);
    set_valid_handle(STD_ERROR_HANDLE,  prepare_pipe);
    WinMainCRTStartup();
}
