/*
 * PLink - a command-line (stdin/stdout) variant of PuTTY.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pwd.h>
#include <sys/ioctl.h>

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
  cleanup_exit(1);
}
void connection_fatal(void *frontend, char *p, ...)
{
  va_list ap;
  fprintf(stderr, "FATAL ERROR: ");
  va_start(ap, p);
  vfprintf(stderr, p, ap);
  va_end(ap);
  fputc('\n', stderr);
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

struct termios orig_termios;

static Backend *back;
static void *backhandle;
static Config cfg;

/*
 * Default settings that are specific to pterm.
 */
char *platform_default_s(const char *name)
{
  if (!strcmp(name, "X11Display"))
    return dupstr(getenv("DISPLAY"));
  if (!strcmp(name, "TermType"))
    return dupstr(getenv("TERM"));
  if (!strcmp(name, "UserName"))
    return get_username();
  return NULL;
}

int platform_default_i(const char *name, int def)
{
  if (!strcmp(name, "TermWidth") || !strcmp(name, "TermHeight")) {
    struct winsize size;
    if (ioctl(0, TIOCGWINSZ, (void *)&size) >= 0)
      return (!strcmp(name, "TermWidth") ? size.ws_col : size.ws_row);
  }
  return def;
}

FontSpec platform_default_fontspec(const char *name)
{
  FontSpec ret;
  *ret.name = '\0';
  return ret;
}

Filename platform_default_filename(const char *name)
{
  Filename ret;
  if (!strcmp(name, "LogFileName"))
    strcpy(ret.path, "putty.log");
  else
    *ret.path = '\0';
  return ret;
}

char *x_get_default(const char *key)
{
  return NULL; /* this is a stub */
}
int term_ldisc(Terminal *term, int mode)
{
  return FALSE;
}
void ldisc_update(void *frontend, int echo, int edit)
{
  /* Update stdin read mode to reflect changes in line discipline. */
  struct termios mode;

  mode = orig_termios;

  if (echo)
    mode.c_lflag |= ECHO;
  else
    mode.c_lflag &= ~ECHO;

  if (edit)
    mode.c_lflag |= ISIG | ICANON;
  else
    mode.c_lflag &= ~(ISIG | ICANON);

  tcsetattr(0, TCSANOW, &mode);
}

void cleanup_termios(void)
{
  tcsetattr(0, TCSANOW, &orig_termios);
}

bufchain stdout_data, stderr_data;

void try_output(int is_stderr)
{
  bufchain *chain = (is_stderr ? &stderr_data : &stdout_data);
  int fd = (is_stderr ? 2 : 1);
  void *senddata;
  int sendlen, ret;

  if (bufchain_size(chain) == 0)
    return;

  bufchain_prefix(chain, &senddata, &sendlen);
  ret = write(fd, senddata, sendlen);
  if (ret > 0)
    bufchain_consume(chain, ret);
  else if (ret < 0) {
    perror(is_stderr ? "stderr: write" : "stdout: write");
    exit(1);
  }
}

int from_backend(void *frontend_handle,
                 int is_stderr,
                 const char *data,
                 int len)
{
  int osize, esize;

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

int signalpipe[2];

void sigwinch(int signum)
{
  write(signalpipe[1], "x", 1);
}

/*
 * In Plink our selects are synchronous, so these functions are
 * empty stubs.
 */
int uxsel_input_add(int fd, int rwx)
{
  return 0;
}
void uxsel_input_remove(int id)
{
}

/*
 * Short description of parameters.
 */
static void usage(void)
{
  printf("PuTTY Link: command-line connection utility\n");
  printf("%s\n", ver);
  printf("Usage: plink [options] [user@]host [command]\n");
  printf("       (\"host\" can also be a PuTTY saved session name)\n");
  printf("Options:\n");
  printf("  -V        print version information\n");
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
  printf("  -C        enable compression\n");
  printf("  -i key    private key file for authentication\n");
  printf("  -s        remote command is an SSH subsystem (SSH-2 only)\n");
  exit(1);
}

static void version(void)
{
  printf("plink: %s\n", ver);
  exit(1);
}

int main(int argc, char **argv)
{
  int sending;
  int portnumber = -1;
  int *fdlist;
  int fd;
  int i, fdcount, fdsize, fdstate;
  int connopen;
  int exitcode;
  int errors;
  int use_subsystem = 0;
  void *ldisc, *logctx;

  ssh_get_line = console_get_line;

  fdlist = NULL;
  fdcount = fdsize = 0;
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
  loaded_session = FALSE;
  default_protocol = cfg.protocol;
  default_port = cfg.port;
  errors = 0;
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
      int ret = cmdline_process_param(p, (argc > 1 ? argv[1] : NULL), 1, &cfg);
      if (ret == -2) {
        fprintf(stderr, "plink: option \"%s\" requires an argument\n", p);
        errors = 1;
      } else if (ret == 2) {
        --argc, ++argv;
      } else if (ret == 1) {
        continue;
      } else if (!strcmp(p, "-batch")) {
        console_batch_mode = 1;
      } else if (!strcmp(p, "-s")) {
        /* Save status to write to cfg later. */
        use_subsystem = 1;
      } else if (!strcmp(p, "-V")) {
        version();
      } else if (!strcmp(p, "-o")) {
        if (argc <= 1) {
          fprintf(stderr, "plink: option \"-o\" requires an argument\n");
          errors = 1;
        } else {
          --argc;
          provide_xrm_string(*++argv);
        }
      } else {
        fprintf(stderr, "plink: unknown option \"%s\"\n", p);
        errors = 1;
      }
    } else if (*p) {
      if (!*cfg.host) {
        char *q = p;

        do_defaults(NULL, &cfg);

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
            if (loaded_session || cfg2.host[0] == '\0') {
              /* No settings for this host; use defaults */
              /* (or session was already loaded with -load) */
              strncpy(cfg.host, p, sizeof(cfg.host) - 1);
              cfg.host[sizeof(cfg.host) - 1] = '\0';
              cfg.port = default_port;
            } else {
              cfg = cfg2;
              /* Ick: patch up internal pointer after copy */
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
              command = sresize(command, cmdsize, char);
            }
            command[cmdlen++] = *p++;
          }
          if (cmdlen >= cmdsize) {
            cmdsize = cmdlen + 512;
            command = sresize(command, cmdsize, char);
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

  if (errors)
    return 1;

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
  cmdline_run_saved(&cfg);

  /*
   * Apply subsystem status.
   */
  if (use_subsystem)
    cfg.ssh_subsys = TRUE;

  /*
   * Trim a colon suffix off the hostname if it's there.
   */
  cfg.host[strcspn(cfg.host, ":")] = '\0';

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
   * Set up the pipe we'll use to tell us about SIGWINCH.
   */
  if (pipe(signalpipe) < 0) {
    perror("pipe");
    exit(1);
  }
  putty_signal(SIGWINCH, sigwinch);

  sk_init();
  uxsel_init();

  /*
   * Start up the connection.
   */
  logctx = log_init(NULL, &cfg);
  console_provide_logctx(logctx);
  {
    const char *error;
    char *realhost;
    /* nodelay is only useful if stdin is a terminal device */
    int nodelay = cfg.tcp_nodelay && isatty(0);

    error = back->init(NULL,
                       &backhandle,
                       &cfg,
                       cfg.host,
                       cfg.port,
                       &realhost,
                       nodelay,
                       cfg.tcp_keepalives);
    if (error) {
      fprintf(stderr, "Unable to open connection:\n%s\n", error);
      return 1;
    }
    back->provide_logctx(backhandle, logctx);
    ldisc = ldisc_create(&cfg, NULL, back, backhandle, NULL);
    sfree(realhost);
  }
  connopen = 1;

  /*
   * Set up the initial console mode. We don't care if this call
   * fails, because we know we aren't necessarily running in a
   * console.
   */
  tcgetattr(0, &orig_termios);
  atexit(cleanup_termios);
  ldisc_update(NULL, 1, 1);
  sending = FALSE;

  while (1) {
    fd_set rset, wset, xset;
    int maxfd;
    int rwx;
    int ret;

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&xset);
    maxfd = 0;

    FD_SET_MAX(signalpipe[0], maxfd, rset);

    if (connopen && !sending && back->socket(backhandle) != NULL &&
        back->sendok(backhandle) &&
        back->sendbuffer(backhandle) < MAX_STDIN_BACKLOG) {
      /* If we're OK to send, then try to read from stdin. */
      FD_SET_MAX(0, maxfd, rset);
    }

    if (bufchain_size(&stdout_data) > 0) {
      /* If we have data for stdout, try to write to stdout. */
      FD_SET_MAX(1, maxfd, wset);
    }

    if (bufchain_size(&stderr_data) > 0) {
      /* If we have data for stderr, try to write to stderr. */
      FD_SET_MAX(2, maxfd, wset);
    }

    /* Count the currently active fds. */
    i = 0;
    for (fd = first_fd(&fdstate, &rwx); fd >= 0; fd = next_fd(&fdstate, &rwx))
      i++;

    /* Expand the fdlist buffer if necessary. */
    if (i > fdsize) {
      fdsize = i + 16;
      fdlist = sresize(fdlist, fdsize, int);
    }

    /*
     * Add all currently open fds to the select sets, and store
     * them in fdlist as well.
     */
    fdcount = 0;
    for (fd = first_fd(&fdstate, &rwx); fd >= 0; fd = next_fd(&fdstate, &rwx)) {
      fdlist[fdcount++] = fd;
      if (rwx & 1)
        FD_SET_MAX(fd, maxfd, rset);
      if (rwx & 2)
        FD_SET_MAX(fd, maxfd, wset);
      if (rwx & 4)
        FD_SET_MAX(fd, maxfd, xset);
    }

    do {
      ret = select(maxfd, &rset, &wset, &xset, NULL);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
      perror("select");
      exit(1);
    }

    for (i = 0; i < fdcount; i++) {
      fd = fdlist[i];
      /*
       * We must process exceptional notifications before
       * ordinary readability ones, or we may go straight
       * past the urgent marker.
       */
      if (FD_ISSET(fd, &xset))
        select_result(fd, 4);
      if (FD_ISSET(fd, &rset))
        select_result(fd, 1);
      if (FD_ISSET(fd, &wset))
        select_result(fd, 2);
    }

    if (FD_ISSET(signalpipe[0], &rset)) {
      char c[1];
      struct winsize size;
      read(signalpipe[0], c, 1); /* ignore its value; it'll be `x' */
      if (ioctl(0, TIOCGWINSZ, (void *)&size) >= 0)
        back->size(backhandle, size.ws_col, size.ws_row);
    }

    if (FD_ISSET(0, &rset)) {
      char buf[4096];
      int ret;

      if (connopen && back->socket(backhandle) != NULL) {
        ret = read(0, buf, sizeof(buf));
        if (ret < 0) {
          perror("stdin: read");
          exit(1);
        } else if (ret == 0) {
          back->special(backhandle, TS_EOF);
          sending = FALSE; /* send nothing further after this */
        } else {
          back->send(backhandle, buf, ret);
        }
      }
    }

    if (FD_ISSET(1, &wset)) {
      try_output(0);
    }

    if (FD_ISSET(2, &wset)) {
      try_output(1);
    }

    if ((!connopen || back->socket(backhandle) == NULL) &&
        bufchain_size(&stdout_data) == 0 && bufchain_size(&stderr_data) == 0)
      break; /* we closed the connection */
  }
  exitcode = back->exitcode(backhandle);
  if (exitcode < 0) {
    fprintf(stderr, "Remote process exit code unavailable\n");
    exitcode = 1; /* this is an error condition */
  }
  cleanup_exit(exitcode);
  return exitcode; /* shouldn't happen, but placates gcc */
}
