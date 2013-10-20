/*
 * PLink - a command-line (stdin/stdout) variant of PuTTY.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#define PUTTY_DO_GLOBALS	   /* actually _define_ globals */
#include "putty.h"
#include "storage.h"
#include "tree234.h"

#define WM_AGENT_CALLBACK (WM_APP + 4)

#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define APPNAME		"PFwd"

#define WM_SYSTRAY	(WM_USER + 100)
#define WM_SYSTRAY2	(WM_USER + 101)
#define WM_CONNECTED	    (WM_USER + 102)
#define WM_DISCONNECTED	    (WM_USER + 103)
#define WM_CONNECTING	    (WM_USER + 104)
#define WM_APPENDLOG	(WM_USER + 105)

#define IDI_ICON	(200)
#define IDI_ICOND	(201)
#define IDD_ABOUT	(210)
#define IDC_VERSION	(211)

#define IDD_PASSWD	(220)
#define IDC_PASSKEY	(221)
#define IDC_PASSWD	(222)

#define IDD_DISCONNECT	(222)
#define IDC_TIMER	(222)
#define IDC_DETAILS	(223)
#define IDC_LOG	        (224)

#define IDM_CLOSE	(0x0010)
#define IDM_ABOUT	(0x0020)

#define XORPAT_SIZE	(256)
#define PWD_ENCODE	(1)
#define PWD_DECODE	(2)

typedef struct {
    HWND    hwnd;
    HANDLE  event;
    Conf    *conf;
} THREAD_PARAM;

static HINSTANCE    m_hInst;
static HANDLE	    m_hThread = NULL;
static THREAD_PARAM m_tParam;
static HWND	    m_hAboutBox;

static const char	    m_szEncTab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int	    m_nDecTab[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,	 0,  1,	 2,  3,	 4,  5,	 6,  7,	 8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
    };

struct agent_callback {
    void (*callback)(void *, void *, int);
    void *callback_ctx;
    void *data;
    int len;
};

static char log_buffer[4096] = {'\0'};
static int log_length = 0;

static void log_append(const char* log) {
    int len = strlen(log);
    if (len + 1 >= sizeof log_buffer) {
        const char* end = log + len;
        log += len - sizeof log_buffer;
        while (log < end && *log++ != '\n')
            ;
        len = end - log;
        log_length = 0;
    }else if (log_length + len + 1 >= sizeof log_buffer) {
        int pos = log_length - sizeof log_buffer;
        while (pos < log_length && log_buffer[pos++] != '\n')
            ;
        if (log_length > pos) {
            log_length -= pos;
            memcpy(log_buffer, log_buffer + pos, log_length);
        }
    }
    if (len > 0) {
        strcpy(log_buffer + log_length, log);
        log_length += len;
    }
}

static int auto_connect_interval;
static int auto_connect_max_count;
static int auto_connect_count;

static int CALLBACK DisconnectedProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BOOL l10nSetDlgItemText(HWND dialog, int id, LPCSTR text);
    static int showlog;
    static int starttime;

    switch (msg) {
    case WM_INITDIALOG:
	showlog = 0;
	starttime = GetTickCount();
	if (auto_connect_count < auto_connect_max_count)
	    SetTimer(hwnd, 1, 500, NULL);
	SetDlgItemText(hwnd, IDC_LOG, log_buffer);
	return 1;

    case WM_COMMAND:
	if (wParam == MAKEWPARAM(IDOK, BN_CLICKED) || wParam == MAKEWPARAM(IDCANCEL, BN_CLICKED)) {
	    EndDialog(hwnd, LOWORD(wParam));
	    auto_connect_count = 0;
	    return 1;
	}else if (wParam == MAKEWPARAM(IDC_DETAILS, BN_CLICKED)) {
	    RECT rc = {0, 0, 188, 132};
	    KillTimer(hwnd, 1);
	    SetDlgItemText(hwnd, IDC_TIMER, "");
	    if (showlog) {
		rc.bottom = 60;
		l10nSetDlgItemText(hwnd, IDC_DETAILS, "&Details >>");
	    }else{
		l10nSetDlgItemText(hwnd, IDC_DETAILS, "&Details <<");
	    }
	    MapDialogRect(hwnd, &rc);
	    AdjustWindowRect(&rc, GetWindowLong(hwnd, GWL_STYLE), FALSE);
	    SetWindowPos(hwnd, NULL, 0, 0, rc.right - rc. left, rc.bottom - rc.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	    showlog = !showlog;
	}
	break;
    case WM_TIMER:
	{
	    int rest = auto_connect_interval - (GetTickCount() - starttime) / 1000;
	    if (rest <= 0) {
		auto_connect_count++;
		KillTimer(hwnd, 1);
		EndDialog(hwnd, IDOK);
	    }else if (rest > 1){
		char buffer[1024];
		sprintf(buffer, "Connect automatically after %d seconds...", rest);
		SetDlgItemText(hwnd, IDC_TIMER, buffer);
	    }else{
		l10nSetDlgItemText(hwnd, IDC_TIMER, "Connect automatically soon...");
	    }
	}
    }
    return 0;
}

void appendLog(const char* p) {
    SendMessage(m_tParam.hwnd, WM_APPENDLOG, 0, (LPARAM) p);
}
void vappendLog(const char* p, va_list ap) {
    char string[1024];
    wvsprintf(string, p, ap);
    appendLog(string);
}
void appendLogF(const char* p, ...) {
    va_list ap;
    va_start(ap, p);
    vappendLog(p, ap);
    va_end(ap);
}

struct thread_local_data {
	WSAEVENT netevent;
	void* backhandle;
	SOCKET* sklist;
};

static DWORD thread_local;

void exit_thread(int exitcode) {
    struct thread_local_data* tld = (struct thread_local_data*) TlsGetValue(thread_local);
    console_provide_logctx(NULL);
    ssh_backend.free(tld->backhandle);
    sfree(tld->sklist);
    CloseHandle(tld->netevent);
    cleanup_exit(exitcode);
}

void fatalbox(char *p, ...)
{
    char buffer[1024];
    va_list ap;
    wsprintf(buffer, "FATAL ERROR\n%s\n", p);
    va_start(ap, p);
    vappendLog(buffer, ap);
    va_end(ap);
    exit_thread(1);
}
void modalfatalbox(char *p, ...)
{
    char buffer[1024];
    va_list ap;
    va_start(ap, p);
    wvsprintf(buffer, p, ap);
    va_end(ap);
    MessageBox(NULL, buffer, "Fatal ERROR", MB_OK | MB_ICONERROR);
    exit_thread(1);
}
void nonfatal(char *p, ...)
{
  char buffer[1024];
  va_list ap;
  va_start(ap, p);
  wvsprintf(buffer, p, ap);
  va_end(ap);
  MessageBox(NULL, buffer, "ERROR", MB_OK | MB_ICONERROR);
  exit_thread(1);
}
void connection_fatal(void *frontend, char *p, ...)
{
    char buffer[1024];
    va_list ap;
    wsprintf(buffer, "FATAL Error\n%s\n", p);
    va_start(ap, p);
    vappendLog(buffer, ap);
    va_end(ap);
    exit_thread(1);
}
void cmdline_error(char *p, ...)
{
    char buffer[1024];
    va_list ap;
    va_start(ap, p);
    wvsprintf(buffer, p, ap);
    va_end(ap);
	MessageBox(NULL, buffer, APPNAME " Command Line Error", MB_OK | MB_ICONERROR);
    exit_thread(1);
}

int term_ldisc(Terminal *term, int mode)
{
    return FALSE;
}
void ldisc_update(void *frontend, int echo, int edit)
{
}

char *get_ttymode(void *frontend, const char *mode) { return NULL; }

int from_backend(void *frontend_handle, int is_stderr,
	 const char *data, int len)
{
    char* buffer = snewn(9 + len + 1 + 1, char);
    strncpy(buffer, is_stderr ? "STDERR : " : "STDOUT : ", 9);
    strncpy(buffer + 9, data, len);
    buffer[9 + len] = '\n';
    buffer[9 + len + 1] = '\0';
    appendLog(buffer);
    sfree(buffer);
    return 0;
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
    return FALSE;   /* do not respond to incoming EOF with outgoing */
}

void agent_schedule_callback(void (*callback)(void *, void *, int),
		 void *callback_ctx, void *data, int len)
{
    assert(!"We shouldn't be here");
}

char *do_select(SOCKET skt, int startup)
{
    struct thread_local_data* tld = (struct thread_local_data*) TlsGetValue(thread_local);
    int events;
    if (startup) {
	events = (FD_CONNECT | FD_READ | FD_WRITE |
	 FD_OOB | FD_CLOSE | FD_ACCEPT);
    } else {
	events = 0;
    }
    if (p_WSAEventSelect(skt, tld->netevent, events) == SOCKET_ERROR) {
	switch (p_WSAGetLastError()) {
	case WSAENETDOWN:
	    return "Network is down";
	default:
	    return "WSAEventSelect(): unknown error";
	}
    }
    return NULL;
}



DWORD WINAPI do_main(LPVOID lParam)
{
    THREAD_PARAM *param = (THREAD_PARAM *)lParam;
    int abort = 0;

    int skcount, sksize;
    int connopen;
    int connected = 0;
    int exitcode = 0;
    long now, next;
	struct thread_local_data tld;
	TlsSetValue(thread_local, &tld);

    tld.sklist = NULL;
    skcount = sksize = 0;

    sk_init();

    if (p_WSAEventSelect == NULL) {
	MessageBox(NULL, "PFwd requires WinSock 2!", APPNAME, MB_OK | MB_ICONERROR);
	return -1;
    }

    /*
     * Start up the connection.
     */
    tld.netevent = CreateEvent(NULL, FALSE, FALSE, NULL);
    {
	const char *error;
	char *realhost;
	/* nodelay is only useful if stdin is a character device (console) */
	int nodelay = conf_get_int(param->conf, CONF_tcp_nodelay) &&
	    (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR);


	error = ssh_backend.init(NULL, &tld.backhandle, param->conf,
                                 conf_get_str(param->conf, CONF_host),
                                 conf_get_int(param->conf, CONF_port),
                                 &realhost, nodelay,
                                 conf_get_int(param->conf, CONF_tcp_keepalives));
	if (error) {
	    appendLog("Unable to open connection:\n");
	    appendLog(error);
	    ExitThread(1);
	    return 1;
	}
	logctx = log_init(NULL, param->conf);
	ssh_backend.provide_logctx(tld.backhandle, logctx);
	console_provide_logctx(logctx);
	sfree(realhost);
    }
    connopen = 1;

    now = GETTICKCOUNT();

    while (1) {
	int nhandles;
	HANDLE *handles;
	int n;
	DWORD ticks;

	if (run_timers(now, &next)) {
	    ticks = next - GETTICKCOUNT();
	    if (ticks < 0) ticks = 0;  /* just in case */
	} else {
	    ticks = INFINITE;
	}

	handles = handle_get_events(&nhandles);
	handles = sresize(handles, nhandles+2, HANDLE);
	handles[nhandles] = tld.netevent;
	handles[nhandles+1] = param->event;
	n = MsgWaitForMultipleObjects(nhandles+2, handles, FALSE, ticks,
			  QS_POSTMESSAGE);
	if ((unsigned)(n - WAIT_OBJECT_0) < (unsigned)nhandles) {
	    handle_got_event(handles[n - WAIT_OBJECT_0]);
	    if (!connected && ssh_backend.ldisc(tld.backhandle, LD_ECHO)) {
		connected = 1;
		PostMessage(param->hwnd, WM_CONNECTED, 0, 0);
	    }
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
		tld.sklist = sresize(tld.sklist, sksize, SOCKET);
	    }

	    /* Retrieve the sockets into sklist. */
	    skcount = 0;
	    for (socket = first_socket(&socketstate);
		 socket != INVALID_SOCKET;
		 socket = next_socket(&socketstate)) {
		tld.sklist[skcount++] = socket;
	    }

	    /* Now we're done enumerating; go through the list. */
	    for (i = 0; i < skcount; i++) {
		WPARAM wp;
		socket = tld.sklist[i];
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
	    if (!connected && ssh_backend.ldisc(tld.backhandle, LD_ECHO)) {
		connected = 1;
		PostMessage(param->hwnd, WM_CONNECTED, 0, 0);
	    }
	} else if (n == WAIT_OBJECT_0 + nhandles+1) {
	    abort = 1;
	    break;
	} else if (n == WAIT_OBJECT_0 + nhandles+2) {
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

	if (!connopen || !ssh_backend.connected(tld.backhandle)) {
	    break;
	}
    }
    exitcode = ssh_backend.exitcode(tld.backhandle);
    if (!abort) {
	if (exitcode < 0) {
	    appendLog("Remote process exit code unavailable\r\n");
	}else{
	    appendLog("Disconnect from Server\r\n");
	}
    }
    exit_thread(exitcode);
    return exitcode;
}

static int Base64Encode(char *pszInput, int nLen, char *pszOutput, int nSize)
{
    int nInp, nOut;
    int nC1, nC2, nC3;

    ZeroMemory(pszOutput, nSize);
    nInp = 0;
    nOut = 0;
    while(nInp < nLen){
	nC1 = pszInput[nInp++] & 0xFF;
	if(nInp == nLen){
	    pszOutput[nOut++] = m_szEncTab[nC1 >> 2];
	    pszOutput[nOut++] = m_szEncTab[(nC1 & 0x03) << 4];
	    pszOutput[nOut++] = '=';
	    pszOutput[nOut++] = '=';
	    break;
	}
	nC2 = pszInput[nInp++];
	if(nInp == nLen){
	    pszOutput[nOut++] = m_szEncTab[nC1 >> 2];
	    pszOutput[nOut++] = m_szEncTab[((nC1 & 0x03) << 4) | ((nC2 & 0xF0) >> 4)];
	    pszOutput[nOut++] = m_szEncTab[(nC2 & 0x0F) << 2];
	    pszOutput[nOut++] = '=';
	    break;
	}
	nC3 = pszInput[nInp++];
	pszOutput[nOut++] = m_szEncTab[nC1 >> 2];
	pszOutput[nOut++] = m_szEncTab[((nC1 & 0x03) << 4) | ((nC2 & 0xF0) >> 4)];
	pszOutput[nOut++] = m_szEncTab[((nC2 & 0x0F) << 2) | ((nC3 & 0xC0) >> 6)];
	pszOutput[nOut++] = m_szEncTab[nC3 & 0x3F];
    }
    return nOut;
}

static int Base64Decode(char *pszInput, int nLen, char *pszOutput, int nSize)
{
    int nInp, nOut;
    int nC1, nC2, nC3, nC4;

    ZeroMemory(pszOutput, nSize);
    nInp = 0;
    nOut = 0;
    while(nInp < nLen){
	/* C1 */
	do{
	    nC1 = m_nDecTab[pszInput[nInp++] & 0xFF];
	}while(nInp < nLen && nC1 == -1);
	if(nC1 == -1){
	    break;
	}

	/* C2 */
	do{
	    nC2 = m_nDecTab[pszInput[nInp++] & 0xFF];
	}while(nInp < nLen && nC2 == -1);
	if(nC2 == -1){
	    break;
	}
	pszOutput[nOut++] = (char)((nC1 << 2) | ((nC2 & 0x30) >> 4));

	/* C3 */
	do{
	    nC3 = pszInput[nInp++] & 0xFF;
	    if(nC3 == '='){
		return nOut;
	    }
	    nC3 = m_nDecTab[nC3];
	}while(nInp < nLen && nC3 == -1);
	if(nC3 == -1){
	    break;
	}
	pszOutput[nOut++] = (char)(((nC2 & 0x0F) << 4) | ((nC3 & 0x3C) >> 2));

	/* C4 */
	do{
	    nC4 = pszInput[nInp++] & 0xFF;
	    if(nC4 == '='){
		return nOut;
	    }
	    nC4 = m_nDecTab[nC4];
	}while(nInp < nLen && nC4 == -1);
	if(nC4 == -1){
	    break;
	}
	pszOutput[nOut++] = (char)(((nC3 & 0x03) << 6) | nC4);
    }
    return nOut;
}

static void GenXorPattern(char *pszFname, char* m_szXorPat, int size)
{
    int fd;
    int i;

    ZeroMemory(m_szXorPat, size);
    if((fd = open(pszFname, _O_BINARY)) == -1){
	return;
    }
    read(fd, m_szXorPat, XORPAT_SIZE);
    close(fd);
    for(i = 0; i < XORPAT_SIZE; i++){
	if(i & 0x01){
	    m_szXorPat[i] = (char)((m_szXorPat[i] << 1) & 0xFF);
	}
    }
}

static void Password(char *pszFname, int nMode, char *pszInput, int cbInput, char *pszOutput, int cbOutput)
{
    char    m_szXorPat[XORPAT_SIZE + 1];
    char    *pBuf;
    char    nChr;
    char    nXor;
    char    nTmp;
    int	    nLen;
    int	    nIdx;
    int	    i, j;

    /* 暗号化キーの作成 */
    GenXorPattern(pszFname, m_szXorPat, sizeof m_szXorPat);

    ZeroMemory(pszOutput, cbOutput);
    if(nMode == PWD_ENCODE){
	pBuf = (char *)malloc(cbInput + 1);
	ZeroMemory(pBuf, cbInput + 1);
	nIdx = 0;
	nChr = 0;
	for(i = 0; i < cbInput; i++){
	    nChr ^= pszInput[i];
	    for(j = 0; j < XORPAT_SIZE; j++){
		nChr ^= m_szXorPat[j];
	    }
	    pBuf[nIdx++] = nChr;
	}
	nLen = Base64Encode(pBuf, cbInput, pszOutput, cbOutput);
    }else{
	nTmp = cbInput << 2;
	pBuf = (char *)malloc(nTmp);
	ZeroMemory(pBuf, nTmp);
	nLen = Base64Decode(pszInput, cbInput, pBuf, nTmp);
	nIdx = 0;
	nXor = 0;
	for(i = 0; i < nLen; i++){
	    nChr  = nTmp = pBuf[i];
	    nChr ^= nXor;
	    nXor  = nTmp;
	    for(j = 0; j < XORPAT_SIZE; j++){
		nChr ^= m_szXorPat[j];
	    }
	    pszOutput[nIdx++] = nChr;
	}
    }
    free(pBuf);
}

static void ChangeTrayIcon(HWND hwnd, int iconid)
{
    static NOTIFYICONDATA nid = {
	sizeof nid,
	NULL,
	IDI_ICON,
    };

    nid.hWnd = hwnd;
    if (iconid == 0) {
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    }else{
	nid.uFlags = 0;
    }
    if (nid.uCallbackMessage == 0) {
	nid.uCallbackMessage = WM_SYSTRAY;
	nid.uFlags |= NIF_MESSAGE;
    }
    if (nid.hIcon == NULL || iconid != 0) {
	nid.hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(iconid));
	nid.uFlags |= NIF_ICON;
    }
    if (nid.szTip[0] == '\0') {
	GetWindowText(hwnd, nid.szTip, sizeof nid.szTip);
	nid.uFlags |= NIF_TIP;
    }
    if (!Shell_NotifyIcon(NIM_MODIFY, &nid))
	Shell_NotifyIcon(NIM_ADD, &nid);
}

static void DelTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {
	sizeof nid,
	NULL,
	IDI_ICON,
    };

    nid.hWnd = hwnd;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

static int CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
	SetDlgItemText(hwnd, IDC_VERSION, ver);
	return 1;

    case WM_COMMAND:
	switch (LOWORD(wParam)) {
	case IDOK:
	    m_hAboutBox = NULL;
	    DestroyWindow(hwnd);
	    return 0;
	}
	return 0;

    case WM_CLOSE:
	m_hAboutBox = NULL;
	DestroyWindow(hwnd);
	return 0;
    }
    return 0;
}

struct PasswordDialogInfo {
    char* buffer;
    int length;
    const char* keyname;
};

static int CALLBACK PasswdProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static struct PasswordDialogInfo* info;
    switch (msg) {
    case WM_INITDIALOG:
	info = (struct PasswordDialogInfo*) lParam;
	SetDlgItemText(hwnd, IDC_PASSKEY, info->keyname);
	SetDlgItemText(hwnd, IDC_PASSWD, "");
	return 1;

    case WM_COMMAND:
	switch (LOWORD(wParam)) {
	case IDOK:
	    GetDlgItemText(hwnd, IDC_PASSWD, info->buffer, info->length);
	    EndDialog(hwnd, 1);
	    break;
	case IDCANCEL:
	    EndDialog(hwnd, -1);
	    break;
	}
	return 0;

    case WM_CLOSE:
	EndDialog(hwnd, 0);
	return 0;
    }
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BOOL l10nAppendMenu(HMENU menu, UINT flags, UINT id, LPCSTR text);
    static int menuinprogress;
    static UINT WM_TASKBARCREATED = 0;
    if (WM_TASKBARCREATED == 0)
	WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");

    switch(msg){
    case WM_CREATE:
	ChangeTrayIcon(hwnd, IDI_ICOND);
	PostMessage(hwnd, WM_CONNECTING, 0, 0);
	break;

    case WM_SYSTRAY:
	if(lParam == WM_RBUTTONUP){
	    POINT cursorpos;
	    GetCursorPos(&cursorpos);
	    PostMessage(hwnd, WM_SYSTRAY2, cursorpos.x, cursorpos.y);
	}
	break;

    case WM_SYSTRAY2:
	if(!menuinprogress){
	    static HMENU m_hPopupMenu = NULL;
	    if (m_hPopupMenu == NULL) {
		/* ポップアップメニューの生成 */
		m_hPopupMenu = CreatePopupMenu();
		l10nAppendMenu(m_hPopupMenu, MF_ENABLED, IDM_ABOUT, "&About");
		AppendMenu(m_hPopupMenu, MF_SEPARATOR, 0, 0);
		l10nAppendMenu(m_hPopupMenu, MF_ENABLED, IDM_CLOSE, "E&xit");
	    }
	    menuinprogress = 1;
	    SetForegroundWindow(hwnd);
	    TrackPopupMenu(
		m_hPopupMenu,
		TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
		wParam, lParam, 0, hwnd, NULL);
	    menuinprogress = 0;
	}
	break;
    case WM_CONNECTED:
	ChangeTrayIcon(hwnd, IDI_ICON);
	auto_connect_count = 0;
	break;
    case WM_DISCONNECTED:
	ChangeTrayIcon(hwnd, IDI_ICOND);
	if (DialogBox(m_hInst, MAKEINTRESOURCE(IDD_DISCONNECT), hwnd, DisconnectedProc) == IDOK) {
	    PostMessage(hwnd, WM_CONNECTING, 0, 0);
	}else{
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
	}
	break;
    case WM_CONNECTING:
	if (m_hThread == NULL) {
	    DWORD main_thread_id;
	    /* Plink 起動 */
	    m_hThread = CreateThread(NULL, 0, do_main, &m_tParam, 0, &main_thread_id);
	}
	break;
    case WM_APPENDLOG:
	log_append((const char*) lParam);
	break;

    case WM_COMMAND:
	switch(wParam & ~0xF){
	case IDM_CLOSE:
	    SetEvent(m_tParam.event);
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
	    break;

	case IDM_ABOUT:
	    if(!m_hAboutBox){
		m_hAboutBox = CreateDialog(m_hInst, MAKEINTRESOURCE(IDD_ABOUT), NULL, AboutProc);
		ShowWindow(m_hAboutBox, SW_SHOWNORMAL);
		SetForegroundWindow(m_hAboutBox);
		SetWindowPos(m_hAboutBox, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	    }
	    break;

	default:
	    break;
	}
	break;

    case WM_DESTROY:
	SetEvent(m_tParam.event);
	DelTrayIcon(hwnd);
	PostQuitMessage(0);
	return 0;

    default:
	if (msg == WM_TASKBARCREATED) {
	    ChangeTrayIcon(hwnd, 0);
	}
	break;

    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static int cmdline_get_line_password_available = 0;
static char cmdline_get_line_password[64];
static void set_cmdline_get_line_password(const char* password)
{
    if (password == NULL) {
        cmdline_get_line_password_available = 0;
        memset(cmdline_get_line_password, 0, sizeof cmdline_get_line_password);
    }else{
        cmdline_get_line_password_available = 1;
        strncpy(cmdline_get_line_password, password, sizeof cmdline_get_line_password);
        cmdline_get_line_password[sizeof cmdline_get_line_password - 1] = '\0';
    }
}

int get_userpass_input(prompts_t *p, unsigned char *in, int inlen) {

    static int tried_once = 0;

    /*
     * We only handle prompts which don't echo (which we assume to be
     * passwords), and (currently) we only cope with a password prompt
     * that comes in a prompt-set on its own.
     */
    if (in || p->n_prompts != 1 || p->prompts[0]->echo) {
	return -1;
    }
    if (!cmdline_get_line_password_available) {
        struct PasswordDialogInfo info = {
	    p->prompts[0]->result,
	    p->prompts[0]->resultsize,
	    p->prompts[0]->prompt,
        };
        return DialogBoxParam(m_hInst, MAKEINTRESOURCE(IDD_PASSWD), m_tParam.hwnd, PasswdProc, (LPARAM) &info) ;
    }

    /*
     * If we've tried once, return utter failure (no more passwords left
     * to try).
     */
    if (tried_once)
	return 0;

    strncpy(p->prompts[0]->result, cmdline_get_line_password, p->prompts[0]->resultsize);
    p->prompts[0]->result[p->prompts[0]->resultsize-1] = '\0';
    set_cmdline_get_line_password(NULL);
    tried_once = 1;
    return 1;
}

extern int use_inifile;
extern char inifile[2 * MAX_PATH + 10];

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG	    msg;

    /* インスタンスハンドルを保存 */
    m_hInst = hInstance;

    {
	HANDLE	hMutex;
	WNDCLASS wc;
	char	szCmdLine[512];
	char	fwd[64];
	char	key[4];
	int	i;
	char	*p;
	char	*end;

	int	port;
	int	protocol;
	int	compress;

	char m_szModFile[512];
	char m_szCurDir[512];
	char m_szIniFile[512];
	char m_szKeyFile[512];
	char m_szPasswd[64];
	char m_szUser[16];
	char m_szHost[256];
	char m_szSession[512];
	const char* sessionname;

	/* モジュールのファイル名を取得 (フルパス) */
	ZeroMemory(m_szModFile, sizeof(m_szModFile));
	GetModuleFileName(NULL, m_szModFile, sizeof(m_szModFile));

	/* モジュールのディレクトリを取得 */
	strcpy(m_szCurDir, m_szModFile);
	if((p = strrchr(m_szCurDir, '\\'))){
	    *p = '\0';
	}

	/* INIファイル名を取得 (フルパス) */
	if(lpCmdLine[0]){
	    if(lpCmdLine[0] == '"'){
		strcpy(szCmdLine, &lpCmdLine[1]);
		szCmdLine[ strlen(szCmdLine) - 1 ] = '\0';
	    }else{
		strcpy(szCmdLine, lpCmdLine);
	    }
	    if(szCmdLine[1] == ':'){
		strcpy(m_szIniFile, szCmdLine);
	    }else if(szCmdLine[0] == '\\'){
		strcpy(m_szIniFile, szCmdLine);
	    }else if((szCmdLine[0] == '.') && (szCmdLine[1] == '\\')){
		sprintf(m_szIniFile, "%s\\%s", m_szCurDir, &szCmdLine[2]);
	    }else{
		sprintf(m_szIniFile, "%s\\%s", m_szCurDir, szCmdLine);
	    }
	}else{
	    strcpy(m_szIniFile, m_szModFile);
	    if((p = strrchr(m_szIniFile, '.'))){
		*p = '\0';
	    }
	    strcat(m_szIniFile, ".ini");
	}

	/*
	 * Initialise port and protocol to sensible defaults. (These
	 * will be overridden by more or less anything.)
	 */
	default_protocol = PROT_SSH;
	default_port = ssh_backend.default_port;

	/* INIファイルの値を取得 */
	GetPrivateProfileString("SSH", "IniFile", "", inifile, sizeof(inifile), m_szIniFile);
	use_inifile = inifile[0] != '\0';
	GetPrivateProfileString("SSH", "Host", "\n:", m_szHost, sizeof(m_szHost), m_szIniFile);
	port = GetPrivateProfileInt("SSH", "Port", 0, m_szIniFile);
	protocol = GetPrivateProfileInt("SSH", "ProtocolVersion", 0, m_szIniFile);
	compress = GetPrivateProfileInt("SSH", "Compression", 0, m_szIniFile);
	GetPrivateProfileString("SSH", "User", "\n:", m_szUser, sizeof(m_szUser), m_szIniFile);
	GetPrivateProfileString("SSH", "Password", "", m_szPasswd, sizeof(m_szPasswd), m_szIniFile);
	GetPrivateProfileString("SSH", "PrivateKey", "\n:", m_szKeyFile, sizeof(m_szKeyFile), m_szIniFile);
	if (m_szHost[0] == '\n') {
	    MessageBox(NULL, "Hostname not specified!", APPNAME, MB_OK | MB_ICONERROR);
	    return -1;
	}
	strcpy(m_szSession, APPNAME);
	strcat(m_szSession, " - ");
	sessionname = m_szSession + strlen(m_szSession);
	if (m_szUser[0] != '\n') {
	    strcat(m_szSession, m_szUser);
	    strcat(m_szSession, "@");
	}
	strcat(m_szSession, m_szHost);
	if (port > 0) {
	    strcat(m_szSession, ":");
	    sprintf(m_szSession + strlen(m_szSession), "%d", port);
	}

	/* ２重起動の場合は終了させる */
	if(!(hMutex = CreateMutex(NULL, TRUE, APPNAME))){
	    return 0;
	}
	if(GetLastError() == ERROR_ALREADY_EXISTS){
	    HWND    hwnd;
	    if((hwnd = FindWindow(APPNAME, m_szSession))){
		SendMessage(hwnd, WM_CLOSE, 0, 0);
		return 0;
	    }
	}

	/* タスクトレイ用ウィンドウの生成 */
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hInst;
	wc.hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = APPNAME;
	RegisterClass(&wc);

	/* plink パラメータセット */
        m_tParam.conf = conf_new();
        conf_set_str(m_tParam.conf, CONF_host, "");
	do_defaults(m_szHost, m_tParam.conf);
        if (conf_get_str(m_tParam.conf, CONF_host)[0] == '\0') {
	    /* No settings for this host; use defaults */
            conf_set_str(m_tParam.conf, CONF_host, m_szHost);
            conf_set_int(m_tParam.conf, CONF_port, 22);
	}
	conf_set_int(m_tParam.conf, CONF_x11_forward, 0);
	conf_set_int(m_tParam.conf, CONF_agentfwd, 0);
	conf_set_int(m_tParam.conf, CONF_nopty, 1);
	conf_set_int(m_tParam.conf, CONF_ssh_no_shell, 1);
	if (port > 0)
	    conf_set_int(m_tParam.conf, CONF_port, port);
	if (protocol == 1) {
            /* ssh protocol 1 only */
	    conf_set_int(m_tParam.conf, CONF_sshprot, 0);
	} else if (protocol == 2) {
            /* ssh protocol 2 only */
	    conf_set_int(m_tParam.conf, CONF_sshprot, 3);
	}
	if (m_szUser[0] != '\n') {
            conf_set_str(m_tParam.conf, CONF_username, m_szUser);
	}
	/* プライベートキーファイル名を取得 (フルパス) */
	if (m_szKeyFile[0] == '\n'){
	    conf_set_filename(m_tParam.conf, CONF_keyfile, "");
	}else if(m_szKeyFile[1] == ':'){
	    conf_set_filename(m_tParam.conf, CONF_keyfile, m_szKeyFile);
	}else if(m_szKeyFile[0] == '\\'){
	    conf_set_filename(m_tParam.conf, CONF_keyfile, m_szKeyFile);
	}else if((m_szKeyFile[0] == '.') && (m_szKeyFile[1] == '\\')){
            char *tmp  = snewn(strlen(m_szCurDir) + strlen(&m_szKeyFile[2]) + 2, char);
	    sprintf(tmp, "%s\\%s", m_szCurDir, &m_szKeyFile[2]);
	    conf_set_filename(m_tParam.conf, CONF_keyfile, tmp);
            sfree(tmp);
	}else{
            char *tmp  = snewn(strlen(m_szCurDir) + strlen(m_szKeyFile) + 2, char);
	    sprintf(tmp, "%s\\%s", m_szCurDir, m_szKeyFile);
	    conf_set_filename(m_tParam.conf, CONF_keyfile, tmp);
            sfree(tmp);
	}

	/* パスワードをデコード */
	if(m_szPasswd[0] == '@'){
	    char pwd[64];
	    /* 平文パスワード ⇒ 暗号化パスワード（INIファイル書き込み）*/
	    Password(conf_get_filename(m_tParam.conf, CONF_keyfile),
                     PWD_ENCODE, &m_szPasswd[1], strlen(m_szPasswd) - 1,
                     pwd, sizeof(pwd));
	    WritePrivateProfileString("SSH", "Password", pwd, m_szIniFile);
	    set_cmdline_get_line_password(&m_szPasswd[1]);
	}else if (m_szPasswd[0] != '\0') {
	    char pwd[64];
	    /* 暗号化パスワード ⇒ 平文パスワード */
	    Password(conf_get_filename(m_tParam.conf, CONF_keyfile),
                     PWD_DECODE, m_szPasswd, strlen(m_szPasswd),
                     pwd, sizeof(pwd));
	    set_cmdline_get_line_password(pwd);
	}

	if (compress)
	    conf_set_int(m_tParam.conf, CONF_compression, 1);

        {
            char *key;
            while ((key = conf_get_str_nthstrkey(m_tParam.conf, CONF_portfwd, 0)) != NULL)
                conf_del_str_str(m_tParam.conf, CONF_portfwd, key);
        }

	i = 0;
	while (1) {
	    char type;
	    char* param;
	    int len;
	    sprintf(key, "%02d", ++i);
	    GetPrivateProfileString("FORWARD", key, "\n:", fwd, sizeof(fwd), m_szIniFile);
	    if (fwd[0] == '\n')
		break;
	    if (fwd[0] == 'D' || fwd[0] == 'L' || fwd[0] == 'R') {
		type = fwd[0];
		param = &fwd[1];
	    }else{
		type = 'L';
		param = fwd;
	    }
	    if (type != 'D') {
		char* p1 = strchr(param, ':');
		if (p1 != NULL) {
		    char* p2 = strchr(p1 + 1, ':');
		    if (p2 != NULL) {
			char* p3 = strchr(p2 + 1, ':');
			if (p3 != NULL)
			    p1 = p2;
			*p1 = '\0';
		    }
		}
                conf_set_str_str(m_tParam.conf, CONF_portfwd, fwd, p1 + 1);
	    } else {
		fwd[0] = 'L';
                conf_set_str_str(m_tParam.conf, CONF_portfwd, fwd, "D");
            }
	}
	if (conf_get_str_nthstrkey(m_tParam.conf, CONF_portfwd, 0) == NULL) {
	    MessageBox(NULL, "No forwarding setting specified!", APPNAME, MB_OK | MB_ICONERROR);
	    return -1;
	}

	thread_local = TlsAlloc();

	/* スレッドへのイベントオブジェクト作成 */
	m_tParam.event = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_tParam.hwnd = CreateWindow(
		    APPNAME,
		    m_szSession,
		    WS_OVERLAPPEDWINDOW,
		    CW_USEDEFAULT, CW_USEDEFAULT,
		    100, 100, NULL, NULL, m_hInst, NULL);
	ShowWindow(m_tParam.hwnd, SW_HIDE);

	auto_connect_interval = GetPrivateProfileInt("SSH", "AutoConnectInterval", 10, m_szIniFile);
	auto_connect_max_count = GetPrivateProfileInt("SSH", "AutoConnectMaxCount", 3, m_szIniFile);
	auto_connect_count = 0;
    }

    /* メッセージループ */
    m_hAboutBox = NULL;
    msg.message = WM_NULL;
    while (msg.message != WM_QUIT) {
	if (MsgWaitForMultipleObjects(1, &m_hThread, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0) {
	    CloseHandle(m_hThread);
	    m_hThread = NULL;
	    PostMessage(m_tParam.hwnd, WM_DISCONNECTED, 0, 0);
	}else{
	    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) && msg.message != WM_QUIT) {
		HWND top = msg.hwnd;
		while (top != NULL && (GetWindowLong(top, GWL_STYLE) & WS_CHILD) != 0)
		    top = GetParent(top);
		if (top != NULL && GetClassLong(top, GCW_ATOM) == 32770 && IsDialogMessage(top, &msg)) {
		}else{
		    TranslateMessage(&msg);
		    DispatchMessage(&msg);
		}
	    }
	}
    }

    SetEvent(m_tParam.event);
    WaitForSingleObject(m_hThread, INFINITE);

    cleanup_exit(0);

    return 0;
}
