/*
 * scp.c  -  Scp (Secure Copy) client for PuTTY.
 * Joris van Rantwijk, Simon Tatham
 *
 * This is mainly based on ssh-1.2.26/scp.c by Timo Rinne & Tatu Ylonen.
 * They, in turn, used stuff from BSD rcp.
 *
 * (SGT, 2001-09-10: Joris van Rantwijk assures me that although
 * this file as originally submitted was inspired by, and
 * _structurally_ based on, ssh-1.2.26's scp.c, there wasn't any
 * actual code duplicated, so the above comment shouldn't give rise
 * to licensing issues.)
 */

#include <windows.h>
#ifndef AUTO_WINSOCK
#ifdef WINSOCK_TWO
#include <winsock2.h>
#else
#include <winsock.h>
#endif
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#define PUTTY_DO_GLOBALS
#include "putty.h"
#include "ssh.h"
#include "sftp.h"
#include "winstuff.h"
#include "storage.h"

#define TIME_POSIX_TO_WIN(t, ft)                                               \
  (*(LONGLONG *)&(ft) =                                                        \
       ((LONGLONG)(t) + (LONGLONG)11644473600) * (LONGLONG)10000000)
#define TIME_WIN_TO_POSIX(ft, t)                                               \
  ((t) = (unsigned long)((*(LONGLONG *)&(ft)) / (LONGLONG)10000000 -           \
                         (LONGLONG)11644473600))

/* GUI Adaptation - Sept 2000 */

/* This is just a base value from which the main message numbers are
 * derived. */
#define WM_APP_BASE 0x8000

/* These two pass a single character value in wParam. They represent
 * the visible output from PSCP. */
#define WM_STD_OUT_CHAR (WM_APP_BASE + 400)
#define WM_STD_ERR_CHAR (WM_APP_BASE + 401)

/* These pass a transfer status update. WM_STATS_CHAR passes a single
 * character in wParam, and is called repeatedly to pass the name of
 * the file, terminated with "\n". WM_STATS_SIZE passes the size of
 * the file being transferred in wParam. WM_STATS_ELAPSED is called
 * to pass the elapsed time (in seconds) in wParam, and
 * WM_STATS_PERCENT passes the percentage of the transfer which is
 * complete, also in wParam. */
#define WM_STATS_CHAR (WM_APP_BASE + 402)
#define WM_STATS_SIZE (WM_APP_BASE + 403)
#define WM_STATS_PERCENT (WM_APP_BASE + 404)
#define WM_STATS_ELAPSED (WM_APP_BASE + 405)

/* These are used at the end of a run to pass an error code in
 * wParam: zero means success, nonzero means failure. WM_RET_ERR_CNT
 * is used after a copy, and WM_LS_RET_ERR_CNT is used after a file
 * list operation. */
#define WM_RET_ERR_CNT (WM_APP_BASE + 406)
#define WM_LS_RET_ERR_CNT (WM_APP_BASE + 407)

/* More transfer status update messages. WM_STATS_DONE passes the
 * number of bytes sent so far in wParam. WM_STATS_ETA passes the
 * estimated time to completion (in seconds). WM_STATS_RATEBS passes
 * the average transfer rate (in bytes per second). */
#define WM_STATS_DONE (WM_APP_BASE + 408)
#define WM_STATS_ETA (WM_APP_BASE + 409)
#define WM_STATS_RATEBS (WM_APP_BASE + 410)

static int list = 0;
static int verbose = 0;
static int recursive = 0;
static int preserve = 0;
static int targetshouldbedirectory = 0;
static int statistics = 1;
static int portnumber = 0;
static int prev_stats_len = 0;
static int scp_unsafe_mode = 0;
static char *password = NULL;
static int errs = 0;
/* GUI Adaptation - Sept 2000 */
#define NAME_STR_MAX 2048
static char statname[NAME_STR_MAX + 1];
static unsigned long statsize = 0;
static unsigned long statdone = 0;
static unsigned long stateta = 0;
static unsigned long statratebs = 0;
static int statperct = 0;
static unsigned long statelapsed = 0;
static int gui_mode = 0;
static char *gui_hwnd = NULL;
static int using_sftp = 0;

static void source(char *src);
static void rsource(char *src);
static void sink(char *targ, char *src);
/* GUI Adaptation - Sept 2000 */
static void tell_char(FILE *stream, char c);
static void tell_str(FILE *stream, char *str);
static void tell_user(FILE *stream, char *fmt, ...);
static void gui_update_stats(char *name,
                             unsigned long size,
                             int percentage,
                             unsigned long elapsed,
                             unsigned long done,
                             unsigned long eta,
                             unsigned long ratebs);

/*
 * The maximum amount of queued data we accept before we stop and
 * wait for the server to process some.
 */
#define MAX_SCP_BUFSIZE 16384

void ldisc_send(char *buf, int len, int interactive)
{
  /*
   * This is only here because of the calls to ldisc_send(NULL,
   * 0) in ssh.c. Nothing in PSCP actually needs to use the ldisc
   * as an ldisc. So if we get called with any real data, I want
   * to know about it.
   */
  assert(len == 0);
}

/* GUI Adaptation - Sept 2000 */
static void send_msg(HWND h, UINT message, WPARAM wParam)
{
  while (!PostMessage(h, message, wParam, 0))
    SleepEx(1000, TRUE);
}

static void tell_char(FILE *stream, char c)
{
  if (!gui_mode)
    fputc(c, stream);
  else {
    unsigned int msg_id = WM_STD_OUT_CHAR;
    if (stream == stderr)
      msg_id = WM_STD_ERR_CHAR;
    send_msg((HWND)atoi(gui_hwnd), msg_id, (WPARAM)c);
  }
}

static void tell_str(FILE *stream, char *str)
{
  unsigned int i;

  for (i = 0; i < strlen(str); ++i)
    tell_char(stream, str[i]);
}

static void tell_user(FILE *stream, char *fmt, ...)
{
  char str[0x100]; /* Make the size big enough */
  va_list ap;
  va_start(ap, fmt);
  vsprintf(str, fmt, ap);
  va_end(ap);
  strcat(str, "\n");
  tell_str(stream, str);
}

static void gui_update_stats(char *name,
                             unsigned long size,
                             int percentage,
                             unsigned long elapsed,
                             unsigned long done,
                             unsigned long eta,
                             unsigned long ratebs)
{
  unsigned int i;

  if (strcmp(name, statname) != 0) {
    for (i = 0; i < strlen(name); ++i)
      send_msg((HWND)atoi(gui_hwnd), WM_STATS_CHAR, (WPARAM)name[i]);
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_CHAR, (WPARAM)'\n');
    strcpy(statname, name);
  }
  if (statsize != size) {
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_SIZE, (WPARAM)size);
    statsize = size;
  }
  if (statdone != done) {
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_DONE, (WPARAM)done);
    statdone = done;
  }
  if (stateta != eta) {
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_ETA, (WPARAM)eta);
    stateta = eta;
  }
  if (statratebs != ratebs) {
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_RATEBS, (WPARAM)ratebs);
    statratebs = ratebs;
  }
  if (statelapsed != elapsed) {
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_ELAPSED, (WPARAM)elapsed);
    statelapsed = elapsed;
  }
  if (statperct != percentage) {
    send_msg((HWND)atoi(gui_hwnd), WM_STATS_PERCENT, (WPARAM)percentage);
    statperct = percentage;
  }
}

/*
 *  Print an error message and perform a fatal exit.
 */
void fatalbox(char *fmt, ...)
{
  char str[0x100]; /* Make the size big enough */
  va_list ap;
  va_start(ap, fmt);
  strcpy(str, "Fatal: ");
  vsprintf(str + strlen(str), fmt, ap);
  va_end(ap);
  strcat(str, "\n");
  tell_str(stderr, str);
  errs++;

  if (gui_mode) {
    unsigned int msg_id = WM_RET_ERR_CNT;
    if (list)
      msg_id = WM_LS_RET_ERR_CNT;
    while (
        !PostMessage((HWND)atoi(gui_hwnd), msg_id, (WPARAM)errs, 0 /*lParam */))
      SleepEx(1000, TRUE);
  }

  cleanup_exit(1);
}
void connection_fatal(char *fmt, ...)
{
  char str[0x100]; /* Make the size big enough */
  va_list ap;
  va_start(ap, fmt);
  strcpy(str, "Fatal: ");
  vsprintf(str + strlen(str), fmt, ap);
  va_end(ap);
  strcat(str, "\n");
  tell_str(stderr, str);
  errs++;

  if (gui_mode) {
    unsigned int msg_id = WM_RET_ERR_CNT;
    if (list)
      msg_id = WM_LS_RET_ERR_CNT;
    while (
        !PostMessage((HWND)atoi(gui_hwnd), msg_id, (WPARAM)errs, 0 /*lParam */))
      SleepEx(1000, TRUE);
  }

  cleanup_exit(1);
}

/*
 * Be told what socket we're supposed to be using.
 */
static SOCKET scp_ssh_socket;
char *do_select(SOCKET skt, int startup)
{
  if (startup)
    scp_ssh_socket = skt;
  else
    scp_ssh_socket = INVALID_SOCKET;
  return NULL;
}
extern int select_result(WPARAM, LPARAM);

/*
 * Receive a block of data from the SSH link. Block until all data
 * is available.
 *
 * To do this, we repeatedly call the SSH protocol module, with our
 * own trap in from_backend() to catch the data that comes back. We
 * do this until we have enough data.
 */

static unsigned char *outptr;              /* where to put the data */
static unsigned outlen;                    /* how much data required */
static unsigned char *pending = NULL;      /* any spare data */
static unsigned pendlen = 0, pendsize = 0; /* length and phys. size of buffer */
int from_backend(int is_stderr, char *data, int datalen)
{
  unsigned char *p = (unsigned char *)data;
  unsigned len = (unsigned)datalen;

  assert(len > 0);

  /*
   * stderr data is just spouted to local stderr and otherwise
   * ignored.
   */
  if (is_stderr) {
    fwrite(data, 1, len, stderr);
    return 0;
  }

  /*
   * If this is before the real session begins, just return.
   */
  if (!outptr)
    return 0;

  if (outlen > 0) {
    unsigned used = outlen;
    if (used > len)
      used = len;
    memcpy(outptr, p, used);
    outptr += used;
    outlen -= used;
    p += used;
    len -= used;
  }

  if (len > 0) {
    if (pendsize < pendlen + len) {
      pendsize = pendlen + len + 4096;
      pending = (pending ? srealloc(pending, pendsize) : smalloc(pendsize));
      if (!pending)
        fatalbox("Out of memory");
    }
    memcpy(pending + pendlen, p, len);
    pendlen += len;
  }

  return 0;
}
static int scp_process_network_event(void)
{
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(scp_ssh_socket, &readfds);
  if (select(1, &readfds, NULL, NULL, NULL) < 0)
    return 0; /* doom */
  select_result((WPARAM)scp_ssh_socket, (LPARAM)FD_READ);
  return 1;
}
static int ssh_scp_recv(unsigned char *buf, int len)
{
  outptr = buf;
  outlen = len;

  /*
   * See if the pending-input block contains some of what we
   * need.
   */
  if (pendlen > 0) {
    unsigned pendused = pendlen;
    if (pendused > outlen)
      pendused = outlen;
    memcpy(outptr, pending, pendused);
    memmove(pending, pending + pendused, pendlen - pendused);
    outptr += pendused;
    outlen -= pendused;
    pendlen -= pendused;
    if (pendlen == 0) {
      pendsize = 0;
      sfree(pending);
      pending = NULL;
    }
    if (outlen == 0)
      return len;
  }

  while (outlen > 0) {
    if (!scp_process_network_event())
      return 0; /* doom */
  }

  return len;
}

/*
 * Loop through the ssh connection and authentication process.
 */
static void ssh_scp_init(void)
{
  if (scp_ssh_socket == INVALID_SOCKET)
    return;
  while (!back->sendok()) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(scp_ssh_socket, &readfds);
    if (select(1, &readfds, NULL, NULL, NULL) < 0)
      return; /* doom */
    select_result((WPARAM)scp_ssh_socket, (LPARAM)FD_READ);
  }
  using_sftp = !ssh_fallback_cmd;
}

/*
 *  Print an error message and exit after closing the SSH link.
 */
static void bump(char *fmt, ...)
{
  char str[0x100]; /* Make the size big enough */
  va_list ap;
  va_start(ap, fmt);
  strcpy(str, "Fatal: ");
  vsprintf(str + strlen(str), fmt, ap);
  va_end(ap);
  strcat(str, "\n");
  tell_str(stderr, str);
  errs++;

  if (back != NULL && back->socket() != NULL) {
    char ch;
    back->special(TS_EOF);
    ssh_scp_recv(&ch, 1);
  }

  if (gui_mode) {
    unsigned int msg_id = WM_RET_ERR_CNT;
    if (list)
      msg_id = WM_LS_RET_ERR_CNT;
    while (
        !PostMessage((HWND)atoi(gui_hwnd), msg_id, (WPARAM)errs, 0 /*lParam */))
      SleepEx(1000, TRUE);
  }

  cleanup_exit(1);
}

/*
 *  Open an SSH connection to user@host and execute cmd.
 */
static void do_cmd(char *host, char *user, char *cmd)
{
  char *err, *realhost;
  DWORD namelen;

  if (host == NULL || host[0] == '\0')
    bump("Empty host name");

  /* Try to load settings for this host */
  do_defaults(host, &cfg);
  if (cfg.host[0] == '\0') {
    /* No settings for this host; use defaults */
    do_defaults(NULL, &cfg);
    strncpy(cfg.host, host, sizeof(cfg.host) - 1);
    cfg.host[sizeof(cfg.host) - 1] = '\0';
    cfg.port = 22;
  }

  /*
   * Enact command-line overrides.
   */
  cmdline_run_saved();

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
   * Trim a colon suffix off the hostname if it's there.
   */
  cfg.host[strcspn(cfg.host, ":")] = '\0';

  /* Set username */
  if (user != NULL && user[0] != '\0') {
    strncpy(cfg.username, user, sizeof(cfg.username) - 1);
    cfg.username[sizeof(cfg.username) - 1] = '\0';
  } else if (cfg.username[0] == '\0') {
    namelen = 0;
    if (GetUserName(user, &namelen) == FALSE)
      bump("Empty user name");
    user = smalloc(namelen * sizeof(char));
    GetUserName(user, &namelen);
    if (verbose)
      tell_user(stderr, "Guessing user name: %s", user);
    strncpy(cfg.username, user, sizeof(cfg.username) - 1);
    cfg.username[sizeof(cfg.username) - 1] = '\0';
    free(user);
  }

  if (cfg.protocol != PROT_SSH)
    cfg.port = 22;

  if (portnumber)
    cfg.port = portnumber;

  /*
   * Disable scary things which shouldn't be enabled for simple
   * things like SCP and SFTP: agent forwarding, port forwarding,
   * X forwarding.
   */
  cfg.x11_forward = 0;
  cfg.agentfwd = 0;
  cfg.portfwd[0] = cfg.portfwd[1] = '\0';

  /*
   * Attempt to start the SFTP subsystem as a first choice,
   * falling back to the provided scp command if that fails.
   */
  strcpy(cfg.remote_cmd, "sftp");
  cfg.ssh_subsys = TRUE;
  cfg.remote_cmd_ptr2 = cmd;
  cfg.ssh_subsys2 = FALSE;
  cfg.nopty = TRUE;

  back = &ssh_backend;

  err = back->init(cfg.host, cfg.port, &realhost, 0);
  if (err != NULL)
    bump("ssh_init: %s", err);
  ssh_scp_init();
  if (verbose && realhost != NULL)
    tell_user(stderr, "Connected to %s\n", realhost);
  sfree(realhost);
}

/*
 *  Update statistic information about current file.
 */
static void print_stats(char *name,
                        unsigned long size,
                        unsigned long done,
                        time_t start,
                        time_t now)
{
  float ratebs;
  unsigned long eta;
  char etastr[10];
  int pct;
  int len;
  int elap;

  elap = (unsigned long)difftime(now, start);

  if (now > start)
    ratebs = (float)done / elap;
  else
    ratebs = (float)done;

  if (ratebs < 1.0)
    eta = size - done;
  else
    eta = (unsigned long)((size - done) / ratebs);
  sprintf(etastr, "%02ld:%02ld:%02ld", eta / 3600, (eta % 3600) / 60, eta % 60);

  pct = (int)(100 * (done * 1.0 / size));

  if (gui_mode)
    /* GUI Adaptation - Sept 2000 */
    gui_update_stats(name, size, pct, elap, done, eta, (unsigned long)ratebs);
  else {
    len = printf("\r%-25.25s | %10ld kB | %5.1f kB/s | ETA: %8s | %3d%%",
                 name,
                 done / 1024,
                 ratebs / 1024.0,
                 etastr,
                 pct);
    if (len < prev_stats_len)
      printf("%*s", prev_stats_len - len, "");
    prev_stats_len = len;

    if (done == size)
      printf("\n");
  }
}

/*
 *  Find a colon in str and return a pointer to the colon.
 *  This is used to separate hostname from filename.
 */
static char *colon(char *str)
{
  /* We ignore a leading colon, since the hostname cannot be
     empty. We also ignore a colon as second character because
     of filenames like f:myfile.txt. */
  if (str[0] == '\0' || str[0] == ':' || str[1] == ':')
    return (NULL);
  while (*str != '\0' && *str != ':' && *str != '/' && *str != '\\')
    str++;
  if (*str == ':')
    return (str);
  else
    return (NULL);
}

/*
 * Return a pointer to the portion of str that comes after the last
 * slash (or backslash or colon, if `local' is TRUE).
 */
static char *stripslashes(char *str, int local)
{
  char *p;

  if (local) {
    p = strchr(str, ':');
    if (p)
      str = p + 1;
  }

  p = strrchr(str, '/');
  if (p)
    str = p + 1;

  if (local) {
    p = strrchr(str, '\\');
    if (p)
      str = p + 1;
  }

  return str;
}

/*
 * Determine whether a string is entirely composed of dots.
 */
static int is_dots(char *str)
{
  return str[strspn(str, ".")] == '\0';
}

/*
 *  Wait for a response from the other side.
 *  Return 0 if ok, -1 if error.
 */
static int response(void)
{
  char ch, resp, rbuf[2048];
  int p;

  if (ssh_scp_recv(&resp, 1) <= 0)
    bump("Lost connection");

  p = 0;
  switch (resp) {
  case 0: /* ok */
    return (0);
  default:
    rbuf[p++] = resp;
    /* fallthrough */
  case 1: /* error */
  case 2: /* fatal error */
    do {
      if (ssh_scp_recv(&ch, 1) <= 0)
        bump("Protocol error: Lost connection");
      rbuf[p++] = ch;
    } while (p < sizeof(rbuf) && ch != '\n');
    rbuf[p - 1] = '\0';
    if (resp == 1)
      tell_user(stderr, "%s\n", rbuf);
    else
      bump("%s", rbuf);
    errs++;
    return (-1);
  }
}

int sftp_recvdata(char *buf, int len)
{
  return ssh_scp_recv(buf, len);
}
int sftp_senddata(char *buf, int len)
{
  back->send((unsigned char *)buf, len);
  return 1;
}

/* ----------------------------------------------------------------------
 * sftp-based replacement for the hacky `pscp -ls'.
 */
static int sftp_ls_compare(const void *av, const void *bv)
{
  const struct fxp_name *a = (const struct fxp_name *)av;
  const struct fxp_name *b = (const struct fxp_name *)bv;
  return strcmp(a->filename, b->filename);
}
void scp_sftp_listdir(char *dirname)
{
  struct fxp_handle *dirh;
  struct fxp_names *names;
  struct fxp_name *ournames;
  int nnames, namesize;
  int i;

  if (!fxp_init()) {
    tell_user(stderr, "unable to initialise SFTP: %s", fxp_error());
    errs++;
    return;
  }

  printf("Listing directory %s\n", dirname);

  dirh = fxp_opendir(dirname);
  if (dirh == NULL) {
    printf("Unable to open %s: %s\n", dirname, fxp_error());
  } else {
    nnames = namesize = 0;
    ournames = NULL;

    while (1) {

      names = fxp_readdir(dirh);
      if (names == NULL) {
        if (fxp_error_type() == SSH_FX_EOF)
          break;
        printf("Reading directory %s: %s\n", dirname, fxp_error());
        break;
      }
      if (names->nnames == 0) {
        fxp_free_names(names);
        break;
      }

      if (nnames + names->nnames >= namesize) {
        namesize += names->nnames + 128;
        ournames = srealloc(ournames, namesize * sizeof(*ournames));
      }

      for (i = 0; i < names->nnames; i++)
        ournames[nnames++] = names->names[i];

      names->nnames = 0; /* prevent free_names */
      fxp_free_names(names);
    }
    fxp_close(dirh);

    /*
     * Now we have our filenames. Sort them by actual file
     * name, and then output the longname parts.
     */
    qsort(ournames, nnames, sizeof(*ournames), sftp_ls_compare);

    /*
     * And print them.
     */
    for (i = 0; i < nnames; i++)
      printf("%s\n", ournames[i].longname);
  }
}

/* ----------------------------------------------------------------------
 * Helper routines that contain the actual SCP protocol elements,
 * implemented both as SCP1 and SFTP.
 */

static struct scp_sftp_dirstack {
  struct scp_sftp_dirstack *next;
  struct fxp_name *names;
  int namepos, namelen;
  char *dirpath;
  char *wildcard;
  int matched_something; /* wildcard match set was non-empty */
} * scp_sftp_dirstack_head;
static char *scp_sftp_remotepath, *scp_sftp_currentname;
static char *scp_sftp_wildcard;
static int scp_sftp_targetisdir, scp_sftp_donethistarget;
static int scp_sftp_preserve, scp_sftp_recursive;
static unsigned long scp_sftp_mtime, scp_sftp_atime;
static int scp_has_times;
static struct fxp_handle *scp_sftp_filehandle;
static uint64 scp_sftp_fileoffset;

void scp_source_setup(char *target, int shouldbedir)
{
  if (using_sftp) {
    /*
     * Find out whether the target filespec is in fact a
     * directory.
     */
    struct fxp_attrs attrs;

    if (!fxp_init()) {
      tell_user(stderr, "unable to initialise SFTP: %s", fxp_error());
      errs++;
      return 1;
    }

    if (!fxp_stat(target, &attrs) ||
        !(attrs.flags & SSH_FILEXFER_ATTR_PERMISSIONS))
      scp_sftp_targetisdir = 0;
    else
      scp_sftp_targetisdir = (attrs.permissions & 0040000) != 0;

    if (shouldbedir && !scp_sftp_targetisdir) {
      bump("pscp: remote filespec %s: not a directory\n", target);
    }

    scp_sftp_remotepath = dupstr(target);

    scp_has_times = 0;
  } else {
    (void)response();
  }
}

int scp_send_errmsg(char *str)
{
  if (using_sftp) {
    /* do nothing; we never need to send our errors to the server */
  } else {
    back->send("\001", 1); /* scp protocol error prefix */
    back->send(str, strlen(str));
  }
  return 0; /* can't fail */
}

int scp_send_filetimes(unsigned long mtime, unsigned long atime)
{
  if (using_sftp) {
    scp_sftp_mtime = mtime;
    scp_sftp_atime = atime;
    scp_has_times = 1;
    return 0;
  } else {
    char buf[80];
    sprintf(buf, "T%lu 0 %lu 0\n", mtime, atime);
    back->send(buf, strlen(buf));
    return response();
  }
}

int scp_send_filename(char *name, unsigned long size, int modes)
{
  if (using_sftp) {
    char *fullname;
    if (scp_sftp_targetisdir) {
      fullname = dupcat(scp_sftp_remotepath, "/", name, NULL);
    } else {
      fullname = dupstr(scp_sftp_remotepath);
    }
    scp_sftp_filehandle =
        fxp_open(fullname, SSH_FXF_WRITE | SSH_FXF_CREAT | SSH_FXF_TRUNC);
    if (!scp_sftp_filehandle) {
      tell_user(stderr, "pscp: unable to open %s: %s", fullname, fxp_error());
      errs++;
      return 1;
    }
    scp_sftp_fileoffset = uint64_make(0, 0);
    sfree(fullname);
    return 0;
  } else {
    char buf[40];
    sprintf(buf, "C%04o %lu ", modes, size);
    back->send(buf, strlen(buf));
    back->send(name, strlen(name));
    back->send("\n", 1);
    return response();
  }
}

int scp_send_filedata(char *data, int len)
{
  if (using_sftp) {
    if (!scp_sftp_filehandle) {
      return 1;
    }
    if (!fxp_write(scp_sftp_filehandle, data, scp_sftp_fileoffset, len)) {
      tell_user(stderr, "error while writing: %s\n", fxp_error());
      errs++;
      return 1;
    }
    scp_sftp_fileoffset = uint64_add32(scp_sftp_fileoffset, len);
    return 0;
  } else {
    int bufsize = back->send(data, len);

    /*
     * If the network transfer is backing up - that is, the
     * remote site is not accepting data as fast as we can
     * produce it - then we must loop on network events until
     * we have space in the buffer again.
     */
    while (bufsize > MAX_SCP_BUFSIZE) {
      if (!scp_process_network_event())
        return 1;
      bufsize = back->sendbuffer();
    }

    return 0;
  }
}

int scp_send_finish(void)
{
  if (using_sftp) {
    struct fxp_attrs attrs;
    if (!scp_sftp_filehandle) {
      return 1;
    }
    if (scp_has_times) {
      attrs.flags = SSH_FILEXFER_ATTR_ACMODTIME;
      attrs.atime = scp_sftp_atime;
      attrs.mtime = scp_sftp_mtime;
      if (!fxp_fsetstat(scp_sftp_filehandle, attrs)) {
        tell_user(stderr, "unable to set file times: %s\n", fxp_error());
        errs++;
      }
    }
    fxp_close(scp_sftp_filehandle);
    scp_has_times = 0;
    return 0;
  } else {
    back->send("", 1);
    return response();
  }
}

char *scp_save_remotepath(void)
{
  if (using_sftp)
    return scp_sftp_remotepath;
  else
    return NULL;
}

void scp_restore_remotepath(char *data)
{
  if (using_sftp)
    scp_sftp_remotepath = data;
}

int scp_send_dirname(char *name, int modes)
{
  if (using_sftp) {
    char *fullname;
    char const *err;
    struct fxp_attrs attrs;
    if (scp_sftp_targetisdir) {
      fullname = dupcat(scp_sftp_remotepath, "/", name, NULL);
    } else {
      fullname = dupstr(scp_sftp_remotepath);
    }

    /*
     * We don't worry about whether we managed to create the
     * directory, because if it exists already it's OK just to
     * use it. Instead, we will stat it afterwards, and if it
     * exists and is a directory we will assume we were either
     * successful or it didn't matter.
     */
    if (!fxp_mkdir(fullname))
      err = fxp_error();
    else
      err = "server reported no error";
    if (!fxp_stat(fullname, &attrs) ||
        !(attrs.flags & SSH_FILEXFER_ATTR_PERMISSIONS) ||
        !(attrs.permissions & 0040000)) {
      tell_user(stderr, "unable to create directory %s: %s", fullname, err);
      errs++;
      return 1;
    }

    scp_sftp_remotepath = fullname;

    return 0;
  } else {
    char buf[40];
    sprintf(buf, "D%04o 0 ", modes);
    back->send(buf, strlen(buf));
    back->send(name, strlen(name));
    back->send("\n", 1);
    return response();
  }
}

int scp_send_enddir(void)
{
  if (using_sftp) {
    sfree(scp_sftp_remotepath);
    return 0;
  } else {
    back->send("E\n", 2);
    return response();
  }
}

/*
 * Yes, I know; I have an scp_sink_setup _and_ an scp_sink_init.
 * That's bad. The difference is that scp_sink_setup is called once
 * right at the start, whereas scp_sink_init is called to
 * initialise every level of recursion in the protocol.
 */
int scp_sink_setup(char *source, int preserve, int recursive)
{
  if (using_sftp) {
    char *newsource;

    if (!fxp_init()) {
      tell_user(stderr, "unable to initialise SFTP: %s", fxp_error());
      errs++;
      return 1;
    }
    /*
     * It's possible that the source string we've been given
     * contains a wildcard. If so, we must split the directory
     * away from the wildcard itself (throwing an error if any
     * wildcardness comes before the final slash) and arrange
     * things so that a dirstack entry will be set up.
     */
    newsource = smalloc(1 + strlen(source));
    if (!wc_unescape(newsource, source)) {
      /* Yes, here we go; it's a wildcard. Bah. */
      char *dupsource, *lastpart, *dirpart, *wildcard;
      dupsource = dupstr(source);
      lastpart = stripslashes(dupsource, 0);
      wildcard = dupstr(lastpart);
      *lastpart = '\0';
      if (*dupsource && dupsource[1]) {
        /*
         * The remains of dupsource are at least two
         * characters long, meaning the pathname wasn't
         * empty or just `/'. Hence, we remove the trailing
         * slash.
         */
        lastpart[-1] = '\0';
      } else if (!*dupsource) {
        /*
         * The remains of dupsource are _empty_ - the whole
         * pathname was a wildcard. Hence we need to
         * replace it with ".".
         */
        sfree(dupsource);
        dupsource = dupstr(".");
      }

      /*
       * Now we have separated our string into dupsource (the
       * directory part) and wildcard. Both of these will
       * need freeing at some point. Next step is to remove
       * wildcard escapes from the directory part, throwing
       * an error if it contains a real wildcard.
       */
      dirpart = smalloc(1 + strlen(dupsource));
      if (!wc_unescape(dirpart, dupsource)) {
        tell_user(stderr, "%s: multiple-level wildcards unsupported", source);
        errs++;
        sfree(dirpart);
        sfree(wildcard);
        sfree(dupsource);
        return 1;
      }

      /*
       * Now we have dirpart (unescaped, ie a valid remote
       * path), and wildcard (a wildcard). This will be
       * sufficient to arrange a dirstack entry.
       */
      scp_sftp_remotepath = dirpart;
      scp_sftp_wildcard = wildcard;
      sfree(dupsource);
    } else {
      scp_sftp_remotepath = newsource;
      scp_sftp_wildcard = NULL;
    }
    scp_sftp_preserve = preserve;
    scp_sftp_recursive = recursive;
    scp_sftp_donethistarget = 0;
    scp_sftp_dirstack_head = NULL;
  }
  return 0;
}

int scp_sink_init(void)
{
  if (!using_sftp) {
    back->send("", 1);
  }
  return 0;
}

#define SCP_SINK_FILE 1
#define SCP_SINK_DIR 2
#define SCP_SINK_ENDDIR 3
#define SCP_SINK_RETRY 4 /* not an action; just try again */
struct scp_sink_action {
  int action;                 /* FILE, DIR, ENDDIR */
  char *buf;                  /* will need freeing after use */
  char *name;                 /* filename or dirname (not ENDDIR) */
  int mode;                   /* access mode (not ENDDIR) */
  unsigned long size;         /* file size (not ENDDIR) */
  int settime;                /* 1 if atime and mtime are filled */
  unsigned long atime, mtime; /* access times for the file */
};

int scp_get_sink_action(struct scp_sink_action *act)
{
  if (using_sftp) {
    char *fname;
    int must_free_fname;
    struct fxp_attrs attrs;
    int ret;

    if (!scp_sftp_dirstack_head) {
      if (!scp_sftp_donethistarget) {
        /*
         * Simple case: we are only dealing with one file.
         */
        fname = scp_sftp_remotepath;
        must_free_fname = 0;
        scp_sftp_donethistarget = 1;
      } else {
        /*
         * Even simpler case: one file _which we've done_.
         * Return 1 (finished).
         */
        return 1;
      }
    } else {
      /*
       * We're now in the middle of stepping through a list
       * of names returned from fxp_readdir(); so let's carry
       * on.
       */
      struct scp_sftp_dirstack *head = scp_sftp_dirstack_head;
      while (head->namepos < head->namelen &&
             (is_dots(head->names[head->namepos].filename) ||
              (head->wildcard &&
               !wc_match(head->wildcard, head->names[head->namepos].filename))))
        head->namepos++; /* skip . and .. */
      if (head->namepos < head->namelen) {
        head->matched_something = 1;
        fname = dupcat(
            head->dirpath, "/", head->names[head->namepos++].filename, NULL);
        must_free_fname = 1;
      } else {
        /*
         * We've come to the end of the list; pop it off
         * the stack and return an ENDDIR action (or RETRY
         * if this was a wildcard match).
         */
        if (head->wildcard) {
          act->action = SCP_SINK_RETRY;
          if (!head->matched_something) {
            tell_user(stderr,
                      "pscp: wildcard '%s' matched "
                      "no files",
                      head->wildcard);
            errs++;
          }
          sfree(head->wildcard);

        } else {
          act->action = SCP_SINK_ENDDIR;
        }

        sfree(head->dirpath);
        sfree(head->names);
        scp_sftp_dirstack_head = head->next;
        sfree(head);

        return 0;
      }
    }

    /*
     * Now we have a filename. Stat it, and see if it's a file
     * or a directory.
     */
    ret = fxp_stat(fname, &attrs);
    if (!ret || !(attrs.flags & SSH_FILEXFER_ATTR_PERMISSIONS)) {
      tell_user(stderr,
                "unable to identify %s: %s",
                fname,
                ret ? "file type not supplied" : fxp_error());
      errs++;
      return 1;
    }

    if (attrs.permissions & 0040000) {
      struct scp_sftp_dirstack *newitem;
      struct fxp_handle *dirhandle;
      int nnames, namesize;
      struct fxp_name *ournames;
      struct fxp_names *names;

      /*
       * It's a directory. If we're not in recursive mode,
       * this merits a complaint (which is fatal if the name
       * was specified directly, but not if it was matched by
       * a wildcard).
       *
       * We skip this complaint completely if
       * scp_sftp_wildcard is set, because that's an
       * indication that we're not actually supposed to
       * _recursively_ transfer the dir, just scan it for
       * things matching the wildcard.
       */
      if (!scp_sftp_recursive && !scp_sftp_wildcard) {
        tell_user(stderr, "pscp: %s: is a directory", fname);
        errs++;
        if (must_free_fname)
          sfree(fname);
        if (scp_sftp_dirstack_head) {
          act->action = SCP_SINK_RETRY;
          return 0;
        } else {
          return 1;
        }
      }

      /*
       * Otherwise, the fun begins. We must fxp_opendir() the
       * directory, slurp the filenames into memory, return
       * SCP_SINK_DIR (unless this is a wildcard match), and
       * set targetisdir. The next time we're called, we will
       * run through the list of filenames one by one,
       * matching them against a wildcard if present.
       *
       * If targetisdir is _already_ set (meaning we're
       * already in the middle of going through another such
       * list), we must push the other (target,namelist) pair
       * on a stack.
       */
      dirhandle = fxp_opendir(fname);
      if (!dirhandle) {
        tell_user(
            stderr, "scp: unable to open directory %s: %s", fname, fxp_error());
        if (must_free_fname)
          sfree(fname);
        errs++;
        return 1;
      }
      nnames = namesize = 0;
      ournames = NULL;
      while (1) {
        int i;

        names = fxp_readdir(dirhandle);
        if (names == NULL) {
          if (fxp_error_type() == SSH_FX_EOF)
            break;
          tell_user(
              stderr, "scp: reading directory %s: %s\n", fname, fxp_error());
          if (must_free_fname)
            sfree(fname);
          sfree(ournames);
          errs++;
          return 1;
        }
        if (names->nnames == 0) {
          fxp_free_names(names);
          break;
        }
        if (nnames + names->nnames >= namesize) {
          namesize += names->nnames + 128;
          ournames = srealloc(ournames, namesize * sizeof(*ournames));
        }
        for (i = 0; i < names->nnames; i++)
          ournames[nnames++] = names->names[i];
        names->nnames = 0; /* prevent free_names */
        fxp_free_names(names);
      }
      fxp_close(dirhandle);

      newitem = smalloc(sizeof(struct scp_sftp_dirstack));
      newitem->next = scp_sftp_dirstack_head;
      newitem->names = ournames;
      newitem->namepos = 0;
      newitem->namelen = nnames;
      if (must_free_fname)
        newitem->dirpath = fname;
      else
        newitem->dirpath = dupstr(fname);
      if (scp_sftp_wildcard) {
        newitem->wildcard = scp_sftp_wildcard;
        newitem->matched_something = 0;
        scp_sftp_wildcard = NULL;
      } else {
        newitem->wildcard = NULL;
      }
      scp_sftp_dirstack_head = newitem;

      if (newitem->wildcard) {
        act->action = SCP_SINK_RETRY;
      } else {
        act->action = SCP_SINK_DIR;
        act->buf = dupstr(stripslashes(fname, 0));
        act->name = act->buf;
        act->size = 0; /* duhh, it's a directory */
        act->mode = 07777 & attrs.permissions;
        if (scp_sftp_preserve && (attrs.flags & SSH_FILEXFER_ATTR_ACMODTIME)) {
          act->atime = attrs.atime;
          act->mtime = attrs.mtime;
          act->settime = 1;
        } else
          act->settime = 0;
      }
      return 0;

    } else {
      /*
       * It's a file. Return SCP_SINK_FILE.
       */
      act->action = SCP_SINK_FILE;
      act->buf = dupstr(stripslashes(fname, 0));
      act->name = act->buf;
      if (attrs.flags & SSH_FILEXFER_ATTR_SIZE) {
        if (uint64_compare(attrs.size, uint64_make(0, ULONG_MAX)) > 0) {
          act->size = ULONG_MAX; /* *boggle* */
        } else
          act->size = attrs.size.lo;
      } else
        act->size = ULONG_MAX; /* no idea */
      act->mode = 07777 & attrs.permissions;
      if (scp_sftp_preserve && (attrs.flags & SSH_FILEXFER_ATTR_ACMODTIME)) {
        act->atime = attrs.atime;
        act->mtime = attrs.mtime;
        act->settime = 1;
      } else
        act->settime = 0;
      if (must_free_fname)
        scp_sftp_currentname = fname;
      else
        scp_sftp_currentname = dupstr(fname);
      return 0;
    }

  } else {
    int done = 0;
    int i, bufsize;
    int action;
    char ch;

    act->settime = 0;
    act->buf = NULL;
    bufsize = 0;

    while (!done) {
      if (ssh_scp_recv(&ch, 1) <= 0)
        return 1;
      if (ch == '\n')
        bump("Protocol error: Unexpected newline");
      i = 0;
      action = ch;
      do {
        if (ssh_scp_recv(&ch, 1) <= 0)
          bump("Lost connection");
        if (i >= bufsize) {
          bufsize = i + 128;
          act->buf = srealloc(act->buf, bufsize);
        }
        act->buf[i++] = ch;
      } while (ch != '\n');
      act->buf[i - 1] = '\0';
      switch (action) {
      case '\01': /* error */
        tell_user(stderr, "%s\n", act->buf);
        errs++;
        continue; /* go round again */
      case '\02': /* fatal error */
        bump("%s", act->buf);
      case 'E':
        back->send("", 1);
        act->action = SCP_SINK_ENDDIR;
        return 0;
      case 'T':
        if (sscanf(act->buf, "%ld %*d %ld %*d", &act->mtime, &act->atime) ==
            2) {
          act->settime = 1;
          back->send("", 1);
          continue; /* go round again */
        }
        bump("Protocol error: Illegal time format");
      case 'C':
      case 'D':
        act->action = (action == 'C' ? SCP_SINK_FILE : SCP_SINK_DIR);
        break;
      default:
        bump("Protocol error: Expected control record");
      }
      /*
       * We will go round this loop only once, unless we hit
       * `continue' above.
       */
      done = 1;
    }

    /*
     * If we get here, we must have seen SCP_SINK_FILE or
     * SCP_SINK_DIR.
     */
    if (sscanf(act->buf, "%o %lu %n", &act->mode, &act->size, &i) != 2)
      bump("Protocol error: Illegal file descriptor format");
    act->name = act->buf + i;
    return 0;
  }
}

int scp_accept_filexfer(void)
{
  if (using_sftp) {
    scp_sftp_filehandle = fxp_open(scp_sftp_currentname, SSH_FXF_READ);
    if (!scp_sftp_filehandle) {
      tell_user(stderr,
                "pscp: unable to open %s: %s",
                scp_sftp_currentname,
                fxp_error());
      errs++;
      return 1;
    }
    scp_sftp_fileoffset = uint64_make(0, 0);
    sfree(scp_sftp_currentname);
    return 0;
  } else {
    back->send("", 1);
    return 0; /* can't fail */
  }
}

int scp_recv_filedata(char *data, int len)
{
  if (using_sftp) {
    int actuallen =
        fxp_read(scp_sftp_filehandle, data, scp_sftp_fileoffset, len);
    if (actuallen == -1 && fxp_error_type() != SSH_FX_EOF) {
      tell_user(stderr, "pscp: error while reading: %s", fxp_error());
      errs++;
      return -1;
    }
    if (actuallen < 0)
      actuallen = 0;

    scp_sftp_fileoffset = uint64_add32(scp_sftp_fileoffset, actuallen);

    return actuallen;
  } else {
    return ssh_scp_recv(data, len);
  }
}

int scp_finish_filerecv(void)
{
  if (using_sftp) {
    fxp_close(scp_sftp_filehandle);
    return 0;
  } else {
    back->send("", 1);
    return response();
  }
}

/* ----------------------------------------------------------------------
 *  Send an error message to the other side and to the screen.
 *  Increment error counter.
 */
static void run_err(const char *fmt, ...)
{
  char str[2048];
  va_list ap;
  va_start(ap, fmt);
  errs++;
  strcpy(str, "scp: ");
  vsprintf(str + strlen(str), fmt, ap);
  strcat(str, "\n");
  scp_send_errmsg(str);
  tell_user(stderr, "%s", str);
  va_end(ap);
}

/*
 *  Execute the source part of the SCP protocol.
 */
static void source(char *src)
{
  unsigned long size;
  char *last;
  HANDLE f;
  DWORD attr;
  unsigned long i;
  unsigned long stat_bytes;
  time_t stat_starttime, stat_lasttime;

  attr = GetFileAttributes(src);
  if (attr == (DWORD)-1) {
    run_err("%s: No such file or directory", src);
    return;
  }

  if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    if (recursive) {
      /*
       * Avoid . and .. directories.
       */
      char *p;
      p = strrchr(src, '/');
      if (!p)
        p = strrchr(src, '\\');
      if (!p)
        p = src;
      else
        p++;
      if (!strcmp(p, ".") || !strcmp(p, ".."))
        /* skip . and .. */;
      else
        rsource(src);
    } else {
      run_err("%s: not a regular file", src);
    }
    return;
  }

  if ((last = strrchr(src, '/')) == NULL)
    last = src;
  else
    last++;
  if (strrchr(last, '\\') != NULL)
    last = strrchr(last, '\\') + 1;
  if (last == src && strchr(src, ':') != NULL)
    last = strchr(src, ':') + 1;

  f = CreateFile(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
  if (f == INVALID_HANDLE_VALUE) {
    run_err("%s: Cannot open file", src);
    return;
  }

  if (preserve) {
    FILETIME actime, wrtime;
    unsigned long mtime, atime;
    GetFileTime(f, NULL, &actime, &wrtime);
    TIME_WIN_TO_POSIX(actime, atime);
    TIME_WIN_TO_POSIX(wrtime, mtime);
    if (scp_send_filetimes(mtime, atime))
      return;
  }

  size = GetFileSize(f, NULL);
  if (verbose)
    tell_user(stderr, "Sending file %s, size=%lu", last, size);
  if (scp_send_filename(last, size, 0644))
    return;

  stat_bytes = 0;
  stat_starttime = time(NULL);
  stat_lasttime = 0;

  for (i = 0; i < size; i += 4096) {
    char transbuf[4096];
    DWORD j, k = 4096;

    if (i + k > size)
      k = size - i;
    if (!ReadFile(f, transbuf, k, &j, NULL) || j != k) {
      if (statistics)
        printf("\n");
      bump("%s: Read error", src);
    }
    if (scp_send_filedata(transbuf, k))
      bump("%s: Network error occurred", src);

    if (statistics) {
      stat_bytes += k;
      if (time(NULL) != stat_lasttime || i + k == size) {
        stat_lasttime = time(NULL);
        print_stats(last, size, stat_bytes, stat_starttime, stat_lasttime);
      }
    }
  }
  CloseHandle(f);

  (void)scp_send_finish();
}

/*
 *  Recursively send the contents of a directory.
 */
static void rsource(char *src)
{
  char *last, *findfile;
  char *save_target;
  HANDLE dir;
  WIN32_FIND_DATA fdat;
  int ok;

  if ((last = strrchr(src, '/')) == NULL)
    last = src;
  else
    last++;
  if (strrchr(last, '\\') != NULL)
    last = strrchr(last, '\\') + 1;
  if (last == src && strchr(src, ':') != NULL)
    last = strchr(src, ':') + 1;

  /* maybe send filetime */

  save_target = scp_save_remotepath();

  if (verbose)
    tell_user(stderr, "Entering directory: %s", last);
  if (scp_send_dirname(last, 0755))
    return;

  findfile = dupcat(src, "/*", NULL);
  dir = FindFirstFile(findfile, &fdat);
  ok = (dir != INVALID_HANDLE_VALUE);
  while (ok) {
    if (strcmp(fdat.cFileName, ".") == 0 || strcmp(fdat.cFileName, "..") == 0) {
      /* ignore . and .. */
    } else {
      char *foundfile = dupcat(src, "/", fdat.cFileName, NULL);
      source(foundfile);
      sfree(foundfile);
    }
    ok = FindNextFile(dir, &fdat);
  }
  FindClose(dir);
  sfree(findfile);

  (void)scp_send_enddir();

  scp_restore_remotepath(save_target);
}

/*
 * Execute the sink part of the SCP protocol.
 */
static void sink(char *targ, char *src)
{
  char *destfname;
  int targisdir = 0;
  int exists;
  DWORD attr;
  HANDLE f;
  unsigned long received;
  int wrerror = 0;
  unsigned long stat_bytes;
  time_t stat_starttime, stat_lasttime;
  char *stat_name;

  attr = GetFileAttributes(targ);
  if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
    targisdir = 1;

  if (targetshouldbedirectory && !targisdir)
    bump("%s: Not a directory", targ);

  scp_sink_init();
  while (1) {
    struct scp_sink_action act;
    if (scp_get_sink_action(&act))
      return;

    if (act.action == SCP_SINK_ENDDIR)
      return;

    if (act.action == SCP_SINK_RETRY)
      continue;

    if (targisdir) {
      /*
       * Prevent the remote side from maliciously writing to
       * files outside the target area by sending a filename
       * containing `../'. In fact, it shouldn't be sending
       * filenames with any slashes or colons in at all; so
       * we'll find the last slash, backslash or colon in the
       * filename and use only the part after that. (And
       * warn!)
       *
       * In addition, we also ensure here that if we're
       * copying a single file and the target is a directory
       * (common usage: `pscp host:filename .') the remote
       * can't send us a _different_ file name. We can
       * distinguish this case because `src' will be non-NULL
       * and the last component of that will fail to match
       * (the last component of) the name sent.
       *
       * Well, not always; if `src' is a wildcard, we do
       * expect to get back filenames that don't correspond
       * exactly to it. Ideally in this case, we would like
       * to ensure that the returned filename actually
       * matches the wildcard pattern - but one of SCP's
       * protocol infelicities is that wildcard matching is
       * done at the server end _by the server's rules_ and
       * so in general this is infeasible. Hence, we only
       * accept filenames that don't correspond to `src' if
       * unsafe mode is enabled or we are using SFTP (which
       * resolves remote wildcards on the client side and can
       * be trusted).
       */
      char *striptarget, *stripsrc;

      striptarget = stripslashes(act.name, 1);
      if (striptarget != act.name) {
        tell_user(stderr,
                  "warning: remote host sent a compound"
                  " pathname '%s'",
                  act.name);
        tell_user(stderr, "         renaming local file to '%s'", striptarget);
      }

      /*
       * Also check to see if the target filename is '.' or
       * '..', or indeed '...' and so on because Windows
       * appears to interpret those like '..'.
       */
      if (is_dots(striptarget)) {
        bump("security violation: remote host attempted to write to"
             " a '.' or '..' path!");
      }

      if (src) {
        stripsrc = stripslashes(src, 1);
        if (strcmp(striptarget, stripsrc) && !using_sftp && !scp_unsafe_mode) {
          tell_user(stderr,
                    "warning: remote host tried to write "
                    "to a file called '%s'",
                    striptarget);
          tell_user(stderr,
                    "         when we requested a file "
                    "called '%s'.",
                    stripsrc);
          tell_user(stderr,
                    "         If this is a wildcard, "
                    "consider upgrading to SSH 2 or using");
          tell_user(stderr,
                    "         the '-unsafe' option. Renaming"
                    " of this file has been disallowed.");
          /* Override the name the server provided with our own. */
          striptarget = stripsrc;
        }
      }

      if (targ[0] != '\0')
        destfname = dupcat(targ, "\\", striptarget, NULL);
      else
        destfname = dupstr(striptarget);
    } else {
      /*
       * In this branch of the if, the target area is a
       * single file with an explicitly specified name in any
       * case, so there's no danger.
       */
      destfname = dupstr(targ);
    }
    attr = GetFileAttributes(destfname);
    exists = (attr != (DWORD)-1);

    if (act.action == SCP_SINK_DIR) {
      if (exists && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        run_err("%s: Not a directory", destfname);
        continue;
      }
      if (!exists) {
        if (!CreateDirectory(destfname, NULL)) {
          run_err("%s: Cannot create directory", destfname);
          continue;
        }
      }
      sink(destfname, NULL);
      /* can we set the timestamp for directories ? */
      continue;
    }

    f = CreateFile(destfname,
                   GENERIC_WRITE,
                   0,
                   NULL,
                   CREATE_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL,
                   0);
    if (f == INVALID_HANDLE_VALUE) {
      run_err("%s: Cannot create file", destfname);
      continue;
    }

    if (scp_accept_filexfer())
      return;

    stat_bytes = 0;
    stat_starttime = time(NULL);
    stat_lasttime = 0;
    stat_name = stripslashes(destfname, 1);

    received = 0;
    while (received < act.size) {
      char transbuf[4096];
      DWORD blksize, read, written;
      blksize = 4096;
      if (blksize > act.size - received)
        blksize = act.size - received;
      read = scp_recv_filedata(transbuf, blksize);
      if (read <= 0)
        bump("Lost connection");
      if (wrerror)
        continue;
      if (!WriteFile(f, transbuf, read, &written, NULL) || written != read) {
        wrerror = 1;
        /* FIXME: in sftp we can actually abort the transfer */
        if (statistics)
          printf("\r%-25.25s | %50s\n",
                 stat_name,
                 "Write error.. waiting for end of file");
        continue;
      }
      if (statistics) {
        stat_bytes += read;
        if (time(NULL) > stat_lasttime || received + read == act.size) {
          stat_lasttime = time(NULL);
          print_stats(
              stat_name, act.size, stat_bytes, stat_starttime, stat_lasttime);
        }
      }
      received += read;
    }
    if (act.settime) {
      FILETIME actime, wrtime;
      TIME_POSIX_TO_WIN(act.atime, actime);
      TIME_POSIX_TO_WIN(act.mtime, wrtime);
      SetFileTime(f, NULL, &actime, &wrtime);
    }

    CloseHandle(f);
    if (wrerror) {
      run_err("%s: Write error", destfname);
      continue;
    }
    (void)scp_finish_filerecv();
    sfree(destfname);
    sfree(act.buf);
  }
}

/*
 * We will copy local files to a remote server.
 */
static void toremote(int argc, char *argv[])
{
  char *src, *targ, *host, *user;
  char *cmd;
  int i;

  targ = argv[argc - 1];

  /* Separate host from filename */
  host = targ;
  targ = colon(targ);
  if (targ == NULL)
    bump("targ == NULL in toremote()");
  *targ++ = '\0';
  if (*targ == '\0')
    targ = ".";
  /* Substitute "." for emtpy target */

  /* Separate host and username */
  user = host;
  host = strrchr(host, '@');
  if (host == NULL) {
    host = user;
    user = NULL;
  } else {
    *host++ = '\0';
    if (*user == '\0')
      user = NULL;
  }

  if (argc == 2) {
    /* Find out if the source filespec covers multiple files
       if so, we should set the targetshouldbedirectory flag */
    HANDLE fh;
    WIN32_FIND_DATA fdat;
    if (colon(argv[0]) != NULL)
      bump("%s: Remote to remote not supported", argv[0]);
    fh = FindFirstFile(argv[0], &fdat);
    if (fh == INVALID_HANDLE_VALUE)
      bump("%s: No such file or directory\n", argv[0]);
    if (FindNextFile(fh, &fdat))
      targetshouldbedirectory = 1;
    FindClose(fh);
  }

  cmd = smalloc(strlen(targ) + 100);
  sprintf(cmd,
          "scp%s%s%s%s -t %s",
          verbose ? " -v" : "",
          recursive ? " -r" : "",
          preserve ? " -p" : "",
          targetshouldbedirectory ? " -d" : "",
          targ);
  do_cmd(host, user, cmd);
  sfree(cmd);

  scp_source_setup(targ, targetshouldbedirectory);

  for (i = 0; i < argc - 1; i++) {
    char *srcpath, *last;
    HANDLE dir;
    WIN32_FIND_DATA fdat;
    src = argv[i];
    if (colon(src) != NULL) {
      tell_user(stderr, "%s: Remote to remote not supported\n", src);
      errs++;
      continue;
    }

    /*
     * Trim off the last pathname component of `src', to
     * provide the base pathname which will be prepended to
     * filenames returned from Find{First,Next}File.
     */
    srcpath = dupstr(src);
    last = stripslashes(srcpath, 1);
    *last = '\0';

    dir = FindFirstFile(src, &fdat);
    if (dir == INVALID_HANDLE_VALUE) {
      run_err("%s: No such file or directory", src);
      continue;
    }
    do {
      char *filename;
      /*
       * Ensure that . and .. are never matched by wildcards,
       * but only by deliberate action.
       */
      if (!strcmp(fdat.cFileName, ".") || !strcmp(fdat.cFileName, "..")) {
        /*
         * Find*File has returned a special dir. We require
         * that _either_ `src' ends in a backslash followed
         * by that string, _or_ `src' is precisely that
         * string.
         */
        int len = strlen(src), dlen = strlen(fdat.cFileName);
        if (len == dlen && !strcmp(src, fdat.cFileName)) {
          /* ok */;
        } else if (len > dlen + 1 && src[len - dlen - 1] == '\\' &&
                   !strcmp(src + len - dlen, fdat.cFileName)) {
          /* ok */;
        } else
          continue; /* ignore this one */
      }
      filename = dupcat(srcpath, fdat.cFileName, NULL);
      source(filename);
      sfree(filename);
    } while (FindNextFile(dir, &fdat));
    FindClose(dir);
    sfree(srcpath);
  }
}

/*
 *  We will copy files from a remote server to the local machine.
 */
static void tolocal(int argc, char *argv[])
{
  char *src, *targ, *host, *user;
  char *cmd;

  if (argc != 2)
    bump("More than one remote source not supported");

  src = argv[0];
  targ = argv[1];

  /* Separate host from filename */
  host = src;
  src = colon(src);
  if (src == NULL)
    bump("Local to local copy not supported");
  *src++ = '\0';
  if (*src == '\0')
    src = ".";
  /* Substitute "." for empty filename */

  /* Separate username and hostname */
  user = host;
  host = strrchr(host, '@');
  if (host == NULL) {
    host = user;
    user = NULL;
  } else {
    *host++ = '\0';
    if (*user == '\0')
      user = NULL;
  }

  cmd = smalloc(strlen(src) + 100);
  sprintf(cmd,
          "scp%s%s%s%s -f %s",
          verbose ? " -v" : "",
          recursive ? " -r" : "",
          preserve ? " -p" : "",
          targetshouldbedirectory ? " -d" : "",
          src);
  do_cmd(host, user, cmd);
  sfree(cmd);

  if (scp_sink_setup(src, preserve, recursive))
    return;

  sink(targ, src);
}

/*
 *  We will issue a list command to get a remote directory.
 */
static void get_dir_list(int argc, char *argv[])
{
  char *src, *host, *user;
  char *cmd, *p, *q;
  char c;

  src = argv[0];

  /* Separate host from filename */
  host = src;
  src = colon(src);
  if (src == NULL)
    bump("Local to local copy not supported");
  *src++ = '\0';
  if (*src == '\0')
    src = ".";
  /* Substitute "." for empty filename */

  /* Separate username and hostname */
  user = host;
  host = strrchr(host, '@');
  if (host == NULL) {
    host = user;
    user = NULL;
  } else {
    *host++ = '\0';
    if (*user == '\0')
      user = NULL;
  }

  cmd = smalloc(4 * strlen(src) + 100);
  strcpy(cmd, "ls -la '");
  p = cmd + strlen(cmd);
  for (q = src; *q; q++) {
    if (*q == '\'') {
      *p++ = '\'';
      *p++ = '\\';
      *p++ = '\'';
      *p++ = '\'';
    } else {
      *p++ = *q;
    }
  }
  *p++ = '\'';
  *p = '\0';

  do_cmd(host, user, cmd);
  sfree(cmd);

  if (using_sftp) {
    scp_sftp_listdir(src);
  } else {
    while (ssh_scp_recv(&c, 1) > 0)
      tell_char(stdout, c);
  }
}

/*
 *  Initialize the Win$ock driver.
 */
static void init_winsock(void)
{
  WORD winsock_ver;
  WSADATA wsadata;

  winsock_ver = MAKEWORD(1, 1);
  if (WSAStartup(winsock_ver, &wsadata))
    bump("Unable to initialise WinSock");
  if (LOBYTE(wsadata.wVersion) != 1 || HIBYTE(wsadata.wVersion) != 1)
    bump("WinSock version is incompatible with 1.1");
}

/*
 *  Short description of parameters.
 */
static void usage(void)
{
  printf("PuTTY Secure Copy client\n");
  printf("%s\n", ver);
  printf("Usage: pscp [options] [user@]host:source target\n");
  printf("       pscp [options] source [source...] [user@]host:target\n");
  printf("       pscp [options] -ls user@host:filespec\n");
  printf("Options:\n");
  printf("  -p        preserve file attributes\n");
  printf("  -q        quiet, don't show statistics\n");
  printf("  -r        copy directories recursively\n");
  printf("  -v        show verbose messages\n");
  printf("  -load sessname  Load settings from saved session\n");
  printf("  -P port   connect to specified port\n");
  printf("  -l user   connect with specified username\n");
  printf("  -pw passw login with specified password\n");
  printf("  -1 -2     force use of particular SSH protocol version\n");
  printf("  -C        enable compression\n");
  printf("  -i key    private key file for authentication\n");
  printf("  -batch    disable all interactive prompts\n");
  printf("  -unsafe   allow server-side wildcards (DANGEROUS)\n");
#if 0
    /*
     * -gui is an internal option, used by GUI front ends to get
     * pscp to pass progress reports back to them. It's not an
     * ordinary user-accessible option, so it shouldn't be part of
     * the command-line help. The only people who need to know
     * about it are programmers, and they can read the source.
     */
    printf
	("  -gui hWnd GUI mode with the windows handle for receiving messages\n");
#endif
  cleanup_exit(1);
}

void cmdline_error(char *p, ...)
{
  va_list ap;
  fprintf(stderr, "pscp: ");
  va_start(ap, p);
  vfprintf(stderr, p, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

/*
 *  Main program (no, really?)
 */
int main(int argc, char *argv[])
{
  int i;

  default_protocol = PROT_TELNET;

  flags = FLAG_STDERR;
  cmdline_tooltype = TOOLTYPE_FILETRANSFER;
  ssh_get_line = &console_get_line;
  init_winsock();
  sk_init();

  for (i = 1; i < argc; i++) {
    int ret;
    if (argv[i][0] != '-')
      break;
    ret = cmdline_process_param(argv[i], i + 1 < argc ? argv[i + 1] : NULL, 1);
    if (ret == -2) {
      cmdline_error("option \"%s\" requires an argument", argv[i]);
    } else if (ret == 2) {
      i++; /* skip next argument */
    } else if (ret == 1) {
      /* We have our own verbosity in addition to `flags'. */
      if (flags & FLAG_VERBOSE)
        verbose = 1;
    } else if (strcmp(argv[i], "-r") == 0) {
      recursive = 1;
    } else if (strcmp(argv[i], "-p") == 0) {
      preserve = 1;
    } else if (strcmp(argv[i], "-q") == 0) {
      statistics = 0;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0) {
      usage();
    } else if (strcmp(argv[i], "-gui") == 0 && i + 1 < argc) {
      gui_hwnd = argv[++i];
      gui_mode = 1;
      console_batch_mode = TRUE;
    } else if (strcmp(argv[i], "-ls") == 0) {
      list = 1;
    } else if (strcmp(argv[i], "-batch") == 0) {
      console_batch_mode = 1;
    } else if (strcmp(argv[i], "-unsafe") == 0) {
      scp_unsafe_mode = 1;
    } else if (strcmp(argv[i], "--") == 0) {
      i++;
      break;
    } else
      usage();
  }
  argc -= i;
  argv += i;
  back = NULL;

  if (list) {
    if (argc != 1)
      usage();
    get_dir_list(argc, argv);

  } else {

    if (argc < 2)
      usage();
    if (argc > 2)
      targetshouldbedirectory = 1;

    if (colon(argv[argc - 1]) != NULL)
      toremote(argc, argv);
    else
      tolocal(argc, argv);
  }

  if (back != NULL && back->socket() != NULL) {
    char ch;
    back->special(TS_EOF);
    ssh_scp_recv(&ch, 1);
  }
  WSACleanup();
  random_save_seed();

  /* GUI Adaptation - August 2000 */
  if (gui_mode) {
    unsigned int msg_id = WM_RET_ERR_CNT;
    if (list)
      msg_id = WM_LS_RET_ERR_CNT;
    while (
        !PostMessage((HWND)atoi(gui_hwnd), msg_id, (WPARAM)errs, 0 /*lParam */))
      SleepEx(1000, TRUE);
  }
  return (errs == 0 ? 0 : 1);
}

/* end */
