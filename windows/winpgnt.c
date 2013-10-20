/*
 * Pageant: the PuTTY Authentication Agent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <tchar.h>
#include <malloc.h>

#define PUTTY_DO_GLOBALS

#include "putty.h"
#include "ssh.h"
#include "misc.h"
#include "tree234.h"

#include <shellapi.h>
#include <shlobj.h>

#ifndef NO_SECURITY
#include <aclapi.h>
#ifdef DEBUG_IPC
#define _WIN32_WINNT 0x0500            /* for ConvertSidToStringSid */
#include <sddl.h>
#endif
#endif

#define IDI_MAINICON 200
#define IDI_TRAYICON 201

#define WM_SYSTRAY   (WM_APP + 6)
#define WM_SYSTRAY2  (WM_APP + 7)
#define TID_ADDSYSTRAY  1

#define AGENT_COPYDATA_ID 0x804e50ba   /* random goop */

/*
 * FIXME: maybe some day we can sort this out ...
 */
#define AGENT_MAX_MSGLEN  8192

/* From MSDN: In the WM_SYSCOMMAND message, the four low-order bits of
 * wParam are used by Windows, and should be masked off, so we shouldn't
 * attempt to store information in them. Hence all these identifiers have
 * the low 4 bits clear. Also, identifiers should < 0xF000. */

#define IDM_CLOSE    0x0010
#define IDM_VIEWKEYS 0x0020
#define IDM_ADDKEY   0x0030
#define IDM_HELP     0x0040
#define IDM_ABOUT    0x0050
#define IDM_CONFIRM_REQUEST 0x0070

#define APPNAME "Pageant"

extern char ver[];

static HWND keylist;
static HWND aboutbox;
static HMENU systray_menu, session_menu;
static int already_running;

static int confirm_any_request;
static char *putty_path;

/* CWD for "add key" file requester. */
static filereq *keypath = NULL;

#define IDM_PUTTY         0x0060
#define IDM_SESSIONS_BASE 0x1000
#define IDM_SESSIONS_MAX  0x2000
#define PUTTY_REGKEY      "Software\\SimonTatham\\PuTTY\\Sessions"
#define PUTTY_DEFAULT     "Default%20Settings"
static int initial_menuitems_count;

static int use_inifile;
static char inifile[2 * MAX_PATH + 10] = {'\0'};
static const char HEADER[] = "Session:";

static int get_use_inifile(void)
{
    if (inifile[0] == '\0') {
    	char buf[10];
	char *p;
	GetModuleFileName(NULL, inifile, sizeof(inifile));
	if((p = strrchr(inifile, '\\'))){
	    *p = '\0';
	}
	strcat(inifile, "\\putty.ini");
	GetPrivateProfileString("Generic", "UseIniFile", "", buf, sizeof (buf), inifile);
	use_inifile = buf[0] == '1';
	if (!use_inifile) {
	    HMODULE module;
	    HRESULT (WINAPI* SHGetFolderPath)(HWND, int, HANDLE, DWORD, LPSTR);
	    module = LoadLibrary("shell32.dll");
	    SHGetFolderPath = (HRESULT (WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR)) GetProcAddress(module, "SHGetFolderPathA");
	    if (SHGetFolderPath == NULL) {
		FreeLibrary(module);
		module = LoadLibrary("SHFolder.dll");
		SHGetFolderPath = (HRESULT (WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR)) GetProcAddress(module, "SHGetFolderPathA");
	    }
	    if (SHGetFolderPath != NULL && SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, inifile))) {
		strcat(inifile, "\\PuTTY\\putty.ini");
		GetPrivateProfileString("Generic", "UseIniFile", "", buf, sizeof (buf), inifile);
		use_inifile = buf[0] == '1';
	    }
	    FreeLibrary(module);
	}
    }
    return use_inifile;
}

/*
 * Print a modal (Really Bad) message box and perform a fatal exit.
 */
void modalfatalbox(char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    buf = dupvprintf(fmt, ap);
    va_end(ap);
    MessageBox(hwnd, buf, "Pageant Fatal Error",
	       MB_SYSTEMMODAL | MB_ICONERROR | MB_OK);
    sfree(buf);
    exit(1);
}

/* Un-munge session names out of the registry. */
static void unmungestr(char *in, char *out, int outlen)
{
    while (*in) {
	if (*in == '%' && in[1] && in[2]) {
	    int i, j;

	    i = in[1] - '0';
	    i -= (i > 9 ? 7 : 0);
	    j = in[2] - '0';
	    j -= (j > 9 ? 7 : 0);

	    *out++ = (i << 4) + j;
	    if (!--outlen)
		return;
	    in += 3;
	} else {
	    *out++ = *in++;
	    if (!--outlen)
		return;
	}
    }
    *out = '\0';
    return;
}

static tree234 *rsakeys, *ssh2keys;

static int has_security;
#ifndef NO_SECURITY
DECL_WINDOWS_FUNCTION(extern, DWORD, GetSecurityInfo,
		      (HANDLE, SE_OBJECT_TYPE, SECURITY_INFORMATION,
		       PSID *, PSID *, PACL *, PACL *,
		       PSECURITY_DESCRIPTOR *));
#endif

/*
 * Forward references
 */
static void *make_keylist1(int *length);
static void *make_keylist2(int *length);
static void *get_keylist1(int *length);
static void *get_keylist2(int *length);

/*
 * We need this to link with the RSA code, because rsaencrypt()
 * pads its data with random bytes. Since we only use rsadecrypt()
 * and the signing functions, which are deterministic, this should
 * never be called.
 *
 * If it _is_ called, there is a _serious_ problem, because it
 * won't generate true random numbers. So we must scream, panic,
 * and exit immediately if that should happen.
 */
int random_byte(void)
{
    MessageBox(hwnd, "Internal Error", APPNAME, MB_OK | MB_ICONERROR);
    exit(0);
    /* this line can't be reached but it placates MSVC's warnings :-) */
    return 0;
}

/*
 * Blob structure for passing to the asymmetric SSH-2 key compare
 * function, prototyped here.
 */
struct blob {
    unsigned char *blob;
    int len;
};
static int cmpkeys_ssh2_asymm(void *av, void *bv);

struct PassphraseProcStruct {
    char **passphrase;
    char *comment;
    int save;
};

static tree234 *passphrases = NULL;

/* 
 * After processing a list of filenames, we want to forget the
 * passphrases.
 */
static void forget_passphrases(void)
{
    while (count234(passphrases) > 0) {
	char *pp = index234(passphrases, 0);
	smemclr(pp, strlen(pp));
	delpos234(passphrases, 0);
	free(pp);
    }
}

/*
 * Dialog-box function for the Licence box.
 */
static int CALLBACK LicenceProc(HWND hwnd, UINT msg,
				WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
      case WM_INITDIALOG:
	return 1;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	  case IDCANCEL:
	    EndDialog(hwnd, 1);
	    return 0;
	}
	return 0;
      case WM_CLOSE:
	EndDialog(hwnd, 1);
	return 0;
    }
    return 0;
}

/*
 * Dialog-box function for the About box.
 */
static int CALLBACK AboutProc(HWND hwnd, UINT msg,
			      WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
      case WM_INITDIALOG:
	SetDlgItemText(hwnd, 100, ver);
	return 1;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	  case IDCANCEL:
	    aboutbox = NULL;
	    DestroyWindow(hwnd);
	    return 0;
	  case 101:
	    EnableWindow(hwnd, 0);
	    DialogBox(hinst, MAKEINTRESOURCE(214), hwnd, LicenceProc);
	    EnableWindow(hwnd, 1);
	    SetActiveWindow(hwnd);
	    return 0;
	}
	return 0;
      case WM_CLOSE:
	aboutbox = NULL;
	DestroyWindow(hwnd);
	return 0;
    }
    return 0;
}

static HWND passphrase_box;

/*
 * Dialog-box function for the passphrase box.
 */
static int CALLBACK PassphraseProc(HWND hwnd, UINT msg,
				   WPARAM wParam, LPARAM lParam)
{
    static char **passphrase = NULL;
    struct PassphraseProcStruct *p;

    switch (msg) {
      case WM_INITDIALOG:
	passphrase_box = hwnd;
	/*
	 * Centre the window.
	 */
	{			       /* centre the window */
	    RECT rs, rd;
	    HWND hw;

	    hw = GetDesktopWindow();
	    if (GetWindowRect(hw, &rs) && GetWindowRect(hwnd, &rd))
		MoveWindow(hwnd,
			   (rs.right + rs.left + rd.left - rd.right) / 2,
			   (rs.bottom + rs.top + rd.top - rd.bottom) / 2,
			   rd.right - rd.left, rd.bottom - rd.top, TRUE);
	}

	SetForegroundWindow(hwnd);
	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	p = (struct PassphraseProcStruct *) lParam;
	SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR) lParam);
	passphrase = p->passphrase;
	if (p->comment)
	    SetDlgItemText(hwnd, 101, p->comment);
        burnstr(*passphrase);
        *passphrase = dupstr("");
	SetDlgItemText(hwnd, 102, *passphrase);
	return TRUE;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	    p = (struct PassphraseProcStruct*) GetWindowLongPtr(hwnd, DWLP_USER);
	    p->save = SendDlgItemMessage(hwnd, 103, BM_GETCHECK, 0, 0) == BST_CHECKED;
	    if (*passphrase)
		EndDialog(hwnd, 1);
	    else
		MessageBeep(0);
	    return 0;
	  case IDCANCEL:
	    EndDialog(hwnd, 0);
	    return 0;
	  case 102:		       /* edit box */
	    if ((HIWORD(wParam) == EN_CHANGE) && passphrase) {
                burnstr(*passphrase);
                *passphrase = GetDlgItemText_alloc(hwnd, 102);
	    }
	    return 0;
	}
	return 0;
      case WM_CLOSE:
	EndDialog(hwnd, 0);
	return 0;
    }
    return 0;
}

/*
 * Warn about the obsolescent key file format.
 */
void old_keyfile_warning(void)
{
    static const char mbtitle[] = "PuTTY Key File Warning";
    static const char message[] =
	"You are loading an SSH-2 private key which has an\n"
	"old version of the file format. This means your key\n"
	"file is not fully tamperproof. Future versions of\n"
	"PuTTY may stop supporting this private key format,\n"
	"so we recommend you convert your key to the new\n"
	"format.\n"
	"\n"
	"You can perform this conversion by loading the key\n"
	"into PuTTYgen and then saving it again.";

    MessageBox(hwnd, message, mbtitle, MB_OK);
}

/*
 * Update the visible key list.
 */
static void keylist_update(void)
{
    struct RSAKey *rkey;
    struct ssh2_userkey *skey;
    int i;

    if (keylist) {
	SendDlgItemMessage(keylist, 100, LB_RESETCONTENT, 0, 0);
	for (i = 0; NULL != (rkey = index234(rsakeys, i)); i++) {
	    char listentry[512], *p;
	    /*
	     * Replace two spaces in the fingerprint with tabs, for
	     * nice alignment in the box.
	     */
	    strcpy(listentry, "ssh1\t");
	    p = listentry + strlen(listentry);
	    rsa_fingerprint(p, sizeof(listentry) - (p - listentry), rkey);
	    p = strchr(listentry, ' ');
	    if (p)
		*p = '\t';
	    p = strchr(listentry, ' ');
	    if (p)
		*p = '\t';
	    SendDlgItemMessage(keylist, 100, LB_ADDSTRING,
			       0, (LPARAM) listentry);
	}
	for (i = 0; NULL != (skey = index234(ssh2keys, i)); i++) {
	    char *listentry, *p;
	    int fp_len;
	    /*
	     * Replace two spaces in the fingerprint with tabs, for
	     * nice alignment in the box.
	     */
	    p = skey->alg->fingerprint(skey->data);
            listentry = dupprintf("%s\t%s", p, skey->comment);
            fp_len = strlen(listentry);
            sfree(p);

	    p = strchr(listentry, ' ');
	    if (p && p < listentry + fp_len)
		*p = '\t';
	    p = strchr(listentry, ' ');
	    if (p && p < listentry + fp_len)
		*p = '\t';

	    SendDlgItemMessage(keylist, 100, LB_ADDSTRING, 0,
			       (LPARAM) listentry);
            sfree(listentry);
	}
	SendDlgItemMessage(keylist, 100, LB_SETCURSEL, (WPARAM) - 1, 0);
    }
}

/* generate random number by MT(http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/mt19937ar.html) */

/* Period parameters */  
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static void random_mask(char* buffer, int length) {
    /* static variables of MT */
    unsigned long mt[N]; /* the array for the state vector  */
    int mti=N+1; /* mti==N+1 means mt[N] is not initialized */
    unsigned long mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    char* dst = buffer;
    unsigned long s = 0xf92fedb5UL;
    /* initializes mt[N] with a seed */
    /* void init_genrand(unsigned long s) */
    {
	mt[0]= s & 0xffffffffUL;
	for (mti=1; mti<N; mti++) {
	    mt[mti] = 
		(1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
	    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
	    /* In the previous versions, MSBs of the seed affect   */
	    /* only MSBs of the array mt[].                        */
	    /* 2002/01/09 modified by Makoto Matsumoto             */
	    mt[mti] &= 0xffffffffUL;
	    /* for >32 bit machines */
	}
    }
    while (length-- > 0) {
	unsigned long y;
	/* generates a random number on [0,0xffffffff]-interval */
	/* unsigned long genrand_int32(void) */
	{
	    /* unsigned long y; */
	    /* static unsigned long mag01[2]={0x0UL, MATRIX_A}; */
	    /* mag01[x] = x * MATRIX_A  for x=0,1 */

	    if (mti >= N) { /* generate N words at one time */
		int kk;

		/* if (mti == N+1)   /* if init_genrand() has not been called, */
		    /* init_genrand(5489UL); /* a default initial seed is used */

		for (kk=0;kk<N-M;kk++) {
		    y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
		    mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		for (;kk<N-1;kk++) {
		    y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
		    mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
		mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

		mti = 0;
	    }
  
	    y = mt[mti++];

	    /* Tempering */
	    y ^= (y >> 11);
	    y ^= (y << 7) & 0x9d2c5680UL;
	    y ^= (y << 15) & 0xefc60000UL;
	    y ^= (y >> 18);

	    /* return y; */
	}
        *dst++ ^= (y >> 4);
    }
}

static int encrypto(char* original, char* buffer) {
    int length = strlen(original);
    if (buffer != NULL) {
	random_mask(original, length);
	while (length > 0) {
	    int n = (length < 3 ? length : 3);
	    base64_encode_atom(original, n, buffer);
	    original += n;
	    length -= n;
	    buffer += 4;
	}
	*buffer = '\0';
    }
    return (length + 2) / 3 * 4;
}

static int decrypto(const char* encrypted, char* buffer) {
    int encrypted_len = strlen(encrypted);
    if (buffer == NULL) {
        return encrypted_len * 3 / 4;
    }else{
	const char* src = encrypted;
	const char* src_end = encrypted + encrypted_len;
	char* dst = buffer;
	while (src < src_end) {
	    int k = base64_decode_atom((char*) src, dst);
	    src += 4;
	    dst += k;
	}
        random_mask(buffer, dst - buffer);
	*dst = '\0';
	return dst - buffer;
    }
}

static void save_passphrases(char* passphrase);

static void load_passphrases() {
    char key[16];
    char buffer[1024];
    int i = 1;
    HKEY hkey;
    char* original;
    int original_len;
    if (get_use_inifile()) {
	while (1) {
	    sprintf(key, "crypto%d", i++);
	    if (GetPrivateProfileString("Passphrases", key, "\n:", buffer, sizeof buffer, inifile) < sizeof buffer - 1) {
		if (buffer[0] == '\n')
		    break;
		original_len = decrypto(buffer, NULL);
		original = (char*) malloc(original_len + 1);
		if (original != NULL) {
		    decrypto(buffer, original);
		    addpos234(passphrases, original, 0);
		}
	    }
	}
	// convert from previous version data
	i = 1;
	while (1) {
	    sprintf(key, "%d", i++);
	    if (GetPrivateProfileString("Passphrases", key, "\n:", buffer, sizeof buffer, inifile) < sizeof buffer - 1) {
		if (buffer[0] == '\n')
		    break;
		addpos234(passphrases, strdup(buffer), 0);
		save_passphrases(buffer);
		WritePrivateProfileString("Passphrases", key, NULL, inifile);
	    }
	}
	return;
    }
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\Passphrases", &hkey) == ERROR_SUCCESS) {
        DWORD type, size;
        int ret;
        while (1) {
	    sprintf(key, "crypto%d", i++);
	    size = sizeof buffer;
	    ret = RegQueryValueEx(hkey, key, 0, &type, buffer, &size);
	    if (ret == ERROR_FILE_NOT_FOUND)
		break;
	    if (ret == ERROR_SUCCESS && type == REG_SZ) {
		original_len = decrypto(buffer, NULL);
		original = (char*) malloc(original_len + 1);
		if (original != NULL) {
		    decrypto(buffer, original);
		    addpos234(passphrases, original, 0);
		}
	    }
        }
	// convert from previous version data
        i = 1;
        while (1) {
	    sprintf(key, "%d", i++);
	    size = sizeof buffer;
	    ret = RegQueryValueEx(hkey, key, 0, &type, buffer, &size);
	    if (ret == ERROR_FILE_NOT_FOUND)
		break;
	    if (ret == ERROR_SUCCESS && type == REG_SZ) {
		addpos234(passphrases, strdup(buffer), 0);
		save_passphrases(buffer);
		RegDeleteValue(hkey, key);
	    }
        }
        RegCloseKey(hkey);
    }
}

static void save_passphrases(char* passphrase) {
    char key[16];
    char buffer[1024];
    int i = 1;
    HKEY hkey;
    int encrypted_len = encrypto(passphrase, NULL);
    char* encrypted = (char*) alloca(encrypted_len + 1);
    encrypto(passphrase, encrypted);
    if (get_use_inifile()) {
	while (1) {
	    sprintf(key, "crypto%d", i++);
	    if (GetPrivateProfileString("Passphrases", key, "\n:", buffer, sizeof buffer, inifile) < sizeof buffer - 1) {
		if (buffer[0] == '\n') {
		    WritePrivateProfileString("Passphrases", key, encrypted, inifile);
		    break;
		}
		if (strcmp(buffer, encrypted) == 0)
		    break;
	    }
	}
	return;
    }
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\Passphrases", &hkey) == ERROR_SUCCESS) {
        DWORD type, size;
        int ret;
        while (1) {
	    sprintf(key, "crypto%d", i++);
	    size = sizeof buffer;
	    ret = RegQueryValueEx(hkey, key, 0, &type, buffer, &size);
	    if (ret == ERROR_FILE_NOT_FOUND) {
		RegSetValueEx(hkey, key, 0, REG_SZ, encrypted, strlen(encrypted) + 1);
		break;
	    }
	    if (ret == ERROR_SUCCESS && type == REG_SZ && strcmp(buffer, encrypted) == 0)
		break;
        }
        RegCloseKey(hkey);
    }
}

/*
 * This function loads a key from a file and adds it.
 */
static void add_keyfile(Filename *filename)
{
    char *passphrase;
    struct RSAKey *rkey = NULL;
    struct ssh2_userkey *skey = NULL;
    int needs_pass;
    int ret;
    int attempts;
    char *comment;
    const char *error = NULL;
    int type;
    int original_pass;
    struct PassphraseProcStruct pps;
	
    type = key_type(filename);
    if (type != SSH_KEYTYPE_SSH1 && type != SSH_KEYTYPE_SSH2) {
	char *msg = dupprintf("Couldn't load this key (%s)",
			      key_type_to_str(type));
	message_box(msg, APPNAME, MB_OK | MB_ICONERROR,
		    HELPCTXID(errors_cantloadkey));
	sfree(msg);
	return;
    }

    /*
     * See if the key is already loaded (in the primary Pageant,
     * which may or may not be us).
     */
    {
	void *blob;
	unsigned char *keylist, *p;
	int i, nkeys, bloblen, keylistlen;

	if (type == SSH_KEYTYPE_SSH1) {
	    if (!rsakey_pubblob(filename, &blob, &bloblen, NULL, &error)) {
		char *msg = dupprintf("Couldn't load private key (%s)", error);
		message_box(msg, APPNAME, MB_OK | MB_ICONERROR,
			    HELPCTXID(errors_cantloadkey));
		sfree(msg);
		return;
	    }
	    keylist = get_keylist1(&keylistlen);
	} else {
	    unsigned char *blob2;
	    blob = ssh2_userkey_loadpub(filename, NULL, &bloblen,
					NULL, &error);
	    if (!blob) {
		char *msg = dupprintf("Couldn't load private key (%s)", error);
		message_box(msg, APPNAME, MB_OK | MB_ICONERROR,
			    HELPCTXID(errors_cantloadkey));
		sfree(msg);
		return;
	    }
	    /* For our purposes we want the blob prefixed with its length */
	    blob2 = snewn(bloblen+4, unsigned char);
	    PUT_32BIT(blob2, bloblen);
	    memcpy(blob2 + 4, blob, bloblen);
	    sfree(blob);
	    blob = blob2;

	    keylist = get_keylist2(&keylistlen);
	}
	if (keylist) {
	    if (keylistlen < 4) {
		MessageBox(hwnd, "Received broken key list?!", APPNAME,
			   MB_OK | MB_ICONERROR);
		return;
	    }
	    nkeys = toint(GET_32BIT(keylist));
	    if (nkeys < 0) {
		MessageBox(NULL, "Received broken key list?!", APPNAME,
			   MB_OK | MB_ICONERROR);
		return;
	    }
	    p = keylist + 4;
	    keylistlen -= 4;

	    for (i = 0; i < nkeys; i++) {
		if (!memcmp(blob, p, bloblen)) {
		    /* Key is already present; we can now leave. */
		    sfree(keylist);
		    sfree(blob);
		    return;
		}
		/* Now skip over public blob */
		if (type == SSH_KEYTYPE_SSH1) {
		    int n = rsa_public_blob_len(p, keylistlen);
		    if (n < 0) {
			MessageBox(hwnd, "Received broken key list?!", APPNAME,
				   MB_OK | MB_ICONERROR);
			return;
		    }
		    p += n;
		    keylistlen -= n;
		} else {
		    int n;
		    if (keylistlen < 4) {
			MessageBox(hwnd, "Received broken key list?!", APPNAME,
				   MB_OK | MB_ICONERROR);
			return;
		    }
		    n = toint(4 + GET_32BIT(p));
		    if (n < 0 || keylistlen < n) {
			MessageBox(hwnd, "Received broken key list?!", APPNAME,
				   MB_OK | MB_ICONERROR);
			return;
		    }
		    p += n;
		    keylistlen -= n;
		}
		/* Now skip over comment field */
		{
		    int n;
		    if (keylistlen < 4) {
			MessageBox(hwnd, "Received broken key list?!", APPNAME,
				   MB_OK | MB_ICONERROR);
			return;
		    }
		    n = toint(4 + GET_32BIT(p));
		    if (n < 0 || keylistlen < n) {
			MessageBox(hwnd, "Received broken key list?!", APPNAME,
				   MB_OK | MB_ICONERROR);
			return;
		    }
		    p += n;
		    keylistlen -= n;
		}
	    }

	    sfree(keylist);
	}

	sfree(blob);
    }

    error = NULL;
    if (type == SSH_KEYTYPE_SSH1)
	needs_pass = rsakey_encrypted(filename, &comment);
    else
	needs_pass = ssh2_userkey_encrypted(filename, &comment);
    attempts = 0;
    if (type == SSH_KEYTYPE_SSH1)
	rkey = snew(struct RSAKey);
    passphrase = NULL;
    original_pass = 0;
    do {
        burnstr(passphrase);
        passphrase = NULL;

	if (needs_pass) {
	    /* try all the remembered passphrases first */
	    char *pp = index234(passphrases, attempts);
	    if(pp) {
		passphrase = dupstr(pp);
	    } else {
		int dlgret;

                pps.passphrase = &passphrase;
                pps.comment = comment;
                pps.save = 0;

		original_pass = 1;
		dlgret = DialogBoxParam(hinst, MAKEINTRESOURCE(210),
					hwnd, PassphraseProc, (LPARAM) &pps);
		passphrase_box = NULL;
		if (!dlgret) {
		    if (comment)
			sfree(comment);
		    if (type == SSH_KEYTYPE_SSH1)
			sfree(rkey);
		    return;		       /* operation cancelled */
		}
	    }
	} else
	    passphrase = dupstr("");

	if (type == SSH_KEYTYPE_SSH1)
	    ret = loadrsakey(filename, rkey, passphrase, &error);
	else {
	    skey = ssh2_load_userkey(filename, passphrase, &error);
	    if (skey == SSH2_WRONG_PASSPHRASE)
		ret = -1;
	    else if (!skey)
		ret = 0;
	    else
		ret = 1;
	}
	attempts++;
    } while (ret == -1);

    if(original_pass && ret) {
        /* If they typed in an ok passphrase, remember it */
	addpos234(passphrases, passphrase, 0);
	if (pps.save) {
	    save_passphrases(passphrase);
	}
    } else {
        /* Otherwise, destroy it */
        burnstr(passphrase);
    }
    passphrase = NULL;

    if (comment)
	sfree(comment);
    if (ret == 0) {
	char *msg = dupprintf("Couldn't load private key (%s)", error);
	message_box(msg, APPNAME, MB_OK | MB_ICONERROR,
		    HELPCTXID(errors_cantloadkey));
	sfree(msg);
	if (type == SSH_KEYTYPE_SSH1)
	    sfree(rkey);
	return;
    }
    if (type == SSH_KEYTYPE_SSH1) {
	if (already_running) {
	    unsigned char *request, *response;
	    void *vresponse;
	    int reqlen, clen, resplen, ret;

	    clen = strlen(rkey->comment);

	    reqlen = 4 + 1 +	       /* length, message type */
		4 +		       /* bit count */
		ssh1_bignum_length(rkey->modulus) +
		ssh1_bignum_length(rkey->exponent) +
		ssh1_bignum_length(rkey->private_exponent) +
		ssh1_bignum_length(rkey->iqmp) +
		ssh1_bignum_length(rkey->p) +
		ssh1_bignum_length(rkey->q) + 4 + clen	/* comment */
		;

	    request = snewn(reqlen, unsigned char);

	    request[4] = SSH1_AGENTC_ADD_RSA_IDENTITY;
	    reqlen = 5;
	    PUT_32BIT(request + reqlen, bignum_bitcount(rkey->modulus));
	    reqlen += 4;
	    reqlen += ssh1_write_bignum(request + reqlen, rkey->modulus);
	    reqlen += ssh1_write_bignum(request + reqlen, rkey->exponent);
	    reqlen +=
		ssh1_write_bignum(request + reqlen,
				  rkey->private_exponent);
	    reqlen += ssh1_write_bignum(request + reqlen, rkey->iqmp);
	    reqlen += ssh1_write_bignum(request + reqlen, rkey->p);
	    reqlen += ssh1_write_bignum(request + reqlen, rkey->q);
	    PUT_32BIT(request + reqlen, clen);
	    memcpy(request + reqlen + 4, rkey->comment, clen);
	    reqlen += 4 + clen;
	    PUT_32BIT(request, reqlen - 4);

	    ret = agent_query(request, reqlen, &vresponse, &resplen,
			      NULL, NULL);
	    assert(ret == 1);
	    response = vresponse;
	    if (resplen < 5 || response[4] != SSH_AGENT_SUCCESS)
		MessageBox(hwnd, "The already running Pageant "
			   "refused to add the key.", APPNAME,
			   MB_OK | MB_ICONERROR);

	    sfree(request);
	    sfree(response);
	} else {
	    if (add234(rsakeys, rkey) != rkey)
		sfree(rkey);	       /* already present, don't waste RAM */
	}
    } else {
	if (already_running) {
	    unsigned char *request, *response;
	    void *vresponse;
	    int reqlen, alglen, clen, keybloblen, resplen, ret;
	    alglen = strlen(skey->alg->name);
	    clen = strlen(skey->comment);

	    keybloblen = skey->alg->openssh_fmtkey(skey->data, NULL, 0);

	    reqlen = 4 + 1 +	       /* length, message type */
		4 + alglen +	       /* algorithm name */
		keybloblen +	       /* key data */
		4 + clen	       /* comment */
		;

	    request = snewn(reqlen, unsigned char);

	    request[4] = SSH2_AGENTC_ADD_IDENTITY;
	    reqlen = 5;
	    PUT_32BIT(request + reqlen, alglen);
	    reqlen += 4;
	    memcpy(request + reqlen, skey->alg->name, alglen);
	    reqlen += alglen;
	    reqlen += skey->alg->openssh_fmtkey(skey->data,
						request + reqlen,
						keybloblen);
	    PUT_32BIT(request + reqlen, clen);
	    memcpy(request + reqlen + 4, skey->comment, clen);
	    reqlen += clen + 4;
	    PUT_32BIT(request, reqlen - 4);

	    ret = agent_query(request, reqlen, &vresponse, &resplen,
			      NULL, NULL);
	    assert(ret == 1);
	    response = vresponse;
	    if (resplen < 5 || response[4] != SSH_AGENT_SUCCESS)
		MessageBox(hwnd, "The already running Pageant "
			   "refused to add the key.", APPNAME,
			   MB_OK | MB_ICONERROR);

	    sfree(request);
	    sfree(response);
	} else {
	    if (add234(ssh2keys, skey) != skey) {
		skey->alg->freekey(skey->data);
		sfree(skey);	       /* already present, don't waste RAM */
	    }
	}
    }
}

/*
 * Create an SSH-1 key list in a malloc'ed buffer; return its
 * length.
 */
static void *make_keylist1(int *length)
{
    int i, nkeys, len;
    struct RSAKey *key;
    unsigned char *blob, *p, *ret;
    int bloblen;

    /*
     * Count up the number and length of keys we hold.
     */
    len = 4;
    nkeys = 0;
    for (i = 0; NULL != (key = index234(rsakeys, i)); i++) {
	nkeys++;
	blob = rsa_public_blob(key, &bloblen);
	len += bloblen;
	sfree(blob);
	len += 4 + strlen(key->comment);
    }

    /* Allocate the buffer. */
    p = ret = snewn(len, unsigned char);
    if (length) *length = len;

    PUT_32BIT(p, nkeys);
    p += 4;
    for (i = 0; NULL != (key = index234(rsakeys, i)); i++) {
	blob = rsa_public_blob(key, &bloblen);
	memcpy(p, blob, bloblen);
	p += bloblen;
	sfree(blob);
	PUT_32BIT(p, strlen(key->comment));
	memcpy(p + 4, key->comment, strlen(key->comment));
	p += 4 + strlen(key->comment);
    }

    assert(p - ret == len);
    return ret;
}

/*
 * Create an SSH-2 key list in a malloc'ed buffer; return its
 * length.
 */
static void *make_keylist2(int *length)
{
    struct ssh2_userkey *key;
    int i, len, nkeys;
    unsigned char *blob, *p, *ret;
    int bloblen;

    /*
     * Count up the number and length of keys we hold.
     */
    len = 4;
    nkeys = 0;
    for (i = 0; NULL != (key = index234(ssh2keys, i)); i++) {
	nkeys++;
	len += 4;	       /* length field */
	blob = key->alg->public_blob(key->data, &bloblen);
	len += bloblen;
	sfree(blob);
	len += 4 + strlen(key->comment);
    }

    /* Allocate the buffer. */
    p = ret = snewn(len, unsigned char);
    if (length) *length = len;

    /*
     * Packet header is the obvious five bytes, plus four
     * bytes for the key count.
     */
    PUT_32BIT(p, nkeys);
    p += 4;
    for (i = 0; NULL != (key = index234(ssh2keys, i)); i++) {
	blob = key->alg->public_blob(key->data, &bloblen);
	PUT_32BIT(p, bloblen);
	p += 4;
	memcpy(p, blob, bloblen);
	p += bloblen;
	sfree(blob);
	PUT_32BIT(p, strlen(key->comment));
	memcpy(p + 4, key->comment, strlen(key->comment));
	p += 4 + strlen(key->comment);
    }

    assert(p - ret == len);
    return ret;
}

/*
 * Acquire a keylist1 from the primary Pageant; this means either
 * calling make_keylist1 (if that's us) or sending a message to the
 * primary Pageant (if it's not).
 */
static void *get_keylist1(int *length)
{
    void *ret;

    if (already_running) {
	unsigned char request[5], *response;
	void *vresponse;
	int resplen, retval;
	request[4] = SSH1_AGENTC_REQUEST_RSA_IDENTITIES;
	PUT_32BIT(request, 4);

	retval = agent_query(request, 5, &vresponse, &resplen, NULL, NULL);
	assert(retval == 1);
	response = vresponse;
	if (resplen < 5 || response[4] != SSH1_AGENT_RSA_IDENTITIES_ANSWER) {
            sfree(response);
	    return NULL;
        }

	ret = snewn(resplen-5, unsigned char);
	memcpy(ret, response+5, resplen-5);
	sfree(response);

	if (length)
	    *length = resplen-5;
    } else {
	ret = make_keylist1(length);
    }
    return ret;
}

/*
 * Acquire a keylist2 from the primary Pageant; this means either
 * calling make_keylist2 (if that's us) or sending a message to the
 * primary Pageant (if it's not).
 */
static void *get_keylist2(int *length)
{
    void *ret;

    if (already_running) {
	unsigned char request[5], *response;
	void *vresponse;
	int resplen, retval;

	request[4] = SSH2_AGENTC_REQUEST_IDENTITIES;
	PUT_32BIT(request, 4);

	retval = agent_query(request, 5, &vresponse, &resplen, NULL, NULL);
	assert(retval == 1);
	response = vresponse;
	if (resplen < 5 || response[4] != SSH2_AGENT_IDENTITIES_ANSWER) {
            sfree(response);
	    return NULL;
        }

	ret = snewn(resplen-5, unsigned char);
	memcpy(ret, response+5, resplen-5);
	sfree(response);

	if (length)
	    *length = resplen-5;
    } else {
	ret = make_keylist2(length);
    }
    return ret;
}

static int get_confirm_timeout() {
    int timeout = -1;
        if (get_use_inifile()) {
            timeout = GetPrivateProfileInt(APPNAME, "ConfirmTimeout", -1, inifile);
        } else {
            HKEY hkey;
            if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
                DWORD type;
                DWORD size = sizeof timeout;
                if (RegQueryValueEx(hkey, "ConfirmTimeout", 0, &type, (LPBYTE) &timeout, &size) != ERROR_SUCCESS || type != REG_DWORD)
                    timeout = -1;
		RegCloseKey(hkey);
	}
    }
    if (timeout < 0 || 3600 < timeout)
        timeout = 30;
    return timeout;
}

struct ConfirmAcceptAgentProcStruct {
    int type;
    const char* fingerprint;
    int dont_ask_again;
    DWORD tickCount;
    int timeout;
};

BOOL CALLBACK ConfirmAcceptAgentProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    BOOL l10nSetDlgItemText(HWND dialog, int id, LPCSTR text);

    switch (message) {
    case WM_INITDIALOG:
        {
            struct ConfirmAcceptAgentProcStruct* info = (struct ConfirmAcceptAgentProcStruct*) lParam;
            const char* s;
            SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR) lParam);
            switch (info->type) {
            case SSH1_AGENTC_RSA_CHALLENGE:
            case SSH2_AGENTC_SIGN_REQUEST:
                s = "Accept query of the following key?";
                break;
            case SSH1_AGENTC_ADD_RSA_IDENTITY:
            case SSH2_AGENTC_ADD_IDENTITY:
                s = "Accept addition of the following key?";
                break;
            case SSH1_AGENTC_REMOVE_RSA_IDENTITY:
            case SSH2_AGENTC_REMOVE_IDENTITY:
                s = "Accept deletion of the following key?";
                break;
            case SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
                s = "Accept deletion of all SSH1 identities?";
                break;
            case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
                s = "Accept deletion of all SSH2 identities?";
                break;
            default:
                EndDialog(dialog, IDNO);
                return FALSE;
            }
            l10nSetDlgItemText(dialog, 100, s);
            if (info->fingerprint != NULL)
                SetDlgItemText(dialog, 101, info->fingerprint);

            SendDlgItemMessage(dialog, 102, BM_SETCHECK, info->dont_ask_again >= 0 ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessage(dialog, WM_NEXTDLGCTL, (WPARAM) GetDlgItem(dialog, info->dont_ask_again > 0 ? IDYES : IDNO), TRUE);

            info->timeout = get_confirm_timeout();
            if (info->timeout != 0) {
                info->tickCount = GetTickCount();
                SetTimer(dialog, 1, 1000, NULL);
            }
        }
        return FALSE;

    case WM_TIMER:
        if (wParam == 1) {
            struct ConfirmAcceptAgentProcStruct* info = (struct ConfirmAcceptAgentProcStruct*) GetWindowLongPtr(dialog, DWLP_USER);
            if ((int) (GetTickCount() - info->tickCount) > info->timeout * 1000) {
                SendDlgItemMessage(dialog, 102, BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessage(dialog, WM_COMMAND, MAKEWPARAM(info->dont_ask_again > 0 ? IDYES : IDNO, BN_CLICKED), 0);
            }
            return TRUE;
        }
        break;

    case WM_COMMAND:
        switch (wParam) {
        case MAKEWPARAM(IDYES, BN_CLICKED):
        case MAKEWPARAM(IDNO, BN_CLICKED):
            {
                struct ConfirmAcceptAgentProcStruct* info = (struct ConfirmAcceptAgentProcStruct*) GetWindowLongPtr(dialog, DWLP_USER);
                info->dont_ask_again = SendDlgItemMessage(dialog, 102, BM_GETCHECK, 0, 0) == BST_CHECKED;
                EndDialog(dialog, LOWORD(wParam));
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static int accept_agent_request(int type, const void* key) {
    static const char VALUE_ACCEPT[] = "accept";
    static const char VALUE_REFUSE[] = "refuse";

    const char* keyname;
    char* fingerprint = NULL;
    const size_t fingerprint_length = 512;
    int accept = -1;

    switch (type) {
    case SSH1_AGENTC_RSA_CHALLENGE:
    case SSH2_AGENTC_SIGN_REQUEST:
        keyname = "QUERY:";
        break;
    case SSH1_AGENTC_ADD_RSA_IDENTITY:
        keyname = "ADD:";
        break;
    case SSH2_AGENTC_ADD_IDENTITY:
        keyname = "ADD:";
        break;
    case SSH1_AGENTC_REMOVE_RSA_IDENTITY:
        keyname = "REMOVE:";
        break;
    case SSH2_AGENTC_REMOVE_IDENTITY:
        keyname = "REMOVE:";
        break;
    case SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
        keyname = "SSH1_REMOVE_ALL";
        break;
    case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
        keyname = "SSH2_REMOVE_ALL";
        break;
    default:
        return 0;
    }

    if (type != SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES && type != SSH2_AGENTC_REMOVE_ALL_IDENTITIES) {
        if (key == NULL) {
            return 0;
        } else {
            size_t keyname_length = strlen(keyname);
            char* buffer = (char*) alloca(keyname_length + fingerprint_length);
            strcpy(buffer, keyname);
            fingerprint = buffer + keyname_length;
            if (type == SSH1_AGENTC_RSA_CHALLENGE
                || type == SSH1_AGENTC_ADD_RSA_IDENTITY
                || type == SSH1_AGENTC_REMOVE_RSA_IDENTITY) {
	        strcpy(fingerprint, "ssh1:");
	        rsa_fingerprint(fingerprint + 5, fingerprint_length - 5, (struct RSAKey*) key);
            } else {
                size_t fp_length;
                struct ssh2_userkey* skey = (struct ssh2_userkey*) key;
	        char* fp = skey->alg->fingerprint(skey->data);
	        strncpy(fingerprint, fp, fingerprint_length);
	        fp_length = strlen(fingerprint);
	        if (fp_length < fingerprint_length - 2) {
		    fingerprint[fp_length] = ' ';
		    strncpy(fingerprint + fp_length + 1, skey->comment,
			    fingerprint_length - fp_length - 1);
	        }
            }
            keyname = buffer;
        }
    }

    if (keyname != NULL) {
        char value[8];
        if (get_use_inifile()) {
            char null = '\0';
	    GetPrivateProfileString(APPNAME, keyname, &null, value, sizeof value, inifile);
        }else{
	    HKEY hkey;
	    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
	        DWORD type;
	        DWORD size = sizeof value;
	        if (RegQueryValueEx(hkey, keyname, 0, &type, (LPBYTE) value, &size) != ERROR_SUCCESS || type != REG_SZ)
		    value[0] = '\0';
	        RegCloseKey(hkey);
	    }
        }
        if (strcmp(value, VALUE_ACCEPT) == 0)
            accept = 1;
        else if (strcmp(value, VALUE_REFUSE) == 0)
            accept = 0;
        else
            accept = -1;
        if (!confirm_any_request && accept >= 0)
            return accept;
    }

    {
        struct ConfirmAcceptAgentProcStruct info = {
            type,
            fingerprint,
            accept,
        };
        HWND focus = GetForegroundWindow();
        DWORD focusThreadId = focus != NULL ? GetWindowThreadProcessId(focus, NULL) : 0;
        DWORD currentThreadId = GetCurrentThreadId();
        if (focusThreadId != 0 && focusThreadId != currentThreadId)
            AttachThreadInput(currentThreadId, focusThreadId, TRUE);
        SetForegroundWindow(hwnd);
        if (focusThreadId != 0 && focusThreadId != currentThreadId)
            AttachThreadInput(currentThreadId, focusThreadId, FALSE);
        accept = DialogBoxParam(hinst, MAKEINTRESOURCE(300), hwnd, ConfirmAcceptAgentProc, (LPARAM) &info) == IDYES ? 1 : 0;
        if (focus != NULL)
            SetForegroundWindow(focus);

        if (keyname != NULL) {
            if (get_use_inifile()) {
                WritePrivateProfileString(APPNAME, keyname, info.dont_ask_again ? accept ? VALUE_ACCEPT : VALUE_REFUSE : NULL, inifile);
            }else{
	        HKEY hkey;
	        if (info.dont_ask_again) {
	            if (RegCreateKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
		        RegSetValueEx(hkey, keyname, 0, REG_SZ, (LPBYTE) (accept ? VALUE_ACCEPT : VALUE_REFUSE), accept ? sizeof VALUE_ACCEPT : sizeof VALUE_REFUSE);
		        RegCloseKey(hkey);
	            }
	        }else{
	            if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
		        RegDeleteValue(hkey, keyname);
		        RegCloseKey(hkey);
	            }
	        }
            }
        }
        return accept;
    }
}

/*
 * This is the main agent function that answers messages.
 */
static void answer_msg(void *msg)
{
    unsigned char *p = msg;
    unsigned char *ret = msg;
    unsigned char *msgend;
    int type;

    /*
     * Get the message length.
     */
    msgend = p + 4 + GET_32BIT(p);

    /*
     * Get the message type.
     */
    if (msgend < p+5)
	goto failure;
    type = p[4];

    p += 5;
    switch (type) {
      case SSH1_AGENTC_REQUEST_RSA_IDENTITIES:
	/*
	 * Reply with SSH1_AGENT_RSA_IDENTITIES_ANSWER.
	 */
	{
	    int len;
	    void *keylist;

	    ret[4] = SSH1_AGENT_RSA_IDENTITIES_ANSWER;
	    keylist = make_keylist1(&len);
	    if (len + 5 > AGENT_MAX_MSGLEN) {
		sfree(keylist);
		goto failure;
	    }
	    PUT_32BIT(ret, len + 1);
	    memcpy(ret + 5, keylist, len);
	    sfree(keylist);
	}
	break;
      case SSH2_AGENTC_REQUEST_IDENTITIES:
	/*
	 * Reply with SSH2_AGENT_IDENTITIES_ANSWER.
	 */
	{
	    int len;
	    void *keylist;

	    ret[4] = SSH2_AGENT_IDENTITIES_ANSWER;
	    keylist = make_keylist2(&len);
	    if (len + 5 > AGENT_MAX_MSGLEN) {
		sfree(keylist);
		goto failure;
	    }
	    PUT_32BIT(ret, len + 1);
	    memcpy(ret + 5, keylist, len);
	    sfree(keylist);
	}
	break;
      case SSH1_AGENTC_RSA_CHALLENGE:
	/*
	 * Reply with either SSH1_AGENT_RSA_RESPONSE or
	 * SSH_AGENT_FAILURE, depending on whether we have that key
	 * or not.
	 */
	{
	    struct RSAKey reqkey, *key;
	    Bignum challenge, response;
	    unsigned char response_source[48], response_md5[16];
	    struct MD5Context md5c;
	    int i, len;

	    p += 4;
	    i = ssh1_read_bignum(p, msgend - p, &reqkey.exponent);
	    if (i < 0)
		goto failure;
	    p += i;
	    i = ssh1_read_bignum(p, msgend - p, &reqkey.modulus);
	    if (i < 0) {
                freebn(reqkey.exponent);
		goto failure;
            }
	    p += i;
	    i = ssh1_read_bignum(p, msgend - p, &challenge);
	    if (i < 0) {
                freebn(reqkey.exponent);
                freebn(reqkey.modulus);
		goto failure;
            }
	    p += i;
	    if (msgend < p+16) {
		freebn(reqkey.exponent);
		freebn(reqkey.modulus);
		freebn(challenge);
		goto failure;
	    }
	    memcpy(response_source + 32, p, 16);
	    p += 16;
	    if (msgend < p+4 ||
		GET_32BIT(p) != 1 ||
		(key = find234(rsakeys, &reqkey, NULL)) == NULL) {
		freebn(reqkey.exponent);
		freebn(reqkey.modulus);
		freebn(challenge);
		goto failure;
	    }
	    if (!accept_agent_request(type, key)) {
		freebn(reqkey.exponent);
		freebn(reqkey.modulus);
		freebn(challenge);
		goto failure;
	    }
	    response = rsadecrypt(challenge, key);
	    for (i = 0; i < 32; i++)
		response_source[i] = bignum_byte(response, 31 - i);

	    MD5Init(&md5c);
	    MD5Update(&md5c, response_source, 48);
	    MD5Final(response_md5, &md5c);
	    smemclr(response_source, 48);	/* burn the evidence */
	    freebn(response);	       /* and that evidence */
	    freebn(challenge);	       /* yes, and that evidence */
	    freebn(reqkey.exponent);   /* and free some memory ... */
	    freebn(reqkey.modulus);    /* ... while we're at it. */

	    /*
	     * Packet is the obvious five byte header, plus sixteen
	     * bytes of MD5.
	     */
	    len = 5 + 16;
	    PUT_32BIT(ret, len - 4);
	    ret[4] = SSH1_AGENT_RSA_RESPONSE;
	    memcpy(ret + 5, response_md5, 16);
	}
	break;
      case SSH2_AGENTC_SIGN_REQUEST:
	/*
	 * Reply with either SSH2_AGENT_SIGN_RESPONSE or
	 * SSH_AGENT_FAILURE, depending on whether we have that key
	 * or not.
	 */
	{
	    struct ssh2_userkey *key;
	    struct blob b;
	    unsigned char *data, *signature;
	    int datalen, siglen, len;

	    if (msgend < p+4)
		goto failure;
	    b.len = toint(GET_32BIT(p));
            if (b.len < 0 || b.len > msgend - (p+4))
                goto failure;
	    p += 4;
	    b.blob = p;
	    p += b.len;
	    if (msgend < p+4)
		goto failure;
	    datalen = toint(GET_32BIT(p));
	    p += 4;
	    if (datalen < 0 || datalen > msgend - p)
		goto failure;
	    data = p;
	    key = find234(ssh2keys, &b, cmpkeys_ssh2_asymm);
	    if (!key)
		goto failure;
	    if (!accept_agent_request(type, key))
		goto failure;
	    signature = key->alg->sign(key->data, data, datalen, &siglen);
	    len = 5 + 4 + siglen;
	    PUT_32BIT(ret, len - 4);
	    ret[4] = SSH2_AGENT_SIGN_RESPONSE;
	    PUT_32BIT(ret + 5, siglen);
	    memcpy(ret + 5 + 4, signature, siglen);
	    sfree(signature);
	}
	break;
      case SSH1_AGENTC_ADD_RSA_IDENTITY:
	/*
	 * Add to the list and return SSH_AGENT_SUCCESS, or
	 * SSH_AGENT_FAILURE if the key was malformed.
	 */
	{
	    struct RSAKey *key;
	    char *comment;
            int n, commentlen;

	    key = snew(struct RSAKey);
	    memset(key, 0, sizeof(struct RSAKey));

	    n = makekey(p, msgend - p, key, NULL, 1);
	    if (n < 0) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }
	    p += n;

	    n = makeprivate(p, msgend - p, key);
	    if (n < 0) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }
	    p += n;

	    n = ssh1_read_bignum(p, msgend - p, &key->iqmp);  /* p^-1 mod q */
	    if (n < 0) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }
	    p += n;

	    n = ssh1_read_bignum(p, msgend - p, &key->p);  /* p */
	    if (n < 0) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }
	    p += n;

	    n = ssh1_read_bignum(p, msgend - p, &key->q);  /* q */
	    if (n < 0) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }
	    p += n;

	    if (msgend < p+4) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }
            commentlen = toint(GET_32BIT(p));

	    if (commentlen < 0 || commentlen > msgend - p) {
		freersakey(key);
		sfree(key);
		goto failure;
	    }

	    comment = snewn(commentlen+1, char);
	    if (comment) {
		memcpy(comment, p + 4, commentlen);
                comment[commentlen] = '\0';
		key->comment = comment;
	    }
            if (!accept_agent_request(type, key)) {
		freersakey(key);
		sfree(key);
		sfree(comment);
	        goto failure;
	    }
	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_FAILURE;
	    if (add234(rsakeys, key) == key) {
		keylist_update();
		ret[4] = SSH_AGENT_SUCCESS;
	    } else {
		freersakey(key);
		sfree(key);
	    }
	}
	break;
      case SSH2_AGENTC_ADD_IDENTITY:
	/*
	 * Add to the list and return SSH_AGENT_SUCCESS, or
	 * SSH_AGENT_FAILURE if the key was malformed.
	 */
	{
	    struct ssh2_userkey *key;
	    char *comment, *alg;
	    int alglen, commlen;
	    int bloblen;


	    if (msgend < p+4)
		goto failure;
	    alglen = toint(GET_32BIT(p));
	    p += 4;
	    if (alglen < 0 || alglen > msgend - p)
		goto failure;
	    alg = p;
	    p += alglen;

	    key = snew(struct ssh2_userkey);
	    /* Add further algorithm names here. */
	    if (alglen == 7 && !memcmp(alg, "ssh-rsa", 7))
		key->alg = &ssh_rsa;
	    else if (alglen == 7 && !memcmp(alg, "ssh-dss", 7))
		key->alg = &ssh_dss;
	    else {
		sfree(key);
		goto failure;
	    }

	    bloblen = msgend - p;
	    key->data = key->alg->openssh_createkey(&p, &bloblen);
	    if (!key->data) {
		sfree(key);
		goto failure;
	    }

	    /*
	     * p has been advanced by openssh_createkey, but
	     * certainly not _beyond_ the end of the buffer.
	     */
	    assert(p <= msgend);

	    if (msgend < p+4) {
		key->alg->freekey(key->data);
		sfree(key);
		goto failure;
	    }
	    commlen = toint(GET_32BIT(p));
	    p += 4;

	    if (commlen < 0 || commlen > msgend - p) {
		key->alg->freekey(key->data);
		sfree(key);
		goto failure;
	    }
	    comment = snewn(commlen + 1, char);
	    if (comment) {
		memcpy(comment, p, commlen);
		comment[commlen] = '\0';
	    }
	    key->comment = comment;

	    if (!accept_agent_request(type, key)) {
		key->alg->freekey(key->data);
		sfree(key);
                sfree(comment);
	        goto failure;
	    }
	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_FAILURE;
	    if (add234(ssh2keys, key) == key) {
		keylist_update();
		ret[4] = SSH_AGENT_SUCCESS;
	    } else {
		key->alg->freekey(key->data);
		sfree(key->comment);
		sfree(key);
	    }
	}
	break;
      case SSH1_AGENTC_REMOVE_RSA_IDENTITY:
	/*
	 * Remove from the list and return SSH_AGENT_SUCCESS, or
	 * perhaps SSH_AGENT_FAILURE if it wasn't in the list to
	 * start with.
	 */
	{
	    struct RSAKey reqkey, *key;
	    int n;

	    n = makekey(p, msgend - p, &reqkey, NULL, 0);
	    if (n < 0)
		goto failure;

	    key = find234(rsakeys, &reqkey, NULL);
	    freebn(reqkey.exponent);
	    freebn(reqkey.modulus);
	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_FAILURE;
	    if (key) {
	        if (!accept_agent_request(type, key)) {
		    freersakey(key);
		    sfree(key);
		    goto failure;
                }
		del234(rsakeys, key);
		keylist_update();
		freersakey(key);
		sfree(key);
		ret[4] = SSH_AGENT_SUCCESS;
	    }
	}
	break;
      case SSH2_AGENTC_REMOVE_IDENTITY:
	/*
	 * Remove from the list and return SSH_AGENT_SUCCESS, or
	 * perhaps SSH_AGENT_FAILURE if it wasn't in the list to
	 * start with.
	 */
	{
	    struct ssh2_userkey *key;
	    struct blob b;

	    if (msgend < p+4)
		goto failure;
	    b.len = toint(GET_32BIT(p));
	    p += 4;

	    if (b.len < 0 || b.len > msgend - p)
		goto failure;
	    b.blob = p;
	    p += b.len;

	    key = find234(ssh2keys, &b, cmpkeys_ssh2_asymm);
	    if (!key)
		goto failure;

	    if (!accept_agent_request(type, key))
	        goto failure;

	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_FAILURE;
	    if (key) {
		del234(ssh2keys, key);
		keylist_update();
		key->alg->freekey(key->data);
		sfree(key);
		ret[4] = SSH_AGENT_SUCCESS;
	    }
	}
	break;
      case SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
	/*
	 * Remove all SSH-1 keys. Always returns success.
	 */
	if (!accept_agent_request(type, NULL)) {
	  goto failure;
	}
	{
	    struct RSAKey *rkey;

	    while ((rkey = index234(rsakeys, 0)) != NULL) {
		del234(rsakeys, rkey);
		freersakey(rkey);
		sfree(rkey);
	    }
	    keylist_update();

	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_SUCCESS;
	}
	break;
      case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
	/*
	 * Remove all SSH-2 keys. Always returns success.
	 */
	if (!accept_agent_request(type, NULL)) {
	  goto failure;
	}
	{
	    struct ssh2_userkey *skey;

	    while ((skey = index234(ssh2keys, 0)) != NULL) {
		del234(ssh2keys, skey);
		skey->alg->freekey(skey->data);
		sfree(skey);
	    }
	    keylist_update();

	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_SUCCESS;
	}
	break;
      default:
      failure:
	/*
	 * Unrecognised message. Return SSH_AGENT_FAILURE.
	 */
	PUT_32BIT(ret, 1);
	ret[4] = SSH_AGENT_FAILURE;
	break;
    }
}

/*
 * Key comparison function for the 2-3-4 tree of RSA keys.
 */
static int cmpkeys_rsa(void *av, void *bv)
{
    struct RSAKey *a = (struct RSAKey *) av;
    struct RSAKey *b = (struct RSAKey *) bv;
    Bignum am, bm;
    int alen, blen;

    am = a->modulus;
    bm = b->modulus;
    /*
     * Compare by length of moduli.
     */
    alen = bignum_bitcount(am);
    blen = bignum_bitcount(bm);
    if (alen > blen)
	return +1;
    else if (alen < blen)
	return -1;
    /*
     * Now compare by moduli themselves.
     */
    alen = (alen + 7) / 8;	       /* byte count */
    while (alen-- > 0) {
	int abyte, bbyte;
	abyte = bignum_byte(am, alen);
	bbyte = bignum_byte(bm, alen);
	if (abyte > bbyte)
	    return +1;
	else if (abyte < bbyte)
	    return -1;
    }
    /*
     * Give up.
     */
    return 0;
}

/*
 * Key comparison function for the 2-3-4 tree of SSH-2 keys.
 */
static int cmpkeys_ssh2(void *av, void *bv)
{
    struct ssh2_userkey *a = (struct ssh2_userkey *) av;
    struct ssh2_userkey *b = (struct ssh2_userkey *) bv;
    int i;
    int alen, blen;
    unsigned char *ablob, *bblob;
    int c;

    /*
     * Compare purely by public blob.
     */
    ablob = a->alg->public_blob(a->data, &alen);
    bblob = b->alg->public_blob(b->data, &blen);

    c = 0;
    for (i = 0; i < alen && i < blen; i++) {
	if (ablob[i] < bblob[i]) {
	    c = -1;
	    break;
	} else if (ablob[i] > bblob[i]) {
	    c = +1;
	    break;
	}
    }
    if (c == 0 && i < alen)
	c = +1;			       /* a is longer */
    if (c == 0 && i < blen)
	c = -1;			       /* a is longer */

    sfree(ablob);
    sfree(bblob);

    return c;
}

/*
 * Key comparison function for looking up a blob in the 2-3-4 tree
 * of SSH-2 keys.
 */
static int cmpkeys_ssh2_asymm(void *av, void *bv)
{
    struct blob *a = (struct blob *) av;
    struct ssh2_userkey *b = (struct ssh2_userkey *) bv;
    int i;
    int alen, blen;
    unsigned char *ablob, *bblob;
    int c;

    /*
     * Compare purely by public blob.
     */
    ablob = a->blob;
    alen = a->len;
    bblob = b->alg->public_blob(b->data, &blen);

    c = 0;
    for (i = 0; i < alen && i < blen; i++) {
	if (ablob[i] < bblob[i]) {
	    c = -1;
	    break;
	} else if (ablob[i] > bblob[i]) {
	    c = +1;
	    break;
	}
    }
    if (c == 0 && i < alen)
	c = +1;			       /* a is longer */
    if (c == 0 && i < blen)
	c = -1;			       /* a is longer */

    sfree(bblob);

    return c;
}

/*
 * Prompt for a key file to add, and add it.
 */
static void prompt_add_keyfile(HWND owner)
{
    OPENFILENAME of;
    char *filelist = snewn(8192, char);
	
    if (!keypath) keypath = filereq_new();
    memset(&of, 0, sizeof(of));
    of.hwndOwner = owner;
    of.lpstrFilter = FILTER_KEY_FILES;
    of.lpstrCustomFilter = NULL;
    of.nFilterIndex = 1;
    of.lpstrFile = filelist;
    *filelist = '\0';
    of.nMaxFile = 8192;
    of.lpstrFileTitle = NULL;
    of.lpstrTitle = "Select Private Key File";
    of.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (request_file(keypath, &of, TRUE, FALSE)) {
	load_passphrases();
	if(strlen(filelist) > of.nFileOffset) {
	    /* Only one filename returned? */
            Filename *fn = filename_from_str(filelist);
	    add_keyfile(fn);
            filename_free(fn);
        } else {
	    /* we are returned a bunch of strings, end to
	     * end. first string is the directory, the
	     * rest the filenames. terminated with an
	     * empty string.
	     */
	    char *dir = filelist;
	    char *filewalker = filelist + strlen(dir) + 1;
	    while (*filewalker != '\0') {
		char *filename = dupcat(dir, "\\", filewalker, NULL);
                Filename *fn = filename_from_str(filename);
		add_keyfile(fn);
                filename_free(fn);
		sfree(filename);
		filewalker += strlen(filewalker) + 1;
	    }
	}

	keylist_update();
	forget_passphrases();
    }
    sfree(filelist);
}

/*
 * Dialog-box function for the key list box.
 */
static int CALLBACK KeyListProc(HWND hwnd, UINT msg,
				WPARAM wParam, LPARAM lParam)
{
    struct RSAKey *rkey;
    struct ssh2_userkey *skey;

    switch (msg) {
      case WM_INITDIALOG:
	/*
	 * Centre the window.
	 */
	{			       /* centre the window */
	    RECT rs, rd;
	    HWND hw;

	    hw = GetDesktopWindow();
	    if (GetWindowRect(hw, &rs) && GetWindowRect(hwnd, &rd))
		MoveWindow(hwnd,
			   (rs.right + rs.left + rd.left - rd.right) / 2,
			   (rs.bottom + rs.top + rd.top - rd.bottom) / 2,
			   rd.right - rd.left, rd.bottom - rd.top, TRUE);
	}

        if (has_help())
            SetWindowLongPtr(hwnd, GWL_EXSTYLE,
			     GetWindowLongPtr(hwnd, GWL_EXSTYLE) |
			     WS_EX_CONTEXTHELP);
        else {
            HWND item = GetDlgItem(hwnd, 103);   /* the Help button */
            if (item)
                DestroyWindow(item);
        }

	keylist = hwnd;
	{
	    static int tabs[] = { 35, 60, 210 };
	    SendDlgItemMessage(hwnd, 100, LB_SETTABSTOPS,
			       sizeof(tabs) / sizeof(*tabs),
			       (LPARAM) tabs);
	}
	keylist_update();
	return 0;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	  case IDCANCEL:
	    keylist = NULL;
	    DestroyWindow(hwnd);
	    return 0;
	  case 101:		       /* add key */
	    if (HIWORD(wParam) == BN_CLICKED ||
		HIWORD(wParam) == BN_DOUBLECLICKED) {
		if (passphrase_box) {
		    MessageBeep(MB_ICONERROR);
		    SetForegroundWindow(passphrase_box);
		    break;
		}
		prompt_add_keyfile(hwnd);
	    }
	    return 0;
	  case 102:		       /* remove key */
	    if (HIWORD(wParam) == BN_CLICKED ||
		HIWORD(wParam) == BN_DOUBLECLICKED) {
		int i;
		int rCount, sCount;
		int *selectedArray;
		
		/* our counter within the array of selected items */
		int itemNum;
		
		/* get the number of items selected in the list */
		int numSelected = 
			SendDlgItemMessage(hwnd, 100, LB_GETSELCOUNT, 0, 0);
		
		/* none selected? that was silly */
		if (numSelected == 0) {
		    MessageBeep(0);
		    break;
		}

		/* get item indices in an array */
		selectedArray = snewn(numSelected, int);
		SendDlgItemMessage(hwnd, 100, LB_GETSELITEMS,
				numSelected, (WPARAM)selectedArray);
		
		itemNum = numSelected - 1;
		rCount = count234(rsakeys);
		sCount = count234(ssh2keys);
		
		/* go through the non-rsakeys until we've covered them all, 
		 * and/or we're out of selected items to check. note that
		 * we go *backwards*, to avoid complications from deleting
		 * things hence altering the offset of subsequent items
		 */
	    for (i = sCount - 1; (itemNum >= 0) && (i >= 0); i--) {
			skey = index234(ssh2keys, i);
			
			if (selectedArray[itemNum] == rCount + i) {
				del234(ssh2keys, skey);
				skey->alg->freekey(skey->data);
				sfree(skey);
			   	itemNum--; 
			}
		}
		
		/* do the same for the rsa keys */
		for (i = rCount - 1; (itemNum >= 0) && (i >= 0); i--) {
			rkey = index234(rsakeys, i);

			if(selectedArray[itemNum] == i) {
				del234(rsakeys, rkey);
				freersakey(rkey);
				sfree(rkey);
				itemNum--;
			}
		}

		sfree(selectedArray); 
		keylist_update();
	    }
	    return 0;
	  case 103:		       /* help */
            if (HIWORD(wParam) == BN_CLICKED ||
                HIWORD(wParam) == BN_DOUBLECLICKED) {
		launch_help(hwnd, WINHELP_CTX_pageant_general);
            }
	    return 0;
	}
	return 0;
      case WM_HELP:
        {
            int id = ((LPHELPINFO)lParam)->iCtrlId;
            char *topic = NULL;
            switch (id) {
              case 100: topic = WINHELP_CTX_pageant_keylist; break;
              case 101: topic = WINHELP_CTX_pageant_addkey; break;
              case 102: topic = WINHELP_CTX_pageant_remkey; break;
            }
            if (topic) {
		launch_help(hwnd, topic);
            } else {
                MessageBeep(0);
            }
        }
        break;
      case WM_CLOSE:
	keylist = NULL;
	DestroyWindow(hwnd);
	return 0;
    }
    return 0;
}

/* Set up a system tray icon */
static BOOL AddTrayIcon(HWND hwnd)
{
    BOOL res;
    NOTIFYICONDATA tnid;
    HICON hicon;

#ifdef NIM_SETVERSION
    tnid.uVersion = 0;
    res = Shell_NotifyIcon(NIM_SETVERSION, &tnid);
#endif

    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = hwnd;
    tnid.uID = 1;	       /* unique within this systray use */
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tnid.uCallbackMessage = WM_SYSTRAY;
    tnid.hIcon = hicon = LoadIcon(hinst, MAKEINTRESOURCE(201));
    strcpy(tnid.szTip, "Pageant (PuTTY authentication agent)");

    res = Shell_NotifyIcon(NIM_ADD, &tnid);
    if (!res)
	SetTimer(hwnd, TID_ADDSYSTRAY, 1000, NULL);

    if (hicon) DestroyIcon(hicon);
    
    return res;
}

/* Update the saved-sessions menu. */
static void update_sessions(void)
{
    int num_entries;
    HKEY hkey;
    TCHAR buf[MAX_PATH + 1];
    MENUITEMINFO mii;

    int index_key, index_menu;

    if (!putty_path)
	return;

    if (get_use_inifile()) {
        tree234* sessions;
        int i;
	char* buffer;
	int length = 256;
	char* p;
	while (1) {
	    buffer = snewn(length, char);
	    if (GetPrivateProfileSectionNames(buffer, length, inifile) < (DWORD) length - 2)
	        break;
            sfree(buffer);
	    length += 256;
	}
	p = buffer;

	for (num_entries = GetMenuItemCount(session_menu);
	    num_entries > initial_menuitems_count;
	    num_entries--)
	    RemoveMenu(session_menu, 0, MF_BYPOSITION);

	index_menu = 0;

        sessions = newtree234(strcmp);
	while(*p != '\0') {
	    if (!strncmp(p, HEADER, sizeof (HEADER) - 1)) {
		if(strcmp(buf, PUTTY_DEFAULT) != 0) {
		    TCHAR session_name[MAX_PATH + 1];
		    unmungestr(p + sizeof (HEADER) - 1, session_name, MAX_PATH);
                    add234(sessions, strdup(session_name));
                }
            }
            p += strlen(p) + 1;
        }
        for (i = 0; (p = index234(sessions, i)) != NULL; i++) {
	    memset(&mii, 0, sizeof(mii));
	    mii.cbSize = sizeof(mii);
	    mii.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
	    mii.fType = MFT_STRING;
	    mii.fState = MFS_ENABLED;
	    mii.wID = (index_menu * 16) + IDM_SESSIONS_BASE;
	    mii.dwTypeData = p;
	    InsertMenuItem(session_menu, index_menu, TRUE, &mii);
	    index_menu++;
            free(p);
	}
        freetree234(sessions);

	sfree(buffer);

	if(index_menu == 0) {
            mii.cbSize = sizeof(mii);
            mii.fMask = MIIM_TYPE | MIIM_STATE;
            mii.fType = MFT_STRING;
            mii.fState = MFS_GRAYED;
            mii.dwTypeData = _T("(No sessions)");
            InsertMenuItem(session_menu, index_menu, TRUE, &mii);
	}
	return;
    }

    if(ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, PUTTY_REGKEY, &hkey))
	return;

    for(num_entries = GetMenuItemCount(session_menu);
	num_entries > initial_menuitems_count;
	num_entries--)
	RemoveMenu(session_menu, 0, MF_BYPOSITION);

    index_key = 0;
    index_menu = 0;

    while(ERROR_SUCCESS == RegEnumKey(hkey, index_key, buf, MAX_PATH)) {
	TCHAR session_name[MAX_PATH + 1];
	unmungestr(buf, session_name, MAX_PATH);
	if(strcmp(buf, PUTTY_DEFAULT) != 0) {
	    memset(&mii, 0, sizeof(mii));
	    mii.cbSize = sizeof(mii);
	    mii.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
	    mii.fType = MFT_STRING;
	    mii.fState = MFS_ENABLED;
	    mii.wID = (index_menu * 16) + IDM_SESSIONS_BASE;
	    mii.dwTypeData = session_name;
	    InsertMenuItem(session_menu, index_menu, TRUE, &mii);
	    index_menu++;
	}
	index_key++;
    }

    RegCloseKey(hkey);

    if(index_menu == 0) {
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_STATE;
	mii.fType = MFT_STRING;
	mii.fState = MFS_GRAYED;
	mii.dwTypeData = _T("(No sessions)");
	InsertMenuItem(session_menu, index_menu, TRUE, &mii);
    }
}

#ifndef NO_SECURITY
/*
 * Versions of Pageant prior to 0.61 expected this SID on incoming
 * communications. For backwards compatibility, and more particularly
 * for compatibility with derived works of PuTTY still using the old
 * Pageant client code, we accept it as an alternative to the one
 * returned from get_user_sid() in winpgntc.c.
 */
PSID get_default_sid(void)
{
    HANDLE proc = NULL;
    DWORD sidlen;
    PSECURITY_DESCRIPTOR psd = NULL;
    PSID sid = NULL, copy = NULL, ret = NULL;

    if ((proc = OpenProcess(MAXIMUM_ALLOWED, FALSE,
                            GetCurrentProcessId())) == NULL)
        goto cleanup;

    if (p_GetSecurityInfo(proc, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION,
                          &sid, NULL, NULL, NULL, &psd) != ERROR_SUCCESS)
        goto cleanup;

    sidlen = GetLengthSid(sid);

    copy = (PSID)smalloc(sidlen);

    if (!CopySid(sidlen, copy, sid))
        goto cleanup;

    /* Success. Move sid into the return value slot, and null it out
     * to stop the cleanup code freeing it. */
    ret = copy;
    copy = NULL;

  cleanup:
    if (proc != NULL)
        CloseHandle(proc);
    if (psd != NULL)
        LocalFree(psd);
    if (copy != NULL)
        sfree(copy);

    return ret;
}
#endif

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
				WPARAM wParam, LPARAM lParam)
{
    int ret;
    static int menuinprogress;
    static UINT msgTaskbarCreated = 0;

    switch (message) {
      case WM_CREATE:
        msgTaskbarCreated = RegisterWindowMessage(_T("TaskbarCreated"));
        break;
      default:
        if (message==msgTaskbarCreated) {
            /*
	     * Explorer has been restarted, so the tray icon will
	     * have been lost.
	     */
	    AddTrayIcon(hwnd);
        }
        break;
        
      case WM_SYSTRAY:
	if (lParam == WM_RBUTTONUP) {
	    POINT cursorpos;
	    GetCursorPos(&cursorpos);
	    PostMessage(hwnd, WM_SYSTRAY2, cursorpos.x, cursorpos.y);
	} else if (lParam == WM_LBUTTONDBLCLK) {
	    /* Run the default menu item. */
	    UINT menuitem = GetMenuDefaultItem(systray_menu, FALSE, 0);
	    if (menuitem != -1)
		PostMessage(hwnd, WM_COMMAND, menuitem, 0);
	}
	break;
      case WM_SYSTRAY2:
	if (!menuinprogress) {
	    menuinprogress = 1;
	    update_sessions();
	    SetForegroundWindow(hwnd);
	    ret = TrackPopupMenu(systray_menu,
				 TPM_RIGHTALIGN | TPM_BOTTOMALIGN |
				 TPM_RIGHTBUTTON,
				 wParam, lParam, 0, hwnd, NULL);
	    menuinprogress = 0;
	}
	break;
      case WM_TIMER:
        if (wParam == TID_ADDSYSTRAY) {
            KillTimer(hwnd, TID_ADDSYSTRAY);
	    AddTrayIcon(hwnd);
	}
	break;
      case WM_COMMAND:
      case WM_SYSCOMMAND:
	switch (wParam & ~0xF) {       /* low 4 bits reserved to Windows */
	  case IDM_PUTTY:
	    if((int)ShellExecute(hwnd, NULL, putty_path, _T(""), _T(""),
				 SW_SHOW) <= 32) {
		MessageBox(hwnd, "Unable to execute PuTTY!",
			   "Error", MB_OK | MB_ICONERROR);
	    }
	    break;
	  case IDM_CLOSE:
	    if (passphrase_box)
		SendMessage(passphrase_box, WM_CLOSE, 0, 0);
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
	    break;
	  case IDM_VIEWKEYS:
	    if (!keylist) {
		keylist = CreateDialog(hinst, MAKEINTRESOURCE(211),
				       hwnd, KeyListProc);
		ShowWindow(keylist, SW_SHOWNORMAL);
	    }
	    /* 
	     * Sometimes the window comes up minimised / hidden for
	     * no obvious reason. Prevent this. This also brings it
	     * to the front if it's already present (the user
	     * selected View Keys because they wanted to _see_ the
	     * thing).
	     */
	    SetForegroundWindow(keylist);
	    SetWindowPos(keylist, HWND_TOP, 0, 0, 0, 0,
			 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	    break;
	  case IDM_ADDKEY:
	    if (passphrase_box) {
		MessageBeep(MB_ICONERROR);
		SetForegroundWindow(passphrase_box);
		break;
	    }
	    prompt_add_keyfile(hwnd);
	    break;
	  case IDM_ABOUT:
	    if (!aboutbox) {
		aboutbox = CreateDialog(hinst, MAKEINTRESOURCE(213),
					hwnd, AboutProc);
		ShowWindow(aboutbox, SW_SHOWNORMAL);
		/* 
		 * Sometimes the window comes up minimised / hidden
		 * for no obvious reason. Prevent this.
		 */
		SetForegroundWindow(aboutbox);
		SetWindowPos(aboutbox, HWND_TOP, 0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	    }
	    break;
	  case IDM_HELP:
	    launch_help(hwnd, WINHELP_CTX_pageant_general);
	    break;
	  case IDM_CONFIRM_REQUEST:
	    {
                MENUITEMINFO mii = {sizeof mii, MIIM_STATE};
                confirm_any_request = !confirm_any_request;
                mii.fState = confirm_any_request ? MFS_CHECKED : MFS_UNCHECKED;
                SetMenuItemInfo(systray_menu, IDM_CONFIRM_REQUEST, FALSE, &mii);
	    }
	    break;
	  default:
	    {
		if(wParam >= IDM_SESSIONS_BASE && wParam <= IDM_SESSIONS_MAX) {
		    MENUITEMINFO mii;
		    TCHAR buf[MAX_PATH + 1];
		    TCHAR param[MAX_PATH + 1];
		    memset(&mii, 0, sizeof(mii));
		    mii.cbSize = sizeof(mii);
		    mii.fMask = MIIM_TYPE;
		    mii.cch = MAX_PATH;
		    mii.dwTypeData = buf;
		    GetMenuItemInfo(session_menu, wParam, FALSE, &mii);
		    strcpy(param, "@");
		    strcat(param, mii.dwTypeData);
		    if((int)ShellExecute(hwnd, NULL, putty_path, param,
					 _T(""), SW_SHOW) <= 32) {
			MessageBox(hwnd, "Unable to execute PuTTY!", "Error",
				   MB_OK | MB_ICONERROR);
		    }
		}
	    }
	    break;
	}
	break;
      case WM_DESTROY:
	quit_help(hwnd);
	PostQuitMessage(0);
	return 0;
      case WM_COPYDATA:
	{
	    COPYDATASTRUCT *cds;
	    char *mapname;
	    void *p;
	    HANDLE filemap;
#ifndef NO_SECURITY
	    PSID mapowner, ourself, ourself2;
#endif
            PSECURITY_DESCRIPTOR psd = NULL;
	    int ret = 0;

	    cds = (COPYDATASTRUCT *) lParam;
	    if (cds->dwData != AGENT_COPYDATA_ID)
		return 0;	       /* not our message, mate */
	    mapname = (char *) cds->lpData;
	    if (mapname[cds->cbData - 1] != '\0')
		return 0;	       /* failure to be ASCIZ! */
#ifdef DEBUG_IPC
	    debug(("mapname is :%s:\n", mapname));
#endif
	    filemap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, mapname);
#ifdef DEBUG_IPC
	    debug(("filemap is %p\n", filemap));
#endif
	    if (filemap != NULL && filemap != INVALID_HANDLE_VALUE) {
#ifndef NO_SECURITY
		int rc;
		if (has_security) {
                    if ((ourself = get_user_sid()) == NULL) {
#ifdef DEBUG_IPC
			debug(("couldn't get user SID\n"));
#endif
                        CloseHandle(filemap);
			return 0;
                    }

                    if ((ourself2 = get_default_sid()) == NULL) {
#ifdef DEBUG_IPC
			debug(("couldn't get default SID\n"));
#endif
                        CloseHandle(filemap);
                        sfree(ourself);
			return 0;
                    }

		    if ((rc = p_GetSecurityInfo(filemap, SE_KERNEL_OBJECT,
						OWNER_SECURITY_INFORMATION,
						&mapowner, NULL, NULL, NULL,
						&psd) != ERROR_SUCCESS)) {
#ifdef DEBUG_IPC
			debug(("couldn't get owner info for filemap: %d\n",
                               rc));
#endif
                        CloseHandle(filemap);
                        sfree(ourself);
                        sfree(ourself2);
			return 0;
		    }
#ifdef DEBUG_IPC
                    {
                        LPTSTR ours, ours2, theirs;
                        ConvertSidToStringSid(mapowner, &theirs);
                        ConvertSidToStringSid(ourself, &ours);
                        ConvertSidToStringSid(ourself2, &ours2);
                        debug(("got sids:\n  oursnew=%s\n  oursold=%s\n"
                               "  theirs=%s\n", ours, ours2, theirs));
                        LocalFree(ours);
                        LocalFree(ours2);
                        LocalFree(theirs);
                    }
#endif
		    if (!EqualSid(mapowner, ourself) &&
                        !EqualSid(mapowner, ourself2)) {
                        CloseHandle(filemap);
                        LocalFree(psd);
                        sfree(ourself);
                        sfree(ourself2);
			return 0;      /* security ID mismatch! */
                    }
#ifdef DEBUG_IPC
		    debug(("security stuff matched\n"));
#endif
                    LocalFree(psd);
                    sfree(ourself);
                    sfree(ourself2);
		} else {
#ifdef DEBUG_IPC
		    debug(("security APIs not present\n"));
#endif
		}
#endif
		p = MapViewOfFile(filemap, FILE_MAP_WRITE, 0, 0, 0);
#ifdef DEBUG_IPC
		debug(("p is %p\n", p));
		{
		    int i;
		    for (i = 0; i < 5; i++)
			debug(("p[%d]=%02x\n", i,
			       ((unsigned char *) p)[i]));
                }
#endif
		answer_msg(p);
		ret = 1;
		UnmapViewOfFile(p);
	    }
	    CloseHandle(filemap);
	    return ret;
	}
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*
 * Fork and Exec the command in cmdline. [DBW]
 */
void spawn_cmd(char *cmdline, char * args, int show)
{
    if (ShellExecute(NULL, _T("open"), cmdline,
		     args, NULL, show) <= (HINSTANCE) 32) {
	char *msg;
	msg = dupprintf("Failed to run \"%.100s\", Error: %d", cmdline,
			(int)GetLastError());
	MessageBox(hwnd, msg, APPNAME, MB_OK | MB_ICONEXCLAMATION);
	sfree(msg);
    }
}

/*
 * This is a can't-happen stub, since Pageant never makes
 * asynchronous agent requests.
 */
void agent_schedule_callback(void (*callback)(void *, void *, int),
			     void *callback_ctx, void *data, int len)
{
    assert(!"We shouldn't get here");
}

void cleanup_exit(int code)
{
    shutdown_help();
    exit(code);
}

int flags = FLAG_SYNCAGENT;

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    BOOL l10nAppendMenu(HMENU menu, UINT flags, UINT id, LPCSTR text);
    WNDCLASS wndclass;
    MSG msg;
    char *command = NULL;
    int added_keys = 0;
    int argc, i;
    char **argv, **argstart;

    hinst = inst;
    hwnd = NULL;

    /*
     * Determine whether we're an NT system (should have security
     * APIs) or a non-NT system (don't do security).
     */
    if (!init_winver())
    {
	modalfatalbox("Windows refuses to report a version");
    }
    if (osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) {
	has_security = TRUE;
    } else
	has_security = FALSE;

    if (has_security) {
#ifndef NO_SECURITY
	/*
	 * Attempt to get the security API we need.
	 */
        if (!init_advapi()) {
	    MessageBox(NULL,
		       "Unable to access security APIs. Pageant will\n"
		       "not run, in case it causes a security breach.",
		       "Pageant Fatal Error", MB_ICONERROR | MB_OK);
	    return 1;
	}
#else
	MessageBox(hwnd,
		   "This program has been compiled for Win9X and will\n"
		   "not run on NT, in case it causes a security breach.",
		   "Pageant Fatal Error", MB_ICONERROR | MB_OK);
	return 1;
#endif
    }

    /*
     * See if we can find our Help file.
     */
    init_help();

    /*
     * Look for the PuTTY binary (we will enable the saved session
     * submenu if we find it).
     */
    {
        char b[2048], *p, *q, *r;
        FILE *fp;
        GetModuleFileName(NULL, b, sizeof(b) - 16);
        r = b;
        p = strrchr(b, '\\');
        if (p && p >= r) r = p+1;
        q = strrchr(b, ':');
        if (q && q >= r) r = q+1;
        strcpy(r, "putty.exe");
        if ( (fp = fopen(b, "r")) != NULL) {
            putty_path = dupstr(b);
            fclose(fp);
        } else
            putty_path = NULL;
    }

    /*
     * Find out if Pageant is already running.
     */
    already_running = agent_exists();

    /*
     * Initialise storage for RSA keys.
     */
    if (!already_running) {
	rsakeys = newtree234(cmpkeys_rsa);
	ssh2keys = newtree234(cmpkeys_ssh2);
    }

    /*
     * Initialise storage for short-term passphrase cache.
     */
    passphrases = newtree234(NULL);

    /*
     * Process the command line and add keys as listed on it.
     */
    split_into_argv(cmdline, &argc, &argv, &argstart);
    if (argc > 1 && !strcmp(argv[0], "-ini") && *(argv[1]) != '\0') {
        char* dummy;
        DWORD attributes;
        GetFullPathName(argv[1], sizeof inifile, inifile, &dummy);
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
    }

    load_passphrases();

    for (i = 0; i < argc; i++) {
	if (!strcmp(argv[i], "-pgpfp")) {
	    pgp_fingerprints();
	    return 1;
	} else if (!strcmp(argv[i], "-c")) {
	    /*
	     * If we see `-c', then the rest of the
	     * command line should be treated as a
	     * command to be spawned.
	     */
	    if (i < argc-1)
		command = argstart[i+1];
	    else
		command = "";
	    break;
	} else {
            Filename *fn = filename_from_str(argv[i]);
	    add_keyfile(fn);
            filename_free(fn);
	    added_keys = TRUE;
	}
    }

    /*
     * Forget any passphrase that we retained while going over
     * command line keyfiles.
     */
    forget_passphrases();

    if (command) {
	char *args;
	if (command[0] == '"')
	    args = strchr(++command, '"');
	else
	    args = strchr(command, ' ');
	if (args) {
	    *args++ = 0;
	    while(*args && isspace(*args)) args++;
	}
	spawn_cmd(command, args, show);
    }

    /*
     * If Pageant was already running, we leave now. If we haven't
     * even taken any auxiliary action (spawned a command or added
     * keys), complain.
     */
    if (already_running) {
	if (!command && !added_keys) {
	    MessageBox(hwnd, "Pageant is already running", "Pageant Error",
		       MB_ICONERROR | MB_OK);
	}
	return 0;
    }

    if (!prev) {
	wndclass.style = 0;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = inst;
	wndclass.hIcon = LoadIcon(inst, MAKEINTRESOURCE(IDI_MAINICON));
	wndclass.hCursor = LoadCursor(NULL, IDC_IBEAM);
	wndclass.hbrBackground = GetStockObject(BLACK_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = APPNAME;

	RegisterClass(&wndclass);
    }

    keylist = NULL;

    hwnd = CreateWindow(APPNAME, APPNAME,
			WS_OVERLAPPEDWINDOW | WS_VSCROLL,
			CW_USEDEFAULT, CW_USEDEFAULT,
			100, 100, NULL, NULL, inst, NULL);

    /* Set up a system tray icon */
    AddTrayIcon(hwnd);

    /* Accelerators used: nsvkxa */
    systray_menu = CreatePopupMenu();
    if (putty_path) {
	session_menu = CreateMenu();
	l10nAppendMenu(systray_menu, MF_ENABLED, IDM_PUTTY, "&New Session");
	l10nAppendMenu(systray_menu, MF_POPUP | MF_ENABLED,
		   (UINT) session_menu, "&Saved Sessions");
	AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    }
    l10nAppendMenu(systray_menu, MF_ENABLED, IDM_VIEWKEYS,
	   "&View Keys");
    l10nAppendMenu(systray_menu, MF_ENABLED, IDM_ADDKEY, "Add &Key");
    confirm_any_request = 0;
    l10nAppendMenu(systray_menu, MF_UNCHECKED, IDM_CONFIRM_REQUEST, "&Confirm Any Request");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    if (has_help())
	l10nAppendMenu(systray_menu, MF_ENABLED, IDM_HELP, "&Help");
    l10nAppendMenu(systray_menu, MF_ENABLED, IDM_ABOUT, "&About");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    l10nAppendMenu(systray_menu, MF_ENABLED, IDM_CLOSE, "E&xit");
    initial_menuitems_count = GetMenuItemCount(session_menu);

    /* Set the default menu item. */
    SetMenuDefaultItem(systray_menu, IDM_VIEWKEYS, FALSE);

    ShowWindow(hwnd, SW_HIDE);

    /*
     * Main message loop.
     */
    while (GetMessage(&msg, NULL, 0, 0) == 1) {
	if (!(IsWindow(keylist) && IsDialogMessage(keylist, &msg)) &&
	    !(IsWindow(aboutbox) && IsDialogMessage(aboutbox, &msg))) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
    }

    /* Clean up the system tray icon */
    {
	NOTIFYICONDATA tnid;

	tnid.cbSize = sizeof(NOTIFYICONDATA);
	tnid.hWnd = hwnd;
	tnid.uID = 1;

	Shell_NotifyIcon(NIM_DELETE, &tnid);

	DestroyMenu(systray_menu);
    }

    if (keypath) filereq_free(keypath);

    cleanup_exit(msg.wParam);
    return msg.wParam;		       /* just in case optimiser complains */
}
