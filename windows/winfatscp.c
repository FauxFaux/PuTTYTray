#define FATTY
#include "winsftp.c"

void sftp_ldisc_send(void *handle, char *buf, int len, int interactive);
extern char *(*do_select)(SOCKET skt, int startup);

int winfatsftp(char *cmdline) {
    int argc, ret;
    char **argv;

    int sftp_from_backend(void *frontend, int is_stderr, const char *data, int len);
    int sftp_from_backend_untrusted(void *frontend, const char *data, int len);
    int sftp_from_backend_eof(void *frontend);
    
    void sftp_agent_schedule_callback(void (*callback)(void *, void *, int),
			     void *callback_ctx, void *data, int len);
    
    void cons_notify_remote_exit(void *frontend);
    
    AllocConsole();

    freopen("CONOUT$", "wb", stdout);
    freopen("CONOUT$", "wb", stderr);
    
    platform_get_x11_auth = &sftp_platform_get_x11_auth;
    do_select = &sftp_do_select;

    get_userpass_input = &sftp_get_userpass_input;    
    
    ldisc_send = &sftp_ldisc_send;
    
    from_backend = &sftp_from_backend;
    from_backend_untrusted = &sftp_from_backend_untrusted;
    from_backend_eof = &sftp_from_backend_eof;
    
    agent_schedule_callback = &sftp_agent_schedule_callback;
    
    notify_remote_exit = &cons_notify_remote_exit;
    
    // v--8---v (oh such a nasty)
    // --as-guiscp
    split_into_argv(cmdline + 8, &argc, &argv, NULL);

    ret = psftp_main(argc, argv);
    return ret;
}

#include "wincons.c"


