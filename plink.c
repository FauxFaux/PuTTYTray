/*
 * PLink - a command-line (stdin/stdout) variant of PuTTY.
 */

#ifndef AUTO_WINSOCK
#include <winsock2.h>
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#define PUTTY_DO_GLOBALS /* actually _define_ globals */
#include "putty.h"
#include "storage.h"
#include "tree234.h"

#define MAX_STDIN_BACKLOG 4096

void fatalbox(char *p, ...)
{
  va_list ap;
  fprintf(stderr, "FATAL ERROR: ");
  va_start(ap, p);
  vfprintf(stderr, p, ap);
  va_end(ap);
  fputc('\n', stderr);
  WSACleanup();
  cleanup_exit(1);
}
void connection_fatal(char *p, ...)
{
  va_list ap;
  fprintf(stderr, "FATAL ERROR: ");
  va_start(ap, p);
  vfprintf(stderr, p, ap);
  va_end(ap);
  fputc('\n', stderr);
  WSACleanup();
  cleanup_exit(1);
}
void cmdline_error(char *p, ...)
{
  va_list ap;
  fprintf(stderr, "plink: ");
  va_start(ap, p);
  vfprintf(stderr, p, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static char *password = NULL;

HANDLE inhandle, outhandle, errhandle;
DWORD orig_console_mode;

WSAEVENT netevent;

int term_ldisc(int mode)
{
  return FALSE;
}
void ldisc_update(int echo, int edit)
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

struct input_data {
  DWORD len;
  char buffer[4096];
  HANDLE event, eventback;
};

static DWORD WINAPI stdin_read_thread(void *param)
{
  struct input_data *idata = (struct input_data *)param;
  HANDLE inhandle;

  inhandle = GetStdHandle(STD_INPUT_HANDLE);

  while (
      ReadFile(
          inhandle, idata->buffer, sizeof(idata->buffer), &idata->len, NULL) &&
      idata->len > 0) {
    SetEvent(idata->event);
    WaitForSingleObject(idata->eventback, INFINITE);
  }

  idata->len = 0;
  SetEvent(idata->event);

  return 0;
}

struct output_data {
  DWORD len, lenwritten;
  int writeret;
  char *buffer;
  int is_stderr, done;
  HANDLE event, eventback;
  int busy;
};

static DWORD WINAPI stdout_write_thread(void *param)
{
  struct output_data *odata = (struct output_data *)param;
  HANDLE outhandle, errhandle;

  outhandle = GetStdHandle(STD_OUTPUT_HANDLE);
  errhandle = GetStdHandle(STD_ERROR_HANDLE);

  while (1) {
    WaitForSingleObject(odata->eventback, INFINITE);
    if (odata->done)
      break;
    odata->writeret = WriteFile(odata->is_stderr ? errhandle : outhandle,
                                odata->buffer,
                                odata->len,
                                &odata->lenwritten,
                                NULL);
    SetEvent(odata->event);
  }

  return 0;
}

bufchain stdout_data, stderr_data;
struct output_data odata, edata;

void try_output(int is_stderr)
{
  struct output_data *data = (is_stderr ? &edata : &odata);
  void *senddata;
  int sendlen;

  if (!data->busy) {
    bufchain_prefix(
        is_stderr ? &stderr_data : &stdout_data, &senddata, &sendlen);
    data->buffer = senddata;
    data->len = sendlen;
    SetEvent(data->eventback);
    data->busy = 1;
  }
}

int from_backend(int is_stderr, char *data, int len)
{
  HANDLE h = (is_stderr ? errhandle : outhandle);
  int osize, esize;

  assert(len > 0);

  if (is_stderr) {
    bufchain_add(&stderr_data, data, len);
    try_output(1);
  } else {
    bufchain_add(&stdout_data, data, len);
    try_output(0);
  }

  osize = bufchain_size(&stdout_data);
  esize = bufchain_size(&stderr_data);

  return osize + esize;
}

/*
 *  Short description of parameters.
 */
static void usage(void)
{
  printf("PuTTY Link: command-line connection utility\n");
  printf("%s\n", ver);
  printf("Usage: plink [options] [user@]host [command]\n");
  printf("       (\"host\" can also be a PuTTY saved session name)\n");
  printf("Options:\n");
  printf("  -v        show verbose messages\n");
  printf("  -load sessname  Load settings from saved session\n");
  printf("  -ssh -telnet -rlogin -raw\n");
  printf("            force use of a particular protocol (default SSH)\n");
  printf("  -P port   connect to specified port\n");
  printf("  -l user   connect with specified username\n");
  printf("  -m file   read remote command(s) from file\n");
  printf("  -batch    disable all interactive prompts\n");
  printf("The following options only apply to SSH connections:\n");
  printf("  -pw passw login with specified password\n");
  printf("  -L listen-port:host:port   Forward local port to "
         "remote address\n");
  printf("  -R listen-port:host:port   Forward remote port to"
         " local address\n");
  printf("  -X -x     enable / disable X11 forwarding\n");
  printf("  -A -a     enable / disable agent forwarding\n");
  printf("  -t -T     enable / disable pty allocation\n");
  printf("  -1 -2     force use of particular protocol version\n");
  printf("  -C        enable compression\n");
  printf("  -i key    private key file for authentication\n");
  exit(1);
}

char *do_select(SOCKET skt, int startup)
{
  int events;
  if (startup) {
    events = (FD_CONNECT | FD_READ | FD_WRITE | FD_OOB | FD_CLOSE | FD_ACCEPT);
  } else {
    events = 0;
  }
  if (WSAEventSelect(skt, netevent, events) == SOCKET_ERROR) {
    switch (WSAGetLastError()) {
    case WSAENETDOWN:
      return "Network is down";
    default:
      return "WSAAsyncSelect(): unknown error";
    }
  }
  return NULL;
}

int main(int argc, char **argv)
{
  WSADATA wsadata;
  WORD winsock_ver;
  WSAEVENT stdinevent, stdoutevent, stderrevent;
  HANDLE handles[4];
  DWORD in_threadid, out_threadid, err_threadid;
  struct input_data idata;
  int reading;
  int sending;
  int portnumber = -1;
  SOCKET *sklist;
  int skcount, sksize;
  int connopen;
  int exitcode;

  ssh_get_line = console_get_line;

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
  do_defaults(NULL, &cfg);
  default_protocol = cfg.protocol;
  default_port = cfg.port;
  {
    /*
     * Override the default protocol if PLINK_PROTOCOL is set.
     */
    char *p = getenv("PLINK_PROTOCOL");
    int i;
    if (p) {
      for (i = 0; backends[i].backend != NULL; i++) {
        if (!strcmp(backends[i].name, p)) {
          default_protocol = cfg.protocol = backends[i].protocol;
          default_port = cfg.port = backends[i].backend->default_port;
          break;
        }
      }
    }
  }
  while (--argc) {
    char *p = *++argv;
    if (*p == '-') {
      int ret = cmdline_process_param(p, (argc > 1 ? argv[1] : NULL), 1);
      if (ret == -2) {
        fprintf(stderr, "plink: option \"%s\" requires an argument\n", p);
      } else if (ret == 2) {
        --argc, ++argv;
      } else if (ret == 1) {
        continue;
      } else if (!strcmp(p, "-batch")) {
        console_batch_mode = 1;
      } else if (!strcmp(p, "-log")) {
        logfile = "putty.log";
      }
    } else if (*p) {
      if (!*cfg.host) {
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
        } else {
          char *r;
          /*
           * Before we process the [user@]host string, we
           * first check for the presence of a protocol
           * prefix (a protocol name followed by ",").
           */
          r = strchr(p, ',');
          if (r) {
            int i, j;
            for (i = 0; backends[i].backend != NULL; i++) {
              j = strlen(backends[i].name);
              if (j == r - p && !memcmp(backends[i].name, p, j)) {
                default_protocol = cfg.protocol = backends[i].protocol;
                portnumber = backends[i].backend->default_port;
                p = r + 1;
                break;
              }
            }
          }

          /*
           * Three cases. Either (a) there's a nonzero
           * length string followed by an @, in which
           * case that's user and the remainder is host.
           * Or (b) there's only one string, not counting
           * a potential initial @, and it exists in the
           * saved-sessions database. Or (c) only one
           * string and it _doesn't_ exist in the
           * database.
           */
          r = strrchr(p, '@');
          if (r == p)
            p++, r = NULL; /* discount initial @ */
          if (r == NULL) {
            /*
             * One string.
             */
            Config cfg2;
            do_defaults(p, &cfg2);
            if (cfg2.host[0] == '\0') {
              /* No settings for this host; use defaults */
              strncpy(cfg.host, p, sizeof(cfg.host) - 1);
              cfg.host[sizeof(cfg.host) - 1] = '\0';
              cfg.port = default_port;
            } else {
              cfg = cfg2;
              cfg.remote_cmd_ptr = cfg.remote_cmd;
            }
          } else {
            *r++ = '\0';
            strncpy(cfg.username, p, sizeof(cfg.username) - 1);
            cfg.username[sizeof(cfg.username) - 1] = '\0';
            strncpy(cfg.host, r, sizeof(cfg.host) - 1);
            cfg.host[sizeof(cfg.host) - 1] = '\0';
            cfg.port = default_port;
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
              command = srealloc(command, cmdsize);
            }
            command[cmdlen++] = *p++;
          }
          if (cmdlen >= cmdsize) {
            cmdsize = cmdlen + 512;
            command = srealloc(command, cmdsize);
          }
          command[cmdlen++] = ' '; /* always add trailing space */
          if (--argc)
            p = *++argv;
        }
        if (cmdlen)
          command[--cmdlen] = '\0';
        /* change trailing blank to NUL */
        cfg.remote_cmd_ptr = command;
        cfg.remote_cmd_ptr2 = NULL;
        cfg.nopty = TRUE; /* command => no terminal */

        break; /* done with cmdline */
      }
    }
  }

  if (!*cfg.host) {
    usage();
  }

  /*
   * Trim leading whitespace off the hostname if it's there.
   */
  {
    int space = strspn(cfg.host, " \t");
    memmove(cfg.host, cfg.host + space, 1 + strlen(cfg.host) - space);
  }

  /* See if host is of the form user@host */
  if (cfg.host[0] != '\0') {
    char *atsign = strchr(cfg.host, '@');
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
   * Perform command-line overrides on session configuration.
   */
  cmdline_run_saved();

  /*
   * Trim a colon suffix off the hostname if it's there.
   */
  cfg.host[strcspn(cfg.host, ":")] = '\0';

  if (!*cfg.remote_cmd_ptr)
    flags |= FLAG_INTERACTIVE;

  /*
   * Select protocol. This is farmed out into a table in a
   * separate file to enable an ssh-free variant.
   */
  {
    int i;
    back = NULL;
    for (i = 0; backends[i].backend != NULL; i++)
      if (backends[i].protocol == cfg.protocol) {
        back = backends[i].backend;
        break;
      }
    if (back == NULL) {
      fprintf(stderr, "Internal fault: Unsupported protocol found\n");
      return 1;
    }
  }

  /*
   * Select port.
   */
  if (portnumber != -1)
    cfg.port = portnumber;

  /*
   * Initialise WinSock.
   */
  winsock_ver = MAKEWORD(2, 0);
  if (WSAStartup(winsock_ver, &wsadata)) {
    MessageBox(NULL,
               "Unable to initialise WinSock",
               "WinSock Error",
               MB_OK | MB_ICONEXCLAMATION);
    return 1;
  }
  if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 0) {
    MessageBox(NULL,
               "WinSock version is incompatible with 2.0",
               "WinSock Error",
               MB_OK | MB_ICONEXCLAMATION);
    WSACleanup();
    return 1;
  }
  sk_init();

  /*
   * Start up the connection.
   */
  netevent = CreateEvent(NULL, FALSE, FALSE, NULL);
  {
    char *error;
    char *realhost;
    /* nodelay is only useful if stdin is a character device (console) */
    int nodelay =
        cfg.tcp_nodelay &&
        (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR);

    error = back->init(cfg.host, cfg.port, &realhost, nodelay);
    if (error) {
      fprintf(stderr, "Unable to open connection:\n%s", error);
      return 1;
    }
    sfree(realhost);
  }
  connopen = 1;

  stdinevent = CreateEvent(NULL, FALSE, FALSE, NULL);
  stdoutevent = CreateEvent(NULL, FALSE, FALSE, NULL);
  stderrevent = CreateEvent(NULL, FALSE, FALSE, NULL);

  inhandle = GetStdHandle(STD_INPUT_HANDLE);
  outhandle = GetStdHandle(STD_OUTPUT_HANDLE);
  errhandle = GetStdHandle(STD_ERROR_HANDLE);
  GetConsoleMode(inhandle, &orig_console_mode);
  SetConsoleMode(inhandle, ENABLE_PROCESSED_INPUT);

  /*
   * Turn off ECHO and LINE input modes. We don't care if this
   * call fails, because we know we aren't necessarily running in
   * a console.
   */
  handles[0] = netevent;
  handles[1] = stdinevent;
  handles[2] = stdoutevent;
  handles[3] = stderrevent;
  sending = FALSE;

  /*
   * Create spare threads to write to stdout and stderr, so we
   * can arrange asynchronous writes.
   */
  odata.event = stdoutevent;
  odata.eventback = CreateEvent(NULL, FALSE, FALSE, NULL);
  odata.is_stderr = 0;
  odata.busy = odata.done = 0;
  if (!CreateThread(NULL, 0, stdout_write_thread, &odata, 0, &out_threadid)) {
    fprintf(stderr, "Unable to create output thread\n");
    cleanup_exit(1);
  }
  edata.event = stderrevent;
  edata.eventback = CreateEvent(NULL, FALSE, FALSE, NULL);
  edata.is_stderr = 1;
  edata.busy = edata.done = 0;
  if (!CreateThread(NULL, 0, stdout_write_thread, &edata, 0, &err_threadid)) {
    fprintf(stderr, "Unable to create error output thread\n");
    cleanup_exit(1);
  }

  while (1) {
    int n;

    if (!sending && back->sendok()) {
      /*
       * Create a separate thread to read from stdin. This is
       * a total pain, but I can't find another way to do it:
       *
       *  - an overlapped ReadFile or ReadFileEx just doesn't
       *    happen; we get failure from ReadFileEx, and
       *    ReadFile blocks despite being given an OVERLAPPED
       *    structure. Perhaps we can't do overlapped reads
       *    on consoles. WHY THE HELL NOT?
       *
       *  - WaitForMultipleObjects(netevent, console) doesn't
       *    work, because it signals the console when
       *    _anything_ happens, including mouse motions and
       *    other things that don't cause data to be readable
       *    - so we're back to ReadFile blocking.
       */
      idata.event = stdinevent;
      idata.eventback = CreateEvent(NULL, FALSE, FALSE, NULL);
      if (!CreateThread(NULL, 0, stdin_read_thread, &idata, 0, &in_threadid)) {
        fprintf(stderr, "Unable to create input thread\n");
        cleanup_exit(1);
      }
      sending = TRUE;
    }

    n = WaitForMultipleObjects(4, handles, FALSE, INFINITE);
    if (n == 0) {
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
      for (socket = first_socket(&socketstate); socket != INVALID_SOCKET;
           socket = next_socket(&socketstate))
        i++;

      /* Expand the buffer if necessary. */
      if (i > sksize) {
        sksize = i + 16;
        sklist = srealloc(sklist, sksize * sizeof(*sklist));
      }

      /* Retrieve the sockets into sklist. */
      skcount = 0;
      for (socket = first_socket(&socketstate); socket != INVALID_SOCKET;
           socket = next_socket(&socketstate)) {
        sklist[skcount++] = socket;
      }

      /* Now we're done enumerating; go through the list. */
      for (i = 0; i < skcount; i++) {
        WPARAM wp;
        socket = sklist[i];
        wp = (WPARAM)socket;
        if (!WSAEnumNetworkEvents(socket, NULL, &things)) {
          static const struct {
            int bit, mask;
          } eventtypes[] = {
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
    } else if (n == 1) {
      reading = 0;
      noise_ultralight(idata.len);
      if (connopen && back->socket() != NULL) {
        if (idata.len > 0) {
          back->send(idata.buffer, idata.len);
        } else {
          back->special(TS_EOF);
        }
      }
    } else if (n == 2) {
      odata.busy = 0;
      if (!odata.writeret) {
        fprintf(stderr, "Unable to write to standard output\n");
        cleanup_exit(0);
      }
      bufchain_consume(&stdout_data, odata.lenwritten);
      if (bufchain_size(&stdout_data) > 0)
        try_output(0);
      if (connopen && back->socket() != NULL) {
        back->unthrottle(bufchain_size(&stdout_data) +
                         bufchain_size(&stderr_data));
      }
    } else if (n == 3) {
      edata.busy = 0;
      if (!edata.writeret) {
        fprintf(stderr, "Unable to write to standard output\n");
        cleanup_exit(0);
      }
      bufchain_consume(&stderr_data, edata.lenwritten);
      if (bufchain_size(&stderr_data) > 0)
        try_output(1);
      if (connopen && back->socket() != NULL) {
        back->unthrottle(bufchain_size(&stdout_data) +
                         bufchain_size(&stderr_data));
      }
    }
    if (!reading && back->sendbuffer() < MAX_STDIN_BACKLOG) {
      SetEvent(idata.eventback);
      reading = 1;
    }
    if ((!connopen || back->socket() == NULL) &&
        bufchain_size(&stdout_data) == 0 && bufchain_size(&stderr_data) == 0)
      break; /* we closed the connection */
  }
  WSACleanup();
  exitcode = back->exitcode();
  if (exitcode < 0) {
    fprintf(stderr, "Remote process exit code unavailable\n");
    exitcode = 1; /* this is an error condition */
  }
  return exitcode;
}
