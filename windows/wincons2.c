/*
 * console.c: various interactive-prompt routines shared between
 * the console PuTTY tools
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "putty.h"
#include "storage.h"
#include "ssh.h"

void appendLogF(const char* p, ...);

static void *console_logctx = NULL;

/*
 * Clean up and exit.
 */
void cleanup_exit(int code)
{
    /*
     * Clean up.
     */
    sk_cleanup();

	random_save_seed();
#ifdef MSCRYPTOAPI
	crypto_wrapup();
#endif

    ExitProcess(code);
}

void set_busy_status(void *frontend, int status)
{
}

void notify_remote_exit(void *frontend)
{
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

    /*
     * Verify the key against the registry.
     */
    ret = verify_host_key(host, port, keytype, keystr);

    if (ret == 0)		       /* success - key matched OK */
	return 1;

    if (ret == 2) {		       /* key was different */
        appendLogF(wrongmsg_batch, keytype, fingerprint);
        return 0;
    }
    if (ret == 1) {		       /* key was absent */
        appendLogF(absentmsg_batch, keytype, fingerprint);
        cleanup_exit(1);
    }
    return 1;
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
    static const char msg_batch[] =
        "The first %s supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Connection abandoned.\n";
    appendLogF(msg_batch, algtype, algname);
    return 0;
}

/*
 * Ask whether to wipe a session log file before writing to it.
 * Returns 2 for wipe, 1 for append, 0 for cancel (don't log).
 */
int askappend(void *frontend, Filename filename,
             void (*callback)(void *ctx, int result), void *ctx)
{
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

    appendLogF(message);
}

/*
 * Display the fingerprints of the PGP Master Keys to the user.
 */
void pgp_fingerprints(void)
{
    appendLogF("These are the fingerprints of the PuTTY PGP Master Keys. They can\n"
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
    if (console_logctx)
	log_eventlog(console_logctx, string);
}

void frontend_keypress(void *handle)
{
    /*
     * This is nothing but a stub, in console code.
     */
    return;
}
