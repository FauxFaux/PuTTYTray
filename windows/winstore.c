/*
 * HACK: PuttyTray / PuTTY File
 *
 * winstore.c: Windows-specific implementation of the interface
 * defined in storage.h.
 *
 * Added file support for PuTTY Tray
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "putty.h"
#include "storage.h"

#include <shlobj.h>
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA 0x001c
#endif

static const char *const reg_jumplist_key = PUTTY_REG_POS "\\Jumplist";
static const char *const reg_jumplist_value = "Recent sessions";
static const char *const puttystr = PUTTY_REG_POS "\\Sessions";

static const char hex[16] = "0123456789ABCDEF";

static int tried_shgetfolderpath = FALSE;
static HMODULE shell32_module = NULL;
DECL_WINDOWS_FUNCTION(static, HRESULT, SHGetFolderPathA, 
		      (HWND, int, HANDLE, DWORD, LPSTR));

// PUTTY Tray / PuTTY File - global storage type
static enum storage_t storagetype = STORAGE_REG;

static char seedpath[2 * MAX_PATH + 10] = "\0";
static char sesspath[2 * MAX_PATH] = "\0";
static char sshkpath[2 * MAX_PATH] = "\0";
static char oldpath[2 * MAX_PATH] = "\0";
static char sessionsuffix[16] = "\0";
static char keysuffix[16] = "\0";

struct setItem {
    char* key;
    char* value;
    struct setItem* next;
};

struct setPack {
    unsigned int fromFile;
    void* handle;
    char* fileBuf;
};

enum storage_t get_storagetype(void)
{
    return storagetype;
}

void set_storagetype(enum storage_t new_storagetype)
{
    storagetype = new_storagetype;
}

static void mungestr(const char *in, char *out)
{
    int candot = 0;

    while (*in) {
	if (*in == ' ' || *in == '\\' || *in == '*' || *in == '?' ||
	    *in == '%' || *in < ' ' || *in > '~' || (*in == '.'
						     && !candot)) {
	    *out++ = '%';
	    *out++ = hex[((unsigned char) *in) >> 4];
	    *out++ = hex[((unsigned char) *in) & 15];
	} else
	    *out++ = *in;
	in++;
	candot = 1;
    }
    *out = '\0';
    return;
}

static void unmungestr(const char *in, char *out, int outlen)
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

void *reg_open_settings_w(const char *sessionname, char **errmsg)
{
    HKEY subkey1, sesskey;
    int ret;
    char *p;

    *errmsg = NULL;

    if (!sessionname || !*sessionname)
	sessionname = "Default Settings";

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);

    ret = RegCreateKey(HKEY_CURRENT_USER, puttystr, &subkey1);
    if (ret != ERROR_SUCCESS) {
	sfree(p);
        *errmsg = dupprintf("Unable to create registry key\n"
                            "HKEY_CURRENT_USER\\%s", puttystr);
	return NULL;
    }
    ret = RegCreateKey(subkey1, p, &sesskey);
    RegCloseKey(subkey1);
    if (ret != ERROR_SUCCESS) {
        *errmsg = dupprintf("Unable to create registry key\n"
                            "HKEY_CURRENT_USER\\%s\\%s", puttystr, p);
	sfree(p);
	return NULL;
    }
    sfree(p);
    return (void *) sesskey;
}

void reg_write_setting_s(void *handle, const char *key, const char *value)
{
    if (handle)
	RegSetValueEx((HKEY) handle, key, 0, REG_SZ, value,
		      1 + strlen(value));
}

void reg_write_setting_i(void *handle, const char *key, int value)
{
    if (handle)
	RegSetValueEx((HKEY) handle, key, 0, REG_DWORD,
		      (CONST BYTE *) &value, sizeof(value));
}

void reg_close_settings_w(void *handle)
{
    RegCloseKey((HKEY) handle);
}

void *reg_open_settings_r(const char *sessionname)
{
    HKEY subkey1, sesskey;
    char *p;

    if (!sessionname || !*sessionname)
	sessionname = "Default Settings";

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &subkey1) != ERROR_SUCCESS) {
	sesskey = NULL;
    } else {
	if (RegOpenKey(subkey1, p, &sesskey) != ERROR_SUCCESS) {
	    sesskey = NULL;
	}
	RegCloseKey(subkey1);
    }

    sfree(p);

    return (void *) sesskey;
}

char *reg_read_setting_s(void *handle, const char *key)
{
    DWORD type, size;
    char *ret;

    if (!handle)
	return NULL;

    /* Find out the type and size of the data. */
    if (RegQueryValueEx((HKEY) handle, key, 0,
			&type, NULL, &size) != ERROR_SUCCESS ||
	type != REG_SZ)
	return NULL;

    ret = snewn(size+1, char);
    if (RegQueryValueEx((HKEY) handle, key, 0,
			&type, ret, &size) != ERROR_SUCCESS ||
	type != REG_SZ) {
        sfree(ret);
        return NULL;
    }

    return ret;
}

int reg_read_setting_i(void *handle, const char *key, int defvalue)
{
    DWORD type, val, size;
    size = sizeof(val);

    if (!handle ||
	RegQueryValueEx((HKEY) handle, key, 0, &type,
			(BYTE *) &val, &size) != ERROR_SUCCESS ||
	size != sizeof(val) || type != REG_DWORD)
	return defvalue;
    else
	return val;
}

FontSpec *read_setting_fontspec(void *handle, const char *name)
{
    char *settingname;
    char *fontname;
    FontSpec *ret;
    int isbold, height, charset;

    fontname = read_setting_s(handle, name);
    if (!fontname)
	return NULL;

    settingname = dupcat(name, "IsBold", NULL);
    isbold = read_setting_i(handle, settingname, -1);
    sfree(settingname);
    if (isbold == -1) {
        sfree(fontname);
        return NULL;
    }

    settingname = dupcat(name, "CharSet", NULL);
    charset = read_setting_i(handle, settingname, -1);
    sfree(settingname);
    if (charset == -1) {
        sfree(fontname);
        return NULL;
    }

    settingname = dupcat(name, "Height", NULL);
    height = read_setting_i(handle, settingname, INT_MIN);
    sfree(settingname);
    if (height == INT_MIN) {
        sfree(fontname);
        return NULL;
    }

    ret = fontspec_new(fontname, isbold, height, charset);
    sfree(fontname);
    return ret;
}

void write_setting_fontspec(void *handle, const char *name, FontSpec *font)
{
    char *settingname;

    write_setting_s(handle, name, font->name);
    settingname = dupcat(name, "IsBold", NULL);
    write_setting_i(handle, settingname, font->isbold);
    sfree(settingname);
    settingname = dupcat(name, "CharSet", NULL);
    write_setting_i(handle, settingname, font->charset);
    sfree(settingname);
    settingname = dupcat(name, "Height", NULL);
    write_setting_i(handle, settingname, font->height);
    sfree(settingname);
}

Filename *read_setting_filename(void *handle, const char *name)
{
    char *tmp = read_setting_s(handle, name);
    if (tmp) {
        Filename *ret = filename_from_str(tmp);
	sfree(tmp);
	return ret;
    } else
	return NULL;
}

void write_setting_filename(void *handle, const char *name, Filename *result)
{
    write_setting_s(handle, name, result->path);
}

void reg_close_settings_r(void *handle)
{
    RegCloseKey((HKEY) handle);
}

void reg_del_settings(const char *sessionname)
{
    HKEY subkey1;
    char *p;

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &subkey1) != ERROR_SUCCESS)
	return;

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);
    RegDeleteKey(subkey1, p);
    sfree(p);

    RegCloseKey(subkey1);

    remove_session_from_jumplist(sessionname);
}

struct enumsettings {
    HKEY key;
    int i;
    int fromFile;
    HANDLE hFile;
};

void *reg_enum_settings_start(void)
{
    struct enumsettings *ret;
    HKEY key;

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &key) != ERROR_SUCCESS)
	return NULL;

    ret = snew(struct enumsettings);
    if (ret) {
	ret->key = key;
	ret->i = 0;
    }

    return ret;
}

char *reg_enum_settings_next(void *handle, char *buffer, int buflen)
{
    struct enumsettings *e = (struct enumsettings *) handle;
    char *otherbuf;
    otherbuf = snewn(3 * buflen, char);
    if (RegEnumKey(e->key, e->i++, otherbuf, 3 * buflen) == ERROR_SUCCESS) {
	unmungestr(otherbuf, buffer, buflen);
	sfree(otherbuf);
	return buffer;
    } else {
	sfree(otherbuf);
	return NULL;
    }
}

void reg_enum_settings_finish(void *handle)
{
    struct enumsettings *e = (struct enumsettings *) handle;
    RegCloseKey(e->key);
    sfree(e);
}

static void hostkey_regname(char *buffer, const char *hostname,
			    int port, const char *keytype)
{
    int len;
    strcpy(buffer, keytype);
    strcat(buffer, "@");
    len = strlen(buffer);
    len += sprintf(buffer + len, "%d:", port);
    mungestr(hostname, buffer + strlen(buffer));
}

int reg_verify_host_key(const char *hostname, int port,
		    const char *keytype, const char *key)
{
    char *otherstr, *regname;
    int len;
    HKEY rkey;
    DWORD readlen;
    DWORD type;
    int ret, compare;

    len = 1 + strlen(key);

    /*
     * Now read a saved key in from the registry and see what it
     * says.
     */
    regname = snewn(3 * (strlen(hostname) + strlen(keytype)) + 15, char);

    hostkey_regname(regname, hostname, port, keytype);

    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys",
		   &rkey) != ERROR_SUCCESS) {
        sfree(regname);
	return 1;		       /* key does not exist in registry */
    }

    readlen = len;
    otherstr = snewn(len, char);
    ret = RegQueryValueEx(rkey, regname, NULL, &type, otherstr, &readlen);

    if (ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA &&
	!strcmp(keytype, "rsa")) {
	/*
	 * Key didn't exist. If the key type is RSA, we'll try
	 * another trick, which is to look up the _old_ key format
	 * under just the hostname and translate that.
	 */
	char *justhost = regname + 1 + strcspn(regname, ":");
	char *oldstyle = snewn(len + 10, char);	/* safety margin */
	readlen = len;
	ret = RegQueryValueEx(rkey, justhost, NULL, &type,
			      oldstyle, &readlen);

	if (ret == ERROR_SUCCESS && type == REG_SZ) {
	    /*
	     * The old format is two old-style bignums separated by
	     * a slash. An old-style bignum is made of groups of
	     * four hex digits: digits are ordered in sensible
	     * (most to least significant) order within each group,
	     * but groups are ordered in silly (least to most)
	     * order within the bignum. The new format is two
	     * ordinary C-format hex numbers (0xABCDEFG...XYZ, with
	     * A nonzero except in the special case 0x0, which
	     * doesn't appear anyway in RSA keys) separated by a
	     * comma. All hex digits are lowercase in both formats.
	     */
	    char *p = otherstr;
	    char *q = oldstyle;
	    int i, j;

	    for (i = 0; i < 2; i++) {
		int ndigits, nwords;
		*p++ = '0';
		*p++ = 'x';
		ndigits = strcspn(q, "/");	/* find / or end of string */
		nwords = ndigits / 4;
		/* now trim ndigits to remove leading zeros */
		while (q[(ndigits - 1) ^ 3] == '0' && ndigits > 1)
		    ndigits--;
		/* now move digits over to new string */
		for (j = 0; j < ndigits; j++)
		    p[ndigits - 1 - j] = q[j ^ 3];
		p += ndigits;
		q += nwords * 4;
		if (*q) {
		    q++;	       /* eat the slash */
		    *p++ = ',';	       /* add a comma */
		}
		*p = '\0';	       /* terminate the string */
	    }

	    /*
	     * Now _if_ this key matches, we'll enter it in the new
	     * format. If not, we'll assume something odd went
	     * wrong, and hyper-cautiously do nothing.
	     */
	    if (!strcmp(otherstr, key))
		RegSetValueEx(rkey, regname, 0, REG_SZ, otherstr,
			      strlen(otherstr) + 1);
	}

        sfree(oldstyle);
    }

    RegCloseKey(rkey);

    compare = strcmp(otherstr, key);

    sfree(otherstr);
    sfree(regname);

    if (ret == ERROR_MORE_DATA ||
	(ret == ERROR_SUCCESS && type == REG_SZ && compare))
	return 2;		       /* key is different in registry */
    else if (ret != ERROR_SUCCESS || type != REG_SZ)
	return 1;		       /* key does not exist in registry */
    else
	return 0;		       /* key matched OK in registry */
}

void reg_store_host_key(const char *hostname, int port,
		    const char *keytype, const char *key)
{
    char *regname;
    HKEY rkey;

    regname = snewn(3 * (strlen(hostname) + strlen(keytype)) + 15, char);

    hostkey_regname(regname, hostname, port, keytype);

    if (RegCreateKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys",
		     &rkey) == ERROR_SUCCESS) {
	RegSetValueEx(rkey, regname, 0, REG_SZ, key, strlen(key) + 1);
	RegCloseKey(rkey);
    } /* else key does not exist in registry */

    sfree(regname);
}

/* ----------------------------------------------------------------------
 * PUTTY FILE HELPERS (not part of storage.h)
 */
/* JK: my generic function for simplyfing error reporting */
static DWORD errorShow(const char* pcErrText, const char* pcErrParam) {

    HWND hwRodic;
    DWORD erChyba;
    char pcBuf[16];
    char* pcHlaska = snewn((pcErrParam ? strlen(pcErrParam) : 0) + strlen(pcErrText) + 31, char);
    
    erChyba = GetLastError();        
    _ltoa(erChyba, pcBuf, 10);

    strcpy(pcHlaska, "Error: ");
    strcat(pcHlaska, pcErrText);
    strcat(pcHlaska, "\n");    

    if (pcErrParam) {
        strcat(pcHlaska, pcErrParam);
        strcat(pcHlaska, "\n");
    }
    strcat(pcHlaska, "Error code: ");
    strcat(pcHlaska, pcBuf);

    /* JK: get parent-window and show */
    hwRodic = GetActiveWindow();
    if (hwRodic != NULL) { hwRodic = GetLastActivePopup(hwRodic);}
        if (MessageBox(hwRodic, pcHlaska, "Error", MB_OK|MB_APPLMODAL|MB_ICONEXCLAMATION) == 0) {
        /* JK: this is really bad -> just ignore */
        return 0;
    }

    sfree(pcHlaska);
    return erChyba;
};

/* JK: pack string for use as filename - pack < > : " / \ | */
static void packstr(const char *in, char *out) {
    while (*in) {
	if (*in == '<' || *in == '>' || *in == ':' || *in == '"' ||
	    *in == '/' || *in == '|') {
	    *out++ = '%';
	    *out++ = hex[((unsigned char) *in) >> 4];
	    *out++ = hex[((unsigned char) *in) & 15];
	} else
	    *out++ = *in;
	in++;
    }
    *out = '\0';
}

/*
 * JK: create directory if specified as dir1\dir2\dir3 and dir1|2 doesn't exists
 * handle if part of path already exists
*/
int createPath(char* dir) {
    char *p;

    p = strrchr(dir, '\\');

    if (p == NULL) {
	/* what if it already exists */
	if (!SetCurrentDirectory(dir)) {
	    CreateDirectory(dir, NULL);
	    return SetCurrentDirectory(dir);
	}
	return 1;
    }
	
    *p = '\0';
    createPath(dir);
    *p = '\\';
    ++p;

    /* what if it already exists */
    if (!SetCurrentDirectory(dir)) {
	CreateDirectory(p, NULL);
	return SetCurrentDirectory(p);
    }
    return 1;
}

/*
 * JK: join path pcMain.pcSuf solving extra cases to pcDest
 * expecting - pcMain as path from WinAPI ::GetCurrentDirectory()/GetModuleFileName()
 *           - pcSuf as user input path from config (at least MAX_PATH long)
*/
char* joinPath(char* pcDest, char* pcMain, char* pcSuf) {

    char* pcBuf = snewn(MAX_PATH+1, char);

    /* at first ExpandEnvironmentStrings */
    if (0 == ExpandEnvironmentStrings(pcSuf, pcBuf, MAX_PATH)) {
	/* JK: failure -> revert back - but it ussualy won't work, so report error to user! */
	errorShow("Unable to ExpandEnvironmentStrings for session path", pcSuf);
	strncpy(pcBuf, pcSuf, strlen(pcSuf));
    }
    /* now ExpandEnvironmentStringsForUser - only on win2000Pro and above */
    /* It's much more tricky than I've expected, so it's ToDo */
    /*
    static HMODULE userenv_module = NULL;
    typedef BOOL (WINAPI *p_ExpandESforUser_t) (HANDLE, LPCTSTR, LPTSTR, DWORD);
    static p_ExpandESforUser_t p_ExpandESforUser = NULL;
	
    HMODULE userenv_module = LoadLibrary("USERENV.DLL");

    if (userenv_module) {
	p_ExpandESforUser = (p_ExpandESforUser_t) GetProcAddress(shell32_module, "ExpandEnvironmentStringsForUserA");
		
	if (p_ExpandESforUser) {

	    TOKEN_IMPERSONATE

	    if (0 == (p_ExpandESforUser(NULL, pcSuf, pcBuf,	MAX_PATH))) {
	    *//* JK: failure -> revert back - but it ussualy won't work, so report error to user! *//*
		errorShow("Unable to ExpandEnvironmentStringsForUser for session path", pcBuf);
		strncpy(pcSuf, pcBuf, strlen(pcSuf));
	    }
	}
    }*/

    /* expand done, resutl in pcBuf */

    if ((*pcBuf == '/') || (*pcBuf == '\\')) {
	/* everything ok */
	strcpy(pcDest, pcMain);
	strcat(pcDest, pcBuf);
    }
    else {
	if (*(pcBuf+1) == ':') {
	    /* absolute path */
	    strcpy(pcDest, pcBuf);
	}
	else {
	    /* some weird relative path - add '\' */
	    strcpy(pcDest, pcMain);
	    strcat(pcDest, "\\");
	    strcat(pcDest, pcBuf);
	}
    }
    sfree(pcBuf);
    return pcDest;
}

/*
 * JK: init path variables from config or otherwise
 * as of 1.5 GetModuleFileName solves our currentDirectory problem
*/
int loadPath() {

    char *fileCont = NULL;
    DWORD fileSize;
    DWORD bytesRead;
    char *p = NULL;
    char *p2 = NULL;
    HANDLE hFile;

    char* puttypath = snewn( (MAX_PATH*2), char);

    /* JK:  save path/curdir */
    GetCurrentDirectory( (MAX_PATH*2), oldpath);

    /* JK: get where putty.exe is */
    if (GetModuleFileName(NULL, puttypath, (MAX_PATH*2)) != 0)
    {
        p = strrchr(puttypath, '\\');
        if (p)
        {
            *p = '\0';
        }
        SetCurrentDirectory(puttypath);
    }
    else GetCurrentDirectory( (MAX_PATH*2), puttypath);

    /* JK: set default values - if there is a config file, it will be overwitten */
    strcpy(sesspath, puttypath);
    strcat(sesspath, "\\sessions");
    strcpy(sshkpath, puttypath);
    strcat(sshkpath, "\\sshhostkeys");
    strcpy(seedpath, puttypath);
    strcat(seedpath, "\\putty.rnd");

    hFile = CreateFile("putty.conf",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

    /* JK: now we can pre-clean-up */
    SetCurrentDirectory(oldpath);

    if (hFile != INVALID_HANDLE_VALUE) {
        fileSize = GetFileSize(hFile, NULL);
        fileCont = snewn(fileSize+16, char);

        if (!ReadFile(hFile, fileCont, fileSize, &bytesRead, NULL)) {
            errorShow("Unable to read configuration file, falling back to defaults", NULL);
        
            /* JK: default values are already there and clean-up at end */
        }
        else {
            /* JK: parse conf file to path variables */
            *(fileCont+fileSize) = '\0';
            p = fileCont;
            while (p) {
                if (*p == ';') {    /* JK: comment -> skip line */
                    p = strchr(p, '\n');
                    ++p;
                    continue;
                }
                p2 = strchr(p, '=');
                if (!p2) break;
                *p2 = '\0';
                ++p2;

                if (!strcmp(p, "sessions")) {
                    p = strchr(p2, '\n');
                    *p = '\0';
                    joinPath(sesspath, puttypath, p2);
                    p2 = sesspath+strlen(sesspath)-1;
                    while ((*p2 == ' ')||(*p2 == '\n')||(*p2 == '\r')||(*p2 == '\t')) --p2;
                    *(p2+1) = '\0';
                }
                else if (!strcmp(p, "sshhostkeys")) {
                    p = strchr(p2, '\n');
                    *p = '\0';
                    joinPath(sshkpath, puttypath, p2);
                    p2 = sshkpath+strlen(sshkpath)-1;
                    while ((*p2 == ' ')||(*p2 == '\n')||(*p2 == '\r')||(*p2 == '\t')) --p2;
                    *(p2+1) = '\0';
                }
                else if (!strcmp(p, "seedfile")) {
                    p = strchr(p2, '\n');
                    *p = '\0';
                    joinPath(seedpath, puttypath, p2);            
                    p2 = seedpath+strlen(seedpath)-1;
                    while ((*p2 == ' ')||(*p2 == '\n')||(*p2 == '\r')||(*p2 == '\t')) --p2;
                    *(p2+1) = '\0';
                }
                else if (!strcmp(p, "sessionsuffix")) {
                    p = strchr(p2, '\n');
                    *p = '\0';
                    strcpy(sessionsuffix, p2);
                    p2 = sessionsuffix+strlen(sessionsuffix)-1;
                    while ((*p2 == ' ')||(*p2 == '\n')||(*p2 == '\r')||(*p2 == '\t')) --p2;
                    *(p2+1) = '\0';
                }
                else if (!strcmp(p, "keysuffix")) {
                    p = strchr(p2, '\n');
                    *p = '\0';
                    strcpy(keysuffix, p2);
                    p2 = keysuffix+strlen(keysuffix)-1;
                    while ((*p2 == ' ')||(*p2 == '\n')||(*p2 == '\r')||(*p2 == '\t')) --p2;
                    *(p2+1) = '\0';
                }
                ++p;
            }
        }
        CloseHandle(hFile);
        sfree(fileCont);
    }
    /* else - INVALID_HANDLE {
     * JK: unable to read conf file - probably doesn't exists
     * we won't create one, user wants putty light, just fall back to defaults
     * and defaults are already there
    }*/

    sfree(puttypath);
    return 1;
}


/* ---------------------------------------------------------------------------------------------------------
 * ---------------------------------------------------------------------------------------------------------
 * FILE FUNCTIONS
 * ---------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------------------------------------------------*/
void *file_open_settings_w(const char *sessionname, char **errmsg)
{
    char *p;
    struct setPack* sp;
    *errmsg = NULL;

    if (!sessionname || !*sessionname) {
	sessionname = "Default Settings";
    }

    /* JK: if sessionname contains [registry] -> cut it off */
    /*if ( *(sessionname+strlen(sessionname)-1) == ']') {
	    p = strrchr(sessionname, '[');
	    *(p-1) = '\0';
    }*/

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);

    sp = snew( struct setPack );
    sp->fromFile = 0;
    sp->handle = NULL;

    /* JK: secure pack for filename */
    sp->fileBuf = snewn(3 * strlen(p) + 1 + 16, char);
    packstr(p, sp->fileBuf);
    strcat(sp->fileBuf, sessionsuffix);
    sfree(p);

    return sp;
}

void file_write_setting_s(void *handle, const char *key, const char *value)
{
    struct setItem *st;

    if (handle) {
        /* JK: counting max lenght of keys/values */
        ((struct setPack*) handle)->fromFile = max(((struct setPack*) handle)->fromFile, strlen(key)+1);
        ((struct setPack*) handle)->fromFile = max(((struct setPack*) handle)->fromFile, strlen(value)+1);

        st = ((struct setPack*) handle)->handle;
        while (st) {
            if ( strcmp(st->key, key) == 0) {
                /* this key already set -> reset */
                sfree(st->value);
                st->value = snewn( strlen(value)+1, char);
                strcpy(st->value, value);
                return;
            }
            st = st->next;
        }
        /* JK: key not found -> add to begin */
        st = snew( struct setItem );
        st->key = snewn( strlen(key)+1, char);
        strcpy(st->key, key);
        st->value = snewn( strlen(value)+1, char);
        strcpy(st->value, value);
        st->next = ((struct setPack*) handle)->handle;
        ((struct setPack*) handle)->handle = st;
    }
}

void file_write_setting_i(void *handle, const char *key, int value)
{
    struct setItem *st;

    if (handle) {
        /* JK: counting max lenght of keys/values */
        ((struct setPack*) handle)->fromFile = max(((struct setPack*) handle)->fromFile, strlen(key)+1);

        st = ((struct setPack*) handle)->handle;
        while (st) {
            if ( strcmp(st->key, key) == 0) {
                /* this key already set -> reset */
                sfree(st->value);
                st->value = snewn(16, char);
                _itoa(value, st->value, 10);
                return;
            }
            st = st->next;
        }
        /* JK: key not found -> add to begin */
        st = snew( struct setItem );
        st->key = snewn( strlen(key)+1, char);
        strcpy(st->key, key);
        st->value = snewn(16, char);
        _itoa(value, st->value, 10);
        st->next = ((struct setPack*) handle)->handle;
        ((struct setPack*) handle)->handle = st;
    }
}

void file_close_settings_w(void *handle)
{
    HANDLE hFile;
    DWORD written;
    WIN32_FIND_DATA FindFile;
    char *p;
    struct setItem *st1,*st2;
    int writeok;

    if (!handle) return;

    /* JK: we will write to disk now - open file, filename stored in handle already packed */
    if ((hFile = FindFirstFile(sesspath, &FindFile)) == INVALID_HANDLE_VALUE) {
        if (!createPath(sesspath)) {
            errorShow("Unable to create directory for storing sessions", sesspath);
            return;
        }
    }
    FindClose(hFile);
    GetCurrentDirectory( (MAX_PATH*2), oldpath);
    SetCurrentDirectory(sesspath);

    hFile = CreateFile( ((struct setPack*) handle)->fileBuf, GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        errorShow("Unable to open file for writing", ((struct setPack*) handle)->fileBuf );
        return;
    }

    /* JK: allocate enough memory for all keys/values */
    p = snewn( max( 3* ((struct setPack*) handle)->fromFile ,16), char);

    /* JK: process linked list */
    st1 = ((struct setPack*) handle)->handle;
    writeok = 1;

    while (st1) {
        mungestr(st1->key, p);
        writeok = writeok && WriteFile( (HANDLE) hFile, p, strlen(p), &written, NULL);
        writeok = writeok && WriteFile( (HANDLE) hFile, "\\", 1, &written, NULL);

        mungestr(st1->value, p);
        writeok = writeok && WriteFile( (HANDLE) hFile, p, strlen(p), &written, NULL);
        writeok = writeok && WriteFile( (HANDLE) hFile, "\\\n", 2, &written, NULL);

        if (!writeok) {
            errorShow("Unable to save settings", st1->key);
            return;
            /* JK: memory should be freed here - fixme */
        }

        st2 = st1->next;
        sfree(st1->key);
        sfree(st1->value);
        sfree(st1);
        st1 = st2;
    }

    sfree(((struct setPack*) handle)->fileBuf);
    CloseHandle( (HANDLE)hFile );
    SetCurrentDirectory(oldpath);
}

void *file_open_settings_r(const char *sessionname)
{
    HKEY subkey1, sesskey;
    char *p;
    char *fileCont;
    DWORD fileSize;
    DWORD bytesRead;
    HANDLE hFile;
    struct setPack* sp;
    struct setItem *st1, *st2;

    sp = snew( struct setPack );

    if (!sessionname || !*sessionname) {
        sessionname = "Default Settings";
    }

    /* JK: in the first call of this function we initialize path variables */
    if (*sesspath == '\0') {
        loadPath();
    }

    /* JK: if sessionname contains [registry] -> cut it off in another buffer */
    /*if ( *(sessionname+strlen(sessionname)-1) == ']') {
        ses = snewn(strlen(sessionname)+1, char);
        strcpy(ses, sessionname);

        p = strrchr(ses, '[');
        *(p-1) = '\0';

        p = snewn(3 * strlen(ses) + 1, char);
        mungestr(ses, p);
        sfree(ses);

        sp->fromFile = 0;
    }
    else {*/
        p = snewn(3 * strlen(sessionname) + 1 + 16, char);
        mungestr(sessionname, p);
        strcat(p, sessionsuffix);

        sp->fromFile = 1;
    //}

    /* JK: default settings must be read from registry */
    /* 8.1.2007 - 0.1.6 try to load them from file if exists - nasty code duplication */
    if (!strcmp(sessionname, "Default Settings")) {
        GetCurrentDirectory( (MAX_PATH*2), oldpath);
        if (SetCurrentDirectory(sesspath)) {
            hFile = CreateFile(p, GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        }
        else {
            hFile = INVALID_HANDLE_VALUE;
        }
        SetCurrentDirectory(oldpath);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            sp->fromFile = 0;
        }
        else {
            sp->fromFile = 1;
            CloseHandle(hFile);
        }
    }

    if (sp->fromFile) {
        /* JK: session is in file -> open dir/file */
        GetCurrentDirectory( (MAX_PATH*2), oldpath);
        if (SetCurrentDirectory(sesspath)) {
            hFile = CreateFile(p, GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        }
        else {
            hFile = INVALID_HANDLE_VALUE;
        }
        SetCurrentDirectory(oldpath);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            errorShow("Unable to read session from file", p);
            sfree(p);
            return NULL;
        }

        /* JK: succes -> load structure setPack from file */
        fileSize = GetFileSize(hFile, NULL);
        fileCont = snewn(fileSize+16, char);

        if (!ReadFile(hFile, fileCont, fileSize, &bytesRead, NULL)) {
            errorShow("Unable to read session from file", p);
            sfree(p);
            return NULL;
        }
        sfree(p);

        st1 = snew( struct setItem );
        sp->fromFile = 1;
        sp->handle = st1;
        
        p = fileCont;
        sp->fileBuf = fileCont; /* JK: remeber for memory freeing */

        /* pJK: arse file in format:
         * key1\value1\
         * ...
        */
        while (p < (fileCont+fileSize)) {
            st1->key = p;
            p = strchr(p, '\\');
            if (!p) break;
            *p = '\0';
            ++p;
            st1->value = p;
            p = strchr(p, '\\');
            if (!p) break;
            *p = '\0';
            ++p;
            ++p; /* for "\\\n" - human readable files */

            st2 = snew( struct setItem );
            st2->next = NULL;
            st2->key = NULL;
            st2->value = NULL;

            st1->next = st2;
            st1 = st2;
        }
        CloseHandle(hFile);
    }
    else {
        /* JK: session is in registry */
        if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &subkey1) != ERROR_SUCCESS) {
            sesskey = NULL;
        }
        else {
            if (RegOpenKey(subkey1, p, &sesskey) != ERROR_SUCCESS) {
                sesskey = NULL;
            }
            RegCloseKey(subkey1);
        }
        sp->fromFile = 0;
        sp->handle = sesskey;
        sfree(p);
    }

    return sp;
}

char *file_read_setting_s(void *handle, const char *key)
{
    struct setItem *st;
    char *p;

    if (!handle) return NULL;    /* JK: new in 0.1.3 */

    if (((struct setPack*) handle)->fromFile) {
        
        p = snewn(3 * strlen(key) + 1, char);
        mungestr(key, p);

        st = ((struct setPack*) handle)->handle;
        while (st->key) {
            if ( strcmp(st->key, p) == 0) {
                const size_t buflen = 1024*16;
                char *buffer = snewn(1024*16, char);
                char *ret;
                unmungestr(st->value, buffer, buflen);
                ret = snewn(strlen(buffer) + 1, char);
                strcpy(ret, buffer);
                sfree(buffer);
                return ret;
            }
            st = st->next;
        }
    }
    else {
        return reg_read_setting_s(((struct setPack*) handle)->handle, key);
    }
    return NULL;
}

int file_read_setting_i(void *handle, const char *key, int defvalue)
{
    DWORD type, val, size;
    struct setItem *st;
    size = sizeof(val);

	if (!handle) return 0;    /* JK: new in 0.1.3 */

    if (((struct setPack*) handle)->fromFile) {
        st = ((struct setPack*) handle)->handle;
        while (st->key) {
            if ( strcmp(st->key, key) == 0) {
                return atoi(st->value);                
            }
            st = st->next;
        }
    }
    else {
        handle = ((struct setPack*) handle)->handle;

        if (!handle || RegQueryValueEx((HKEY) handle, key, 0, &type, (BYTE *) &val, &size) != ERROR_SUCCESS || size != sizeof(val) || type != REG_DWORD) {
            return defvalue;
        }
        else {
            return val;
        }
    }
    /* JK: should not end here -> value not found in file */
    return defvalue;
}

void file_close_settings_r(void *handle)
{
    if (!handle) return;    /* JK: new in 0.1.3 */

    if (((struct setPack*) handle)->fromFile) {
        struct setItem *st1, *st2;

        st1 = ((struct setPack*) handle)->handle;
        while (st1) {
            st2 = st1->next;
            sfree(st1);
            st1 = st2;
        }
        sfree( ((struct setPack*) handle)->fileBuf );
        sfree(handle);
    }
    else {
        handle = ((struct setPack*) handle)->handle;
        RegCloseKey((HKEY) handle);
    }
}

void file_del_settings(const char *sessionname)
{
    char *p;
    char *p2;

    /* JK: if sessionname contains [registry] -> cut it off and delete from registry */
    /*if ( *(sessionname+strlen(sessionname)-1) == ']') {

        p = strrchr(sessionname, '[');
        *(p-1) = '\0';

        p = snewn(3 * strlen(sessionname) + 1, char);
        mungestr(sessionname, p);
        
        if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &subkey1) != ERROR_SUCCESS)    return;

        RegDeleteKey(subkey1, p);
        RegCloseKey(subkey1);
    }
    else {*/
        /* JK: delete from file - file itself */

        p = snewn(3 * strlen(sessionname) + strlen(sessionsuffix) + 1, char);
        mungestr(sessionname, p);
        strcat(p, sessionsuffix);
        p2 = snewn(3 * strlen(p) + 1, char);
        packstr(p, p2);

        GetCurrentDirectory( (MAX_PATH*2), oldpath);
        if (SetCurrentDirectory(sesspath)) {
            if (!DeleteFile(p2))
            {
                errorShow("Unable to delete settings.", NULL);
            }
            SetCurrentDirectory(oldpath);
        }
    //}

    sfree(p);
}

void *file_enum_settings_start(void)
{
    struct enumsettings *ret;
    HKEY key;

	/* JK: in the first call of this function we can initialize path variables */
    if (*sesspath == '\0') {
        loadPath();
    }
    /* JK: we have path variables */
    
    /* JK: let's do what this function should normally do */
    ret = snew(struct enumsettings);

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &key) != ERROR_SUCCESS) {
        /*
         * JK: nothing in registry -> pretend we found it, first call to file_enum_settings_next
         * will solve this by starting scanning dir sesspath
        */
    }
    ret->key = key;
    ret->fromFile = 0;
    ret->hFile = NULL;
    ret->i = 0;

    return ret;
}

char *file_enum_settings_next(void *handle, char *buffer, int buflen)
{
    struct enumsettings *e = (struct enumsettings *) handle;
    WIN32_FIND_DATA FindFileData;
    HANDLE hFile;
    char *otherbuf;
    
    if (!handle) return NULL;    /* JK: new in 0.1.3 */
    
    otherbuf = snewn( (3*buflen)+1, char); /* must be here */

    if (!e->fromFile) {

        /*if (RegEnumKey(e->key, e->i++, otherbuf, 3 * buflen) == ERROR_SUCCESS) {
            unmungestr(otherbuf, buffer, buflen);
            strcat(buffer, " [registry]");
            sfree(otherbuf);
            return buffer;
        }
        else {*/
            /* JK: registry scanning done, starting scanning directory "sessions" */
            e->fromFile = 1;
            GetCurrentDirectory( (MAX_PATH*2), oldpath);
            if (!SetCurrentDirectory(sesspath)) {
                sfree(otherbuf);
                return NULL;
            }
            hFile = FindFirstFile("*", &FindFileData);

            /* JK: skip directories (extra check for "." and ".." too, seems to bug on some machines) */
            while ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || FindFileData.cFileName[0] == '.') { // HACK: PUTTY TRAY / PUTTY FILE: Fixed directory check
                if (!FindNextFile(hFile,&FindFileData)) {
                    sfree(otherbuf);
                    return NULL;
                }
            }
            /* JK: a file found */
            if (hFile != INVALID_HANDLE_VALUE && !((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || FindFileData.cFileName[0] == '.')) { // HACK: PUTTY TRAY / PUTTY FILE: Fixed directory check
                e->hFile = hFile;
                unmungestr(FindFileData.cFileName, buffer, buflen);
                sfree(otherbuf);
                /* JK: cut off sessionsuffix */
                otherbuf = buffer + strlen(buffer) - strlen(sessionsuffix);
                if (strncmp(otherbuf, sessionsuffix, strlen(sessionsuffix)) == 0) {
                    *otherbuf = '\0';
                }
                return buffer;
            }
            else {
                /* JK: not a single file found -> give up */
                sfree(otherbuf);
                return NULL;
            }
        //}
    }
    else if (e->fromFile) {
        if (FindNextFile(e->hFile, &FindFileData)) {
	    if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || FindFileData.cFileName[0] == '.') {
                return enum_settings_next(handle, buffer, buflen);
	    }
            unmungestr(FindFileData.cFileName, buffer, buflen);
            sfree(otherbuf);
            /* JK: cut off sessionsuffix */
            otherbuf = buffer + strlen(buffer) - strlen(sessionsuffix);
            if (strncmp(otherbuf, sessionsuffix, strlen(sessionsuffix)) == 0) {
                *otherbuf = '\0';
            }
            return buffer;
        }
        else {
            sfree(otherbuf);
            return NULL;
        }
    }
    /* JK: should not end here */
    sfree(otherbuf);
    return NULL;
}

void file_enum_settings_finish(void *handle)
{
    struct enumsettings *e = (struct enumsettings *) handle;
    if (!handle) return;	/* JK: new in 0.1.3 */

    RegCloseKey(e->key);
    if (((struct enumsettings *)handle)->hFile != NULL) { FindClose(((struct enumsettings *)handle)->hFile); }
    SetCurrentDirectory(oldpath);
    sfree(e);
}

int file_verify_host_key(const char *hostname, int port,
		    const char *keytype, const char *key)
{
    char *otherstr, *regname;
    int len;
    HKEY rkey;
    DWORD readlen;
    DWORD type;
    int ret, compare, userMB;

    DWORD fileSize;
    DWORD bytesRW;
    char *p;
    HANDLE hFile;
    WIN32_FIND_DATA FindFile;

    len = 1 + strlen(key);

    /* Now read a saved key in from the registry and see what it says. */
    otherstr = snewn(len, char);
    regname = snewn(3 * (strlen(hostname) + strlen(keytype)) + 15, char);

    hostkey_regname(regname, hostname, port, keytype);

	/* JK: settings on disk - every hostkey as file in dir */
    GetCurrentDirectory( (MAX_PATH*2), oldpath);
    if (SetCurrentDirectory(sshkpath)) {
        
        p = snewn(3 * strlen(regname) + 1 + 16, char);
        packstr(regname, p);
        strcat(p, keysuffix);

        hFile = CreateFile(p, GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        SetCurrentDirectory(oldpath);

        if (hFile != INVALID_HANDLE_VALUE) {
            /* JK: ok we got it -> read it to otherstr */
            fileSize = GetFileSize(hFile, NULL);
            otherstr = snewn(fileSize+1, char);
            ReadFile(hFile, otherstr, fileSize, &bytesRW, NULL);
            *(otherstr+fileSize) = '\0';

            compare = strcmp(otherstr, key);

            CloseHandle(hFile);
            sfree(otherstr);
            sfree(regname);
            sfree(p);

            if (compare) { /* key is here, but different */
                return 2;
            }
            else { /* key is here and match */
                return 0;
            }
        }
        else {
            /* not found as file -> try registry */
            sfree(p);
        }
    }
    else {
        /* JK: there are no hostkeys as files -> try registry -> nothing to do here now */
    }
    
    /* JK: directory/file not found -> try registry */
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys", &rkey) != ERROR_SUCCESS) {
        return 1;               /* key does not exist in registry */
    }

    readlen = len;
    ret = RegQueryValueEx(rkey, regname, NULL, &type, otherstr, &readlen);

    if (ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA &&
	!strcmp(keytype, "rsa")) {
	/*
	 * Key didn't exist. If the key type is RSA, we'll try
	 * another trick, which is to look up the _old_ key format
	 * under just the hostname and translate that.
	 */
	char *justhost = regname + 1 + strcspn(regname, ":");
	char *oldstyle = snewn(len + 10, char);	/* safety margin */
	readlen = len;
	ret = RegQueryValueEx(rkey, justhost, NULL, &type,
			      oldstyle, &readlen);

	if (ret == ERROR_SUCCESS && type == REG_SZ) {
	    /*
	     * The old format is two old-style bignums separated by
	     * a slash. An old-style bignum is made of groups of
	     * four hex digits: digits are ordered in sensible
	     * (most to least significant) order within each group,
	     * but groups are ordered in silly (least to most)
	     * order within the bignum. The new format is two
	     * ordinary C-format hex numbers (0xABCDEFG...XYZ, with
	     * A nonzero except in the special case 0x0, which
	     * doesn't appear anyway in RSA keys) separated by a
	     * comma. All hex digits are lowercase in both formats.
	     */
	    char *p = otherstr;
	    char *q = oldstyle;
	    int i, j;

	    for (i = 0; i < 2; i++) {
		int ndigits, nwords;
		*p++ = '0';
		*p++ = 'x';
		ndigits = strcspn(q, "/");	/* find / or end of string */
		nwords = ndigits / 4;
		/* now trim ndigits to remove leading zeros */
		while (q[(ndigits - 1) ^ 3] == '0' && ndigits > 1)
		    ndigits--;
		/* now move digits over to new string */
		for (j = 0; j < ndigits; j++)
		    p[ndigits - 1 - j] = q[j ^ 3];
		p += ndigits;
		q += nwords * 4;
		if (*q) {
		    q++;	       /* eat the slash */
		    *p++ = ',';	       /* add a comma */
		}
		*p = '\0';	       /* terminate the string */
	    }

	    /*
	     * Now _if_ this key matches, we'll enter it in the new
	     * format. If not, we'll assume something odd went
	     * wrong, and hyper-cautiously do nothing.
	     */
	    if (!strcmp(otherstr, key))
		RegSetValueEx(rkey, regname, 0, REG_SZ, otherstr,
			      strlen(otherstr) + 1);
		/* JK: session is not saved to file - fixme */
	}
    }

    compare = strcmp(otherstr, key);

    if (ret == ERROR_MORE_DATA || (ret == ERROR_SUCCESS && type == REG_SZ && compare)) {
        RegCloseKey(rkey);
        return 2;               /* key is different in registry */
    }
    else if (ret != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(rkey);
        return 1;               /* key does not exist in registry */
    }
    else { /* key matched OK in registry */
        /* JK: matching key found in registry -> warn user, ask what to do */
        p = snewn(256, char);
        userMB = MessageBox(NULL, "The host key is cached in the Windows registry. "
            "Do you want to move it to a file? \n\n"
            "Yes \t-> Move to file (and delete from registry)\n"
            "No \t-> Copy to file (and keep in registry)\n"
            "Cancel \t-> nothing will be done\n", "Security risk", MB_YESNOCANCEL|MB_ICONWARNING);

        if ((userMB == IDYES) || (userMB == IDNO)) {
            /* JK: save key to file */
            if ((hFile = FindFirstFile(sshkpath, &FindFile)) == INVALID_HANDLE_VALUE) {
                createPath(sshkpath);
            }
            FindClose(hFile);
            SetCurrentDirectory(sshkpath);

            p = snewn(3*strlen(regname) + 1 + 16, char);
            packstr(regname, p);
            strcat(p, keysuffix);
            
            hFile = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            if (hFile == INVALID_HANDLE_VALUE) {
                errorShow("Unable to create file (key won't be deleted from registry)", p);
                userMB = IDNO;
            }
            else {
                if (!WriteFile(hFile, key, strlen(key), &bytesRW, NULL)) {
                    errorShow("Unable to save key to file (key won't be deleted from registry)", NULL);
                    userMB = IDNO;
                }
                CloseHandle(hFile);
            }
        }
        if (userMB == IDYES) {
            /* delete from registry */
            if (RegDeleteValue(rkey, regname) != ERROR_SUCCESS) {
                errorShow("Unable to delete registry value", regname);
            }
        }
        /* JK: else (Cancel) -> nothing to be done right now */
        
        RegCloseKey(rkey);

        sfree(otherstr);
        sfree(regname);
        return 0;               
    }
}

void file_store_host_key(const char *hostname, int port,
		    const char *keytype, const char *key)
{
    char *regname = NULL;
    WIN32_FIND_DATA FindFile;
    HANDLE hFile = NULL;
    char* p = NULL;
    DWORD bytesWritten;

    regname = snewn(3 * (strlen(hostname) + strlen(keytype)) + 15, char);
    hostkey_regname(regname, hostname, port, keytype);

	/* JK: save hostkey to file in dir */
    if ((hFile = FindFirstFile(sshkpath, &FindFile)) == INVALID_HANDLE_VALUE) {
        createPath(sshkpath);
    }
    FindClose(hFile);
    GetCurrentDirectory( (MAX_PATH*2), oldpath);
    SetCurrentDirectory(sshkpath);

    p = snewn(3*strlen(regname) + 1, char);
    packstr(regname, p);
    strcat(p, keysuffix);
    hFile = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        errorShow("Unable to create file", p);
    }
    else {
        if (!WriteFile(hFile, key, strlen(key), &bytesWritten, NULL)) {
            errorShow("Unable to save key to file", NULL);
        }
        CloseHandle(hFile);
    }
    SetCurrentDirectory(oldpath);

    sfree(p);
    sfree(regname);
}

/*
 * Open (or delete) the random seed file.
 */
enum { DEL, OPEN_R, OPEN_W };
static int try_random_seed(char const *path, int action, HANDLE *ret)
{
    if (action == DEL) {
        if (!DeleteFile(path) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            nonfatal("Unable to delete '%s': %s", path,
                     win_strerror(GetLastError()));
        }
	*ret = INVALID_HANDLE_VALUE;
	return FALSE;		       /* so we'll do the next ones too */
    }

    *ret = CreateFile(path,
		      action == OPEN_W ? GENERIC_WRITE : GENERIC_READ,
		      action == OPEN_W ? 0 : (FILE_SHARE_READ |
					      FILE_SHARE_WRITE),
		      NULL,
		      action == OPEN_W ? CREATE_ALWAYS : OPEN_EXISTING,
		      action == OPEN_W ? FILE_ATTRIBUTE_NORMAL : 0,
		      NULL);

    return (*ret != INVALID_HANDLE_VALUE);
}

static HANDLE access_random_seed(int action)
{
    HKEY rkey;
    DWORD type, size;
    HANDLE rethandle;
    char seedpath[2 * MAX_PATH + 10] = "\0";

    /* PuttyTray / PuTTY File - HACK STARTS HERE */
    assert(seedpath != '\0'); {
         /* JK: In PuTTY 0.58 this won't ever happen - this function was called only if (!seedpath[0])
         * This changed in PuTTY 0.59 - read the long comment below
         */

    }
    /* PuttyTray / PuTTY File - HACK ENDS HERE */

    /*
     * Iterate over a selection of possible random seed paths until
     * we find one that works.
     * 
     * We do this iteration separately for reading and writing,
     * meaning that we will automatically migrate random seed files
     * if a better location becomes available (by reading from the
     * best location in which we actually find one, and then
     * writing to the best location in which we can _create_ one).
     */

    /*
     * First, try the location specified by the user in the
     * Registry, if any.
     */
    size = sizeof(seedpath);
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS, &rkey) ==
	ERROR_SUCCESS) {
	int ret = RegQueryValueEx(rkey, "RandSeedFile",
				  0, &type, seedpath, &size);
	if (ret != ERROR_SUCCESS || type != REG_SZ)
	    seedpath[0] = '\0';
	RegCloseKey(rkey);

	if (*seedpath && try_random_seed(seedpath, action, &rethandle))
	    return rethandle;
    }

    /*
     * Next, try the user's local Application Data directory,
     * followed by their non-local one. This is found using the
     * SHGetFolderPath function, which won't be present on all
     * versions of Windows.
     */
    if (!tried_shgetfolderpath) {
	/* This is likely only to bear fruit on systems with IE5+
	 * installed, or WinMe/2K+. There is some faffing with
	 * SHFOLDER.DLL we could do to try to find an equivalent
	 * on older versions of Windows if we cared enough.
	 * However, the invocation below requires IE5+ anyway,
	 * so stuff that. */
	shell32_module = load_system32_dll("shell32.dll");
	GET_WINDOWS_FUNCTION(shell32_module, SHGetFolderPathA);
	tried_shgetfolderpath = TRUE;
    }
    if (p_SHGetFolderPathA) {
	if (SUCCEEDED(p_SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA,
					 NULL, SHGFP_TYPE_CURRENT, seedpath))) {
	    strcat(seedpath, "\\PUTTY.RND");
	    if (try_random_seed(seedpath, action, &rethandle))
		return rethandle;
	}

	if (SUCCEEDED(p_SHGetFolderPathA(NULL, CSIDL_APPDATA,
					 NULL, SHGFP_TYPE_CURRENT, seedpath))) {
	    strcat(seedpath, "\\PUTTY.RND");
	    if (try_random_seed(seedpath, action, &rethandle))
		return rethandle;
	}
    }

    /*
     * Failing that, try %HOMEDRIVE%%HOMEPATH% as a guess at the
     * user's home directory.
     */
    {
	int len, ret;

	len =
	    GetEnvironmentVariable("HOMEDRIVE", seedpath,
				   sizeof(seedpath));
	ret =
	    GetEnvironmentVariable("HOMEPATH", seedpath + len,
				   sizeof(seedpath) - len);
	if (ret != 0) {
	    strcat(seedpath, "\\PUTTY.RND");
	    if (try_random_seed(seedpath, action, &rethandle))
		return rethandle;
	}
    }

    /*
     * And finally, fall back to C:\WINDOWS.
     */
    GetWindowsDirectory(seedpath, sizeof(seedpath));
    strcat(seedpath, "\\PUTTY.RND");
    if (try_random_seed(seedpath, action, &rethandle))
	return rethandle;

    /*
     * If even that failed, give up.
     */
    return INVALID_HANDLE_VALUE;
}

void read_random_seed(noise_consumer_t consumer)
{
    HANDLE seedf = access_random_seed(OPEN_R);

    if (seedf != INVALID_HANDLE_VALUE) {
	while (1) {
	    char buf[1024];
	    DWORD len;

	    if (ReadFile(seedf, buf, sizeof(buf), &len, NULL) && len)
		consumer(buf, len);
	    else
		break;
	}
	CloseHandle(seedf);
    }
}

void write_random_seed(void *data, int len)
{
    HANDLE seedf = access_random_seed(OPEN_W);

    if (seedf != INVALID_HANDLE_VALUE) {
	DWORD lenwritten;

	WriteFile(seedf, data, len, &lenwritten, NULL);
	CloseHandle(seedf);
    }
}

/*
 * Internal function supporting the jump list registry code. All the
 * functions to add, remove and read the list have substantially
 * similar content, so this is a generalisation of all of them which
 * transforms the list in the registry by prepending 'add' (if
 * non-null), removing 'rem' from what's left (if non-null), and
 * returning the resulting concatenated list of strings in 'out' (if
 * non-null).
 */
static int transform_jumplist_registry
    (const char *add, const char *rem, char **out)
{
    int ret;
    HKEY pjumplist_key, psettings_tmp;
    DWORD type;
    DWORD value_length;
    char *old_value, *new_value;
    char *piterator_old, *piterator_new, *piterator_tmp;

    ret = RegCreateKeyEx(HKEY_CURRENT_USER, reg_jumplist_key, 0, NULL,
                         REG_OPTION_NON_VOLATILE, (KEY_READ | KEY_WRITE), NULL,
                         &pjumplist_key, NULL);
    if (ret != ERROR_SUCCESS) {
	return JUMPLISTREG_ERROR_KEYOPENCREATE_FAILURE;
    }

    /* Get current list of saved sessions in the registry. */
    value_length = 200;
    old_value = snewn(value_length, char);
    ret = RegQueryValueEx(pjumplist_key, reg_jumplist_value, NULL, &type,
                          old_value, &value_length);
    /* When the passed buffer is too small, ERROR_MORE_DATA is
     * returned and the required size is returned in the length
     * argument. */
    if (ret == ERROR_MORE_DATA) {
        sfree(old_value);
        old_value = snewn(value_length, char);
        ret = RegQueryValueEx(pjumplist_key, reg_jumplist_value, NULL, &type,
                              old_value, &value_length);
    }

    if (ret == ERROR_FILE_NOT_FOUND) {
        /* Value doesn't exist yet. Start from an empty value. */
        *old_value = '\0';
        *(old_value + 1) = '\0';
    } else if (ret != ERROR_SUCCESS) {
        /* Some non-recoverable error occurred. */
        sfree(old_value);
        RegCloseKey(pjumplist_key);
        return JUMPLISTREG_ERROR_VALUEREAD_FAILURE;
    } else if (type != REG_MULTI_SZ) {
        /* The value present in the registry has the wrong type: we
         * try to delete it and start from an empty value. */
        ret = RegDeleteValue(pjumplist_key, reg_jumplist_value);
        if (ret != ERROR_SUCCESS) {
            sfree(old_value);
            RegCloseKey(pjumplist_key);
            return JUMPLISTREG_ERROR_VALUEREAD_FAILURE;
        }

        *old_value = '\0';
        *(old_value + 1) = '\0';
    }

    /* Check validity of registry data: REG_MULTI_SZ value must end
     * with \0\0. */
    piterator_tmp = old_value;
    while (((piterator_tmp - old_value) < (value_length - 1)) &&
           !(*piterator_tmp == '\0' && *(piterator_tmp+1) == '\0')) {
        ++piterator_tmp;
    }

    if ((piterator_tmp - old_value) >= (value_length-1)) {
        /* Invalid value. Start from an empty value. */
        *old_value = '\0';
        *(old_value + 1) = '\0';
    }

    /*
     * Modify the list, if we're modifying.
     */
    if (add || rem) {
        /* Walk through the existing list and construct the new list of
         * saved sessions. */
        new_value = snewn(value_length + (add ? strlen(add) + 1 : 0), char);
        piterator_new = new_value;
        piterator_old = old_value;

        /* First add the new item to the beginning of the list. */
        if (add) {
            strcpy(piterator_new, add);
            piterator_new += strlen(piterator_new) + 1;
        }
        /* Now add the existing list, taking care to leave out the removed
         * item, if it was already in the existing list. */
        while (*piterator_old != '\0') {
            if (!rem || strcmp(piterator_old, rem) != 0) {
                /* Check if this is a valid session, otherwise don't add. */
                psettings_tmp = open_settings_r(piterator_old);
                if (psettings_tmp != NULL) {
                    close_settings_r(psettings_tmp);
                    strcpy(piterator_new, piterator_old);
                    piterator_new += strlen(piterator_new) + 1;
                }
            }
            piterator_old += strlen(piterator_old) + 1;
        }
        *piterator_new = '\0';
        ++piterator_new;

        /* Save the new list to the registry. */
        ret = RegSetValueEx(pjumplist_key, reg_jumplist_value, 0, REG_MULTI_SZ,
                            new_value, piterator_new - new_value);

        sfree(old_value);
        old_value = new_value;
    } else
        ret = ERROR_SUCCESS;

    /*
     * Either return or free the result.
     */
    if (out && ret == ERROR_SUCCESS)
        *out = old_value;
    else
        sfree(old_value);

    /* Clean up and return. */
    RegCloseKey(pjumplist_key);

    if (ret != ERROR_SUCCESS) {
        return JUMPLISTREG_ERROR_VALUEWRITE_FAILURE;
    } else {
        return JUMPLISTREG_OK;
    }
}

/* Adds a new entry to the jumplist entries in the registry. */
int add_to_jumplist_registry(const char *item)
{
    return transform_jumplist_registry(item, item, NULL);
}

/* Removes an item from the jumplist entries in the registry. */
int remove_from_jumplist_registry(const char *item)
{
    return transform_jumplist_registry(NULL, item, NULL);
}

/* Returns the jumplist entries from the registry. Caller must free
 * the returned pointer. */
char *get_jumplist_registry_entries (void)
{
    char *list_value;

    if (transform_jumplist_registry(NULL,NULL,&list_value) != JUMPLISTREG_OK) {
	list_value = snewn(2, char);
        *list_value = '\0';
        *(list_value + 1) = '\0';
    }
    return list_value;
}

/*
 * Recursively delete a registry key and everything under it.
 */
static void registry_recursive_remove(HKEY key)
{
    DWORD i;
    char name[MAX_PATH + 1];
    HKEY subkey;

    i = 0;
    while (RegEnumKey(key, i, name, sizeof(name)) == ERROR_SUCCESS) {
	if (RegOpenKey(key, name, &subkey) == ERROR_SUCCESS) {
	    registry_recursive_remove(subkey);
	    RegCloseKey(subkey);
	}
	RegDeleteKey(key, name);
    }
}

void cleanup_all(void)
{
    HKEY key;
    int ret;
    char name[MAX_PATH + 1];

    /* ------------------------------------------------------------
     * Wipe out the random seed file, in all of its possible
     * locations.
     */
    access_random_seed(DEL);

    /* ------------------------------------------------------------
     * Ask Windows to delete any jump list information associated
     * with this installation of PuTTY.
     */
    clear_jumplist();

    /* ------------------------------------------------------------
     * Destroy all registry information associated with PuTTY.
     */

    /*
     * Open the main PuTTY registry key and remove everything in it.
     */
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS, &key) ==
	ERROR_SUCCESS) {
	registry_recursive_remove(key);
	RegCloseKey(key);
    }
    /*
     * Now open the parent key and remove the PuTTY main key. Once
     * we've done that, see if the parent key has any other
     * children.
     */
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_PARENT,
		   &key) == ERROR_SUCCESS) {
	RegDeleteKey(key, PUTTY_REG_PARENT_CHILD);
	ret = RegEnumKey(key, 0, name, sizeof(name));
	RegCloseKey(key);
	/*
	 * If the parent key had no other children, we must delete
	 * it in its turn. That means opening the _grandparent_
	 * key.
	 */
	if (ret != ERROR_SUCCESS) {
	    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_GPARENT,
			   &key) == ERROR_SUCCESS) {
		RegDeleteKey(key, PUTTY_REG_GPARENT_CHILD);
		RegCloseKey(key);
	    }
	}
    }
    /*
     * Now we're done.
     */
}

#define CONCAT(...) __VA_ARGS__

#define STORAGE_TYPE_SWITCHER_FULL(has_return, return_t, method, types, calls) \
return_t method(types) { \
    if (storagetype == STORAGE_FILE) { \
        has_return file_##method(calls); \
    } else { \
        has_return reg_##method(calls); \
    } \
}

#define STORAGE_TYPE_SWITCHER(return_t, method, types, calls) \
    STORAGE_TYPE_SWITCHER_FULL(return, return_t, method, types, calls)

#define STORAGE_TYPE_SWITCHER_VOID(method, types, calls) \
    STORAGE_TYPE_SWITCHER_FULL(      , void, method, types, calls)

STORAGE_TYPE_SWITCHER(void *, open_settings_w,
    CONCAT(const char *sessionname, char **errmsg),
    CONCAT(sessionname, errmsg))

STORAGE_TYPE_SWITCHER_VOID(write_setting_s,
    CONCAT(void *handle, const char *key, const char *value),
    CONCAT(handle, key, value))

STORAGE_TYPE_SWITCHER_VOID(write_setting_i,
    CONCAT(void *handle, const char *key, int value),
    CONCAT(handle, key, value))

STORAGE_TYPE_SWITCHER_VOID(close_settings_w,
    CONCAT(void *handle),
    CONCAT(handle))

STORAGE_TYPE_SWITCHER(void *, open_settings_r,
    CONCAT(const char *sessionname),
    CONCAT(sessionname))

STORAGE_TYPE_SWITCHER(char *, read_setting_s,
    CONCAT(void *handle, const char *key),
    CONCAT(handle, key))

STORAGE_TYPE_SWITCHER(int, read_setting_i,
    CONCAT(void *handle, const char *key, int defvalue),
    CONCAT(handle, key, defvalue))

STORAGE_TYPE_SWITCHER_VOID(close_settings_r,
    CONCAT(void *handle),
    CONCAT(handle))

STORAGE_TYPE_SWITCHER_VOID(del_settings,
    CONCAT(const char *sessionname),
    CONCAT(sessionname))

STORAGE_TYPE_SWITCHER(char*, enum_settings_next,
    CONCAT(void *handle, char *buffer, int buflen),
    CONCAT(      handle,       buffer,     buflen))

STORAGE_TYPE_SWITCHER_VOID(enum_settings_finish,
    CONCAT(void *handle),
    CONCAT(      handle))

STORAGE_TYPE_SWITCHER(int, verify_host_key,
    CONCAT(const char *hostname, int port,
            const char *keytype, const char *key),
    CONCAT(hostname, port, keytype, key))

STORAGE_TYPE_SWITCHER_VOID(store_host_key,
    CONCAT(const char *hostname, int port,
            const char *keytype, const char *key),
    CONCAT(hostname, port, keytype, key))

STORAGE_TYPE_SWITCHER(void *, enum_settings_start,
    CONCAT(),
    CONCAT())