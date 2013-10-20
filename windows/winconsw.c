/*
 * winconsw.c - various interactive-prompt routines shared between
 * the Windows console PuTTY tools
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "putty.h"
#include "storage.h"
#include "ssh.h"

int console_batch_mode = FALSE;

static void *console_logctx = NULL;

int mboxprintf(DWORD style, const char* caption, const char *fmt, ...) {
    int r;
    char* msg;
    va_list ap;

    va_start(ap, fmt);
    msg = dupvprintf(fmt, ap);
    va_end(ap);

    r = MessageBox(NULL, msg, caption == NULL ? "plinkw" : caption, style | MB_SETFOREGROUND);
    sfree(msg);

    return r;
}

/*
 * Clean up and exit.
 */
void cleanup_exit(int code)
{
    void handle_cleanup();
    /*
     * Clean up.
     */
    sk_cleanup();

    random_save_seed();
#ifdef MSCRYPTOAPI
    crypto_wrapup();
#endif

    handle_cleanup();

    ExitThread(code);
}

void set_busy_status(void *frontend, int status)
{
}

void notify_remote_exit(void *frontend)
{
    void notify_exited();
    notify_exited();
}

void timer_change_notify(long next)
{
}

int verify_ssh_host_key(void *frontend, char *host, int port, char *keytype,
                        char *keystr, char *fingerprint,
                        void (*callback)(void *ctx, int result), void *ctx)
{
    int ret;

    static const char absentmsg_batch[] =
	"The server's host key is not cached in the registry. You\n"
	"have no guarantee that the server is the computer you\n"
	"think it is.\n"
	"The server's %s key fingerprint is:\n"
	"%s\n"
	"Connection abandoned.\n";
    static const char absentmsg[] =
	"The server's host key is not cached in the registry. You\n"
	"have no guarantee that the server is the computer you\n"
	"think it is.\n"
	"The server's %s key fingerprint is:\n"
	"%s\n"
	"If you trust this host, hit Yes to add the key to\n"
	"PuTTY's cache and carry on connecting.\n"
	"If you want to carry on connecting just once, without\n"
	"adding the key to the cache, hit No.\n"
	"If you do not trust this host, hit Cancel to abandon the\n"
	"connection.\n"
	"Store key in cache?";

    static const char wrongmsg_batch[] =
	"WARNING - POTENTIAL SECURITY BREACH!\n"
	"The server's host key does not match the one PuTTY has\n"
	"cached in the registry. This means that either the\n"
	"server administrator has changed the host key, or you\n"
	"have actually connected to another computer pretending\n"
	"to be the server.\n"
	"The new %s key fingerprint is:\n"
	"%s\n"
	"Connection abandoned.\n";
    static const char wrongmsg[] =
	"WARNING - POTENTIAL SECURITY BREACH!\n"
	"\n"
	"The server's host key does not match the one PuTTY has\n"
	"cached in the registry. This means that either the\n"
	"server administrator has changed the host key, or you\n"
	"have actually connected to another computer pretending\n"
	"to be the server.\n"
	"The new %s key fingerprint is:\n"
	"%s\n"
	"If you were expecting this change and trust the new key,\n"
	"hit Yes to update PuTTY's cache and continue connecting.\n"
	"If you want to carry on connecting but without updating\n"
	"the cache, hit No.\n"
	"If you want to abandon the connection completely, hit\n"
	"Cancel. Hitting Cancel is the ONLY guaranteed safe\n"
	"choice.\n"
	"Update cached key?";

    static const char abandoned[] = "Connection abandoned.\n";

    static const char mbtitle[] = "PuTTY Security Alert";
    int mbret;

    /*
     * Verify the key against the registry.
     */
    ret = verify_host_key(host, port, keytype, keystr);

    if (ret == 0)		       /* success - key matched OK */
	return 1;

    if (ret == 2) {		       /* key was different */
	if (console_batch_mode) {
	    mboxprintf(MB_ICONERROR | MB_OK, mbtitle, wrongmsg_batch, keytype, fingerprint);
            return 0;
	}
	mbret = mboxprintf(MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON3, mbtitle, wrongmsg, keytype, fingerprint);
    }
    if (ret == 1) {		       /* key was absent */
	if (console_batch_mode) {
	    mboxprintf(MB_ICONERROR | MB_OK, mbtitle, absentmsg_batch, keytype, fingerprint);
            return 0;
	}
	mbret = mboxprintf(MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON3, mbtitle, absentmsg, keytype, fingerprint);
    }

    if (mbret != IDCANCEL) {
	if (mbret == IDYES)
	    store_host_key(host, port, keytype, keystr);
        return 1;
    } else {
	mboxprintf(MB_ICONERROR | MB_OK, mbtitle, abandoned);
        return 0;
    }
}

void update_specials_menu(void *frontend)
{
}

/*
 * Ask whether the selected algorithm is acceptable (since it was
 * below the configured 'warn' threshold).
 */
int askalg(void *frontend, const char *algtype, const char *algname,
	   void (*callback)(void *ctx, int result), void *ctx)
{
    static const char mbtitle[] = "PuTTY Security Alert";
    static const char msg[] =
	"The first %s supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Continue with connection?";
    static const char msg_batch[] =
	"The first %s supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Connection abandoned.\n";
    static const char abandoned[] = "Connection abandoned.\n";

    int mbret;

    if (console_batch_mode) {
	mboxprintf(MB_ICONERROR | MB_OK, mbtitle, msg_batch, algtype, algname);
	return 0;
    }

    mbret = mboxprintf(MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2, mbtitle, msg, algtype, algname);
    if (mbret == IDYES) {
	return 1;
    } else {
	mboxprintf(MB_ICONWARNING | MB_OK, mbtitle, abandoned);
	return 0;
    }
}

/*
 * Ask whether to wipe a session log file before writing to it.
 * Returns 2 for wipe, 1 for append, 0 for cancel (don't log).
 */
int askappend(void *frontend, Filename filename,
	      void (*callback)(void *ctx, int result), void *ctx)
{
    static const char msgtemplate[] =
	"The session log file \"%.*s\" already exists.\n"
	"You can overwrite it with a new session log,\n"
	"append your session log to the end of it,\n"
	"or disable session logging for this session.\n"
	"Hit Yes to wipe the file, No to append to it,\n"
	"or Cancel to disable logging."
	"Wipe the log file? ";

    static const char msgtemplate_batch[] =
	"The session log file \"%.*s\" already exists.\n"
	"Logging will not be enabled.\n";

    static const char mbtitle[] = "PuTTY Log to File";
    int mbret;

    if (console_batch_mode) {
	mboxprintf(MB_ICONERROR | MB_OK, mbtitle, msgtemplate_batch, FILENAME_MAX, filename.path);
	return 0;
    }
    mbret = mboxprintf(MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3, mbtitle, msgtemplate, FILENAME_MAX, filename.path);

    if (mbret == IDYES)
	return 2;
    else if (mbret == IDNO)
	return 1;
    else
	return 0;
}

/*
 * Warn about the obsolescent key file format.
 * 
 * Uniquely among these functions, this one does _not_ expect a
 * frontend handle. This means that if PuTTY is ported to a
 * platform which requires frontend handles, this function will be
 * an anomaly. Fortunately, the problem it addresses will not have
 * been present on that platform, so it can plausibly be
 * implemented as an empty function.
 */
void old_keyfile_warning(void)
{
    static const char message[] =
	"You are loading an SSH-2 private key which has an\n"
	"old version of the file format. This means your key\n"
	"file is not fully tamperproof. Future versions of\n"
	"PuTTY may stop supporting this private key format,\n"
	"so we recommend you convert your key to the new\n"
	"format.\n"
	"\n"
	"Once the key is loaded into PuTTYgen, you can perform\n"
	"this conversion simply by saving it again.\n";

    mboxprintf(MB_OK, "PuTTY Key File Warning", message);
}

/*
 * Display the fingerprints of the PGP Master Keys to the user.
 */
void pgp_fingerprints(void)
{
    fputs("These are the fingerprints of the PuTTY PGP Master Keys. They can\n"
	  "be used to establish a trust path from this executable to another\n"
	  "one. See the manual for more information.\n"
	  "(Note: these fingerprints have nothing to do with SSH!)\n"
	  "\n"
	  "PuTTY Master Key (RSA), 1024-bit:\n"
	  "  " PGP_RSA_MASTER_KEY_FP "\n"
	  "PuTTY Master Key (DSA), 1024-bit:\n"
	  "  " PGP_DSA_MASTER_KEY_FP "\n", stdout);
}

void console_provide_logctx(void *logctx)
{
    console_logctx = logctx;
}

void logevent(void *frontend, const char *string)
{
    log_eventlog(console_logctx, string);
}

static void console_data_untrusted(HANDLE hout, const char *data, int len)
{
    DWORD dummy;
    /* FIXME: control-character filtering */
    WriteFile(hout, data, len, &dummy, NULL);
}

#define IDD_GETLINE                     101
#define IDC_INSTRUCTION                 100
#define IDC_PROMPT_0                    1000
#define IDC_RESULT_0                    2000

#define DIALOG_MERGIN   7
#define DIALOG_WIDTH  200
#define PITCH           7
#define STATIC_HEIGHT   8
#define EDIT_HEIGHT    14
#define BUTTON_WIDTH   50
#define BUTTON_HEIGHT  14

static const char STATIC_CLASS[] = "Static";
static const char EDIT_CLASS[]   = "Edit";
static const char BUTTON_CLASS[] = "Button";

static HWND create_control(HWND dialog, int ctrlID, long style, long exstyle, const char* windowClass, const char* label, RECT* r) {
    HWND window;
    HINSTANCE instance = (HINSTANCE) GetWindowLongPtr(dialog, GWLP_HINSTANCE);
    HFONT font = (HFONT) SendMessage(dialog, WM_GETFONT, 0, 0);
    MapDialogRect(dialog, r);
    window = CreateWindowEx(exstyle, windowClass, label, style, r->left, r->top, r->right - r->left, r->bottom - r->top, dialog, (HMENU) ctrlID, instance, NULL);
    SendMessage(window, WM_SETFONT, (WPARAM) font, 0);
    return window;
}

HWND create_static(HWND dialog, const char* label, RECT r) {
    return create_control(dialog, -1, WS_CHILD | WS_VISIBLE, 0, STATIC_CLASS, label, &r);
}

HWND create_edit(HWND dialog, int ctrlID, prompt_t* prompt, RECT r) {
    HWND window;
    long style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    if (!prompt->echo)
        style |= ES_PASSWORD;
    window = create_control(dialog, ctrlID, style, WS_EX_CLIENTEDGE, EDIT_CLASS, prompt->result, &r);
    SendMessage(window, EM_LIMITTEXT, prompt->resultsize - 1, 0);
    return window;
}

HWND create_button(HWND dialog, int ctrlID, const char* label, RECT r) {
    return create_control(dialog, ctrlID, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, BUTTON_CLASS, label, &r);
}

BOOL CALLBACK getline_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        {
            HINSTANCE instance = (HINSTANCE) GetWindowLongPtr(hwndDlg, GWLP_HINSTANCE);
            prompts_t *p = (prompts_t *) lParam;
            RECT r;
            HFONT font = (HFONT) SendMessage(hwndDlg, WM_GETFONT, 0, 0);
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            SetWindowText(hwndDlg, p->name);
            r.left = 0;
            r.top = 0;
            r.right = DIALOG_WIDTH;
            r.bottom = DIALOG_MERGIN
                + (p->instr_reqd ? STATIC_HEIGHT + PITCH : 0)
                + (STATIC_HEIGHT + PITCH + EDIT_HEIGHT + PITCH) * p->n_prompts
                + BUTTON_HEIGHT
                + DIALOG_MERGIN;
            MapDialogRect(hwndDlg, &r);
            AdjustWindowRectEx(&r, GetWindowLong(hwndDlg, GWL_STYLE), FALSE, GetWindowLong(hwndDlg, GWL_EXSTYLE));
            SetWindowPos(hwndDlg, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOREDRAW);
            r.left = DIALOG_MERGIN;
            r.right = DIALOG_WIDTH - DIALOG_MERGIN;
            r.top = DIALOG_MERGIN;
            if (p->instr_reqd) {
                r.bottom = r.top + STATIC_HEIGHT;
                create_static(hwndDlg, p->instruction, r);
                r.top = r.bottom + PITCH;
            }
            {
                int i;
                for (i = 0; i < (int)p->n_prompts; i++) {
                    r.bottom = r.top + STATIC_HEIGHT;
                    create_static(hwndDlg, p->prompts[i]->prompt, r);
                    r.top = r.bottom + PITCH;
                    
                    r.bottom = r.top + EDIT_HEIGHT;
                    create_edit(hwndDlg, IDC_RESULT_0 + i, p->prompts[i], r);
                    r.top = r.bottom + PITCH;
                }
            }
            {
                r.bottom = r.top + BUTTON_HEIGHT;
                r.left = DIALOG_WIDTH - DIALOG_MERGIN - BUTTON_WIDTH - PITCH - BUTTON_WIDTH;
                r.right = r.left + BUTTON_WIDTH;
                create_button(hwndDlg, IDOK, "OK", r);
                
                r.left = r.right + PITCH;
                r.right = r.left + BUTTON_WIDTH;
                create_button(hwndDlg, IDCANCEL, "Cancel", r);
            }
        }
        return TRUE;
    case WM_COMMAND:
        switch (wParam) {
        case MAKEWPARAM(IDCANCEL, BN_CLICKED):
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        case MAKEWPARAM(IDOK, BN_CLICKED):
            {
                int i;
                prompts_t *p = (prompts_t *) GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
                for (i = 0; i < (int)p->n_prompts; i++) {
                    GetDlgItemText(hwndDlg, IDC_RESULT_0 + i, p->prompts[i]->result, p->prompts[i]->resultsize);
                }
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

int console_get_userpass_input(prompts_t *p, unsigned char *in, int inlen)
{
    /*
     * Zero all the results, in case we abort half-way through.
     */
    {
	int i;
	for (i = 0; i < (int)p->n_prompts; i++)
	    memset(p->prompts[i]->result, 0, p->prompts[i]->resultsize);
    }

    if (console_batch_mode || p->n_prompts < 1 || p->n_prompts > 10)
	return 0;

   if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GETLINE), NULL, getline_dialog_proc, (LPARAM) p) != IDOK)
	return -1;
    
    return 1; /* success */

}

void frontend_keypress(void *handle)
{
    /*
     * This is nothing but a stub, in console code.
     */
    return;
}
