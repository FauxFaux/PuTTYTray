/*
 * config.c - the platform-independent parts of the PuTTY
 * configuration box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

#define PRINTER_DISABLED_STRING "없음 (프린트 사용 안 함)"

#define HOST_BOX_TITLE "Host Name (or IP address)"
#define PORT_BOX_TITLE "Port"

/*
 * Convenience function: determine whether this binary supports a
 * given backend.
 */
static int have_backend(int protocol)
{
    struct backend_list *p = backends;
    for (p = backends; p->name; p++) {
	if (p->protocol == protocol)
	    return 1;
    }
    return 0;
}

static void config_host_handler(union control *ctrl, void *dlg,
				void *data, int event)
{
    Config *cfg = (Config *)data;

    /*
     * This function works just like the standard edit box handler,
     * only it has to choose the control's label and text from two
     * different places depending on the protocol.
     */
    if (event == EVENT_REFRESH) {
	if (cfg->protocol == PROT_SERIAL) {
	    /*
	     * This label text is carefully chosen to contain an n,
	     * since that's the shortcut for the host name control.
	     */
	    dlg_label_change(ctrl, dlg, "Serial line");
	    dlg_editbox_set(ctrl, dlg, cfg->serline);
	} else {
	    dlg_label_change(ctrl, dlg, HOST_BOX_TITLE);
	    dlg_editbox_set(ctrl, dlg, cfg->host);
	}
    } else if (event == EVENT_VALCHANGE) {
	if (cfg->protocol == PROT_SERIAL)
	    dlg_editbox_get(ctrl, dlg, cfg->serline, lenof(cfg->serline));
	else
	    dlg_editbox_get(ctrl, dlg, cfg->host, lenof(cfg->host));
    }
}

static void config_port_handler(union control *ctrl, void *dlg,
				void *data, int event)
{
    Config *cfg = (Config *)data;
    char buf[80];

    /*
     * This function works just like the standard edit box handler,
     * only it has to choose the control's label and text from two
     * different places depending on the protocol.
     */
    if (event == EVENT_REFRESH) {
	if (cfg->protocol == PROT_SERIAL) {
	    /*
	     * This label text is carefully chosen to contain a p,
	     * since that's the shortcut for the port control.
	     */
	    dlg_label_change(ctrl, dlg, "Speed");
	    sprintf(buf, "%d", cfg->serspeed);
	} else {
	    dlg_label_change(ctrl, dlg, PORT_BOX_TITLE);
	    sprintf(buf, "%d", cfg->port);
	}
	dlg_editbox_set(ctrl, dlg, buf);
    } else if (event == EVENT_VALCHANGE) {
	dlg_editbox_get(ctrl, dlg, buf, lenof(buf));
	if (cfg->protocol == PROT_SERIAL)
	    cfg->serspeed = atoi(buf);
	else
	    cfg->port = atoi(buf);
    }
}

struct hostport {
    union control *host, *port;
};

/*
 * We export this function so that platform-specific config
 * routines can use it to conveniently identify the protocol radio
 * buttons in order to add to them.
 */
void config_protocolbuttons_handler(union control *ctrl, void *dlg,
				    void *data, int event)
{
    int button, defport;
    Config *cfg = (Config *)data;
    struct hostport *hp = (struct hostport *)ctrl->radio.context.p;

    /*
     * This function works just like the standard radio-button
     * handler, except that it also has to change the setting of
     * the port box, and refresh both host and port boxes when. We
     * expect the context parameter to point at a hostport
     * structure giving the `union control's for both.
     */
    if (event == EVENT_REFRESH) {
	for (button = 0; button < ctrl->radio.nbuttons; button++)
	    if (cfg->protocol == ctrl->radio.buttondata[button].i)
		break;
	/* We expected that `break' to happen, in all circumstances. */
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	int oldproto = cfg->protocol;
	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	cfg->protocol = ctrl->radio.buttondata[button].i;
	if (oldproto != cfg->protocol) {
	    defport = -1;
	    switch (cfg->protocol) {
	      case PROT_SSH: defport = 22; break;
	      case PROT_TELNET: defport = 23; break;
	      case PROT_RLOGIN: defport = 513; break;
	    }
	    if (defport > 0 && cfg->port != defport) {
		cfg->port = defport;
	    }
	}
	dlg_refresh(hp->host, dlg);
	dlg_refresh(hp->port, dlg);
    }
}

static void loggingbuttons_handler(union control *ctrl, void *dlg,
				   void *data, int event)
{
    int button;
    Config *cfg = (Config *)data;
    /* This function works just like the standard radio-button handler,
     * but it has to fall back to "no logging" in situations where the
     * configured logging type isn't applicable.
     */
    if (event == EVENT_REFRESH) {
        for (button = 0; button < ctrl->radio.nbuttons; button++)
            if (cfg->logtype == ctrl->radio.buttondata[button].i)
	        break;
    
    /* We fell off the end, so we lack the configured logging type */
    if (button == ctrl->radio.nbuttons) {
        button=0;
        cfg->logtype=LGTYP_NONE;
    }
    dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
        button = dlg_radiobutton_get(ctrl, dlg);
        assert(button >= 0 && button < ctrl->radio.nbuttons);
        cfg->logtype = ctrl->radio.buttondata[button].i;
    }
}

static void numeric_keypad_handler(union control *ctrl, void *dlg,
				   void *data, int event)
{
    int button;
    Config *cfg = (Config *)data;
    /*
     * This function works much like the standard radio button
     * handler, but it has to handle two fields in Config.
     */
    if (event == EVENT_REFRESH) {
	if (cfg->nethack_keypad)
	    button = 2;
	else if (cfg->app_keypad)
	    button = 1;
	else
	    button = 0;
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	if (button == 2) {
	    cfg->app_keypad = FALSE;
	    cfg->nethack_keypad = TRUE;
	} else {
	    cfg->app_keypad = (button != 0);
	    cfg->nethack_keypad = FALSE;
	}
    }
}

static void cipherlist_handler(union control *ctrl, void *dlg,
			       void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int i;

	static const struct { char *s; int c; } ciphers[] = {
	    { "3DES",			CIPHER_3DES },
	    { "Blowfish",		CIPHER_BLOWFISH },
	    { "DES",			CIPHER_DES },
	    { "AES (SSH2 만)",	CIPHER_AES },
	    { "Arc4 (SSH2 만)",	CIPHER_ARCFOUR },
	    { "-- 이하는 위험함 --",	CIPHER_WARN }
	};

	/* Set up the "selected ciphers" box. */
	/* (cipherlist assumed to contain all ciphers) */
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < CIPHER_MAX; i++) {
	    int c = cfg->ssh_cipherlist[i];
	    int j;
	    char *cstr = NULL;
	    for (j = 0; j < (sizeof ciphers) / (sizeof ciphers[0]); j++) {
		if (ciphers[j].c == c) {
		    cstr = ciphers[j].s;
		    break;
		}
	    }
	    dlg_listbox_addwithid(ctrl, dlg, cstr, c);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < CIPHER_MAX; i++)
	    cfg->ssh_cipherlist[i] = dlg_listbox_getid(ctrl, dlg, i);

    }
}

static void kexlist_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int i;

	static const struct { char *s; int k; } kexes[] = {
	    { "디피-헬만 그룹 1",		KEX_DHGROUP1 },
	    { "디피-헬만 그룹 14",	KEX_DHGROUP14 },
	    { "디피-헬만 그룹 교환",	KEX_DHGEX },
	    { "-- 이하는 위험함 --",		KEX_WARN }
	};

	/* Set up the "kex preference" box. */
	/* (kexlist assumed to contain all algorithms) */
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < KEX_MAX; i++) {
	    int k = cfg->ssh_kexlist[i];
	    int j;
	    char *kstr = NULL;
	    for (j = 0; j < (sizeof kexes) / (sizeof kexes[0]); j++) {
		if (kexes[j].k == k) {
		    kstr = kexes[j].s;
		    break;
		}
	    }
	    dlg_listbox_addwithid(ctrl, dlg, kstr, k);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < KEX_MAX; i++)
	    cfg->ssh_kexlist[i] = dlg_listbox_getid(ctrl, dlg, i);

    }
}

static void printerbox_handler(union control *ctrl, void *dlg,
			       void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int nprinters, i;
	printer_enum *pe;

	dlg_update_start(ctrl, dlg);
	/*
	 * Some backends may wish to disable the drop-down list on
	 * this edit box. Be prepared for this.
	 */
	if (ctrl->editbox.has_list) {
	    dlg_listbox_clear(ctrl, dlg);
	    dlg_listbox_add(ctrl, dlg, PRINTER_DISABLED_STRING);
	    pe = printer_start_enum(&nprinters);
	    for (i = 0; i < nprinters; i++)
		dlg_listbox_add(ctrl, dlg, printer_get_name(pe, i));
	    printer_finish_enum(pe);
	}
	dlg_editbox_set(ctrl, dlg,
			(*cfg->printer ? cfg->printer :
			 PRINTER_DISABLED_STRING));
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_VALCHANGE) {
	dlg_editbox_get(ctrl, dlg, cfg->printer, sizeof(cfg->printer));
	if (!strcmp(cfg->printer, PRINTER_DISABLED_STRING))
	    *cfg->printer = '\0';
    }
}

static void codepage_handler(union control *ctrl, void *dlg,
			     void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int i;
	const char *cp;
	dlg_update_start(ctrl, dlg);
	strcpy(cfg->line_codepage,
	       cp_name(decode_codepage(cfg->line_codepage)));
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; (cp = cp_enumerate(i)) != NULL; i++)
	    dlg_listbox_add(ctrl, dlg, cp);
	dlg_editbox_set(ctrl, dlg, cfg->line_codepage);
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_VALCHANGE) {
	dlg_editbox_get(ctrl, dlg, cfg->line_codepage,
			sizeof(cfg->line_codepage));
	strcpy(cfg->line_codepage,
	       cp_name(decode_codepage(cfg->line_codepage)));
    }
}

static void sshbug_handler(union control *ctrl, void *dlg,
			   void *data, int event)
{
    if (event == EVENT_REFRESH) {
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	dlg_listbox_addwithid(ctrl, dlg, "자동", AUTO);
	dlg_listbox_addwithid(ctrl, dlg, "끔", FORCE_OFF);
	dlg_listbox_addwithid(ctrl, dlg, "켬", FORCE_ON);
	switch (*(int *)ATOFFSET(data, ctrl->listbox.context.i)) {
	  case AUTO:      dlg_listbox_select(ctrl, dlg, 0); break;
	  case FORCE_OFF: dlg_listbox_select(ctrl, dlg, 1); break;
	  case FORCE_ON:  dlg_listbox_select(ctrl, dlg, 2); break;
	}
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_SELCHANGE) {
	int i = dlg_listbox_index(ctrl, dlg);
	if (i < 0)
	    i = AUTO;
	else
	    i = dlg_listbox_getid(ctrl, dlg, i);
	*(int *)ATOFFSET(data, ctrl->listbox.context.i) = i;
    }
}

#define SAVEDSESSION_LEN 2048

struct sessionsaver_data {
    union control *editbox, *listbox, *loadbutton, *savebutton, *delbutton;
    union control *okbutton, *cancelbutton;
    struct sesslist sesslist;
    int midsession;
};

/* 
 * Helper function to load the session selected in the list box, if
 * any, as this is done in more than one place below. Returns 0 for
 * failure.
 */
static int load_selected_session(struct sessionsaver_data *ssd,
				 char *savedsession,
				 void *dlg, Config *cfg, int *maybe_launch)
{
    int i = dlg_listbox_index(ssd->listbox, dlg);
    int isdef;
    if (i < 0) {
	dlg_beep(dlg);
	return 0;
    }
    isdef = !strcmp(ssd->sesslist.sessions[i], "기본 설정");
    load_settings(ssd->sesslist.sessions[i], cfg);
    if (!isdef) {
	strncpy(savedsession, ssd->sesslist.sessions[i],
		SAVEDSESSION_LEN);
	savedsession[SAVEDSESSION_LEN-1] = '\0';
	if (maybe_launch)
	    *maybe_launch = TRUE;
    } else {
	savedsession[0] = '\0';
	if (maybe_launch)
	    *maybe_launch = FALSE;
    }
    dlg_refresh(NULL, dlg);
    /* Restore the selection, which might have been clobbered by
     * changing the value of the edit box. */
    dlg_listbox_select(ssd->listbox, dlg, i);
    return 1;
}

static void sessionsaver_handler(union control *ctrl, void *dlg,
				 void *data, int event)
{
    Config *cfg = (Config *)data;
    struct sessionsaver_data *ssd =
	(struct sessionsaver_data *)ctrl->generic.context.p;
    char *savedsession;

    /*
     * The first time we're called in a new dialog, we must
     * allocate space to store the current contents of the saved
     * session edit box (since it must persist even when we switch
     * panels, but is not part of the Config).
     */
    if (!ssd->editbox) {
        savedsession = NULL;
    } else if (!dlg_get_privdata(ssd->editbox, dlg)) {
	savedsession = (char *)
	    dlg_alloc_privdata(ssd->editbox, dlg, SAVEDSESSION_LEN);
	savedsession[0] = '\0';
    } else {
	savedsession = dlg_get_privdata(ssd->editbox, dlg);
    }

    if (event == EVENT_REFRESH) {
	if (ctrl == ssd->editbox) {
	    dlg_editbox_set(ctrl, dlg, savedsession);
	} else if (ctrl == ssd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < ssd->sesslist.nsessions; i++)
		dlg_listbox_add(ctrl, dlg, ssd->sesslist.sessions[i]);
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_VALCHANGE) {
        int top, bottom, halfway, i;
	if (ctrl == ssd->editbox) {
	    dlg_editbox_get(ctrl, dlg, savedsession,
			    SAVEDSESSION_LEN);
	    top = ssd->sesslist.nsessions;
	    bottom = -1;
	    while (top-bottom > 1) {
	        halfway = (top+bottom)/2;
	        i = strcmp(savedsession, ssd->sesslist.sessions[halfway]);
	        if (i <= 0 ) {
		    top = halfway;
	        } else {
		    bottom = halfway;
	        }
	    }
	    if (top == ssd->sesslist.nsessions) {
	        top -= 1;
	    }
	    dlg_listbox_select(ssd->listbox, dlg, top);
	}
    } else if (event == EVENT_ACTION) {
	int mbl = FALSE;
	if (!ssd->midsession &&
	    (ctrl == ssd->listbox ||
	     (ssd->loadbutton && ctrl == ssd->loadbutton))) {
	    /*
	     * The user has double-clicked a session, or hit Load.
	     * We must load the selected session, and then
	     * terminate the configuration dialog _if_ there was a
	     * double-click on the list box _and_ that session
	     * contains a hostname.
	     */
	    if (load_selected_session(ssd, savedsession, dlg, cfg, &mbl) &&
		(mbl && ctrl == ssd->listbox && cfg_launchable(cfg))) {
		dlg_end(dlg, 1);       /* it's all over, and succeeded */
	    }
	} else if (ctrl == ssd->savebutton) {
	    int isdef = !strcmp(savedsession, "기본 설정");
	    if (!savedsession[0]) {
		int i = dlg_listbox_index(ssd->listbox, dlg);
		if (i < 0) {
		    dlg_beep(dlg);
		    return;
		}
		isdef = !strcmp(ssd->sesslist.sessions[i], "기본 설정");
		if (!isdef) {
		    strncpy(savedsession, ssd->sesslist.sessions[i],
			    SAVEDSESSION_LEN);
		    savedsession[SAVEDSESSION_LEN-1] = '\0';
		} else {
		    savedsession[0] = '\0';
		}
	    }
            {
                char *errmsg = save_settings(savedsession, cfg);
                if (errmsg) {
                    dlg_error_msg(dlg, errmsg);
                    sfree(errmsg);
                }
            }
	    get_sesslist(&ssd->sesslist, FALSE);
	    get_sesslist(&ssd->sesslist, TRUE);
	    dlg_refresh(ssd->editbox, dlg);
	    dlg_refresh(ssd->listbox, dlg);
	} else if (!ssd->midsession &&
		   ssd->delbutton && ctrl == ssd->delbutton) {
	    int i = dlg_listbox_index(ssd->listbox, dlg);
	    if (i <= 0) {
		dlg_beep(dlg);
	    } else {
		del_settings(ssd->sesslist.sessions[i]);
		get_sesslist(&ssd->sesslist, FALSE);
		get_sesslist(&ssd->sesslist, TRUE);
		dlg_refresh(ssd->listbox, dlg);
	    }
	} else if (ctrl == ssd->okbutton) {
            if (ssd->midsession) {
                /* In a mid-session Change Settings, Apply is always OK. */
		dlg_end(dlg, 1);
                return;
            }
	    /*
	     * Annoying special case. If the `Open' button is
	     * pressed while no host name is currently set, _and_
	     * the session list previously had the focus, _and_
	     * there was a session selected in that which had a
	     * valid host name in it, then load it and go.
	     */
	    if (dlg_last_focused(ctrl, dlg) == ssd->listbox &&
		!cfg_launchable(cfg)) {
		Config cfg2;
		int mbl = FALSE;
		if (!load_selected_session(ssd, savedsession, dlg,
					   &cfg2, &mbl)) {
		    dlg_beep(dlg);
		    return;
		}
		/* If at this point we have a valid session, go! */
		if (mbl && cfg_launchable(&cfg2)) {
		    *cfg = cfg2;       /* structure copy */
		    cfg->remote_cmd_ptr = NULL;
		    dlg_end(dlg, 1);
		} else
		    dlg_beep(dlg);
                return;
	    }

	    /*
	     * Otherwise, do the normal thing: if we have a valid
	     * session, get going.
	     */
	    if (cfg_launchable(cfg)) {
		dlg_end(dlg, 1);
	    } else
		dlg_beep(dlg);
	} else if (ctrl == ssd->cancelbutton) {
	    dlg_end(dlg, 0);
	}
    }
}

struct charclass_data {
    union control *listbox, *editbox, *button;
};

static void charclass_handler(union control *ctrl, void *dlg,
			      void *data, int event)
{
    Config *cfg = (Config *)data;
    struct charclass_data *ccd =
	(struct charclass_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == ccd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < 128; i++) {
		char str[100];
		sprintf(str, "%d\t(0x%02X)\t%c\t%d", i, i,
			(i >= 0x21 && i != 0x7F) ? i : ' ', cfg->wordness[i]);
		dlg_listbox_add(ctrl, dlg, str);
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == ccd->button) {
	    char str[100];
	    int i, n;
	    dlg_editbox_get(ccd->editbox, dlg, str, sizeof(str));
	    n = atoi(str);
	    for (i = 0; i < 128; i++) {
		if (dlg_listbox_issel(ccd->listbox, dlg, i))
		    cfg->wordness[i] = n;
	    }
	    dlg_refresh(ccd->listbox, dlg);
	}
    }
}

struct colour_data {
    union control *listbox, *redit, *gedit, *bedit, *button;
};

static const char *const colours[] = {
    "기본 글자색", "기본 글자색(굵음)",
    "기본 배경색", "기본 배경색(굵음)",
    "커서 글자", "커서 색깔",
    "ANSI 검은색", "ANSI 검은색(굵음)",
    "ANSI 빨강", "ANSI 빨강(굵음)",
    "ANSI 녹색", "ANSI 녹색(굵음)",
    "ANSI 노랑", "ANSI 노랑(굵음)",
    "ANSI 파랑", "ANSI 파랑(굵음)",
    "ANSI 마젠타", "ANSI 마젠타(굵음)",
    "ANSI 하늘색", "ANSI 하늘색(굵음)",
    "ANSI 하양", "ANSI 하양(굵음)"
};

static void colour_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    struct colour_data *cd =
	(struct colour_data *)ctrl->generic.context.p;
    int update = FALSE, r, g, b;

    if (event == EVENT_REFRESH) {
	if (ctrl == cd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < lenof(colours); i++)
		dlg_listbox_add(ctrl, dlg, colours[i]);
	    dlg_update_done(ctrl, dlg);
	    dlg_editbox_set(cd->redit, dlg, "");
	    dlg_editbox_set(cd->gedit, dlg, "");
	    dlg_editbox_set(cd->bedit, dlg, "");
	}
    } else if (event == EVENT_SELCHANGE) {
	if (ctrl == cd->listbox) {
	    /* The user has selected a colour. Update the RGB text. */
	    int i = dlg_listbox_index(ctrl, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
		return;
	    }
	    r = cfg->colours[i][0];
	    g = cfg->colours[i][1];
	    b = cfg->colours[i][2];
	    update = TRUE;
	}
    } else if (event == EVENT_VALCHANGE) {
	if (ctrl == cd->redit || ctrl == cd->gedit || ctrl == cd->bedit) {
	    /* The user has changed the colour using the edit boxes. */
	    char buf[80];
	    int i, cval;

	    dlg_editbox_get(ctrl, dlg, buf, lenof(buf));
	    cval = atoi(buf);
	    if (cval > 255) cval = 255;
	    if (cval < 0)   cval = 0;

	    i = dlg_listbox_index(cd->listbox, dlg);
	    if (i >= 0) {
		if (ctrl == cd->redit)
		    cfg->colours[i][0] = cval;
		else if (ctrl == cd->gedit)
		    cfg->colours[i][1] = cval;
		else if (ctrl == cd->bedit)
		    cfg->colours[i][2] = cval;
	    }
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == cd->button) {
	    int i = dlg_listbox_index(cd->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
		return;
	    }
	    /*
	     * Start a colour selector, which will send us an
	     * EVENT_CALLBACK when it's finished and allow us to
	     * pick up the results.
	     */
	    dlg_coloursel_start(ctrl, dlg,
				cfg->colours[i][0],
				cfg->colours[i][1],
				cfg->colours[i][2]);
	}
    } else if (event == EVENT_CALLBACK) {
	if (ctrl == cd->button) {
	    int i = dlg_listbox_index(cd->listbox, dlg);
	    /*
	     * Collect the results of the colour selector. Will
	     * return nonzero on success, or zero if the colour
	     * selector did nothing (user hit Cancel, for example).
	     */
	    if (dlg_coloursel_results(ctrl, dlg, &r, &g, &b)) {
		cfg->colours[i][0] = r;
		cfg->colours[i][1] = g;
		cfg->colours[i][2] = b;
		update = TRUE;
	    }
	}
    }

    if (update) {
	char buf[40];
	sprintf(buf, "%d", r); dlg_editbox_set(cd->redit, dlg, buf);
	sprintf(buf, "%d", g); dlg_editbox_set(cd->gedit, dlg, buf);
	sprintf(buf, "%d", b); dlg_editbox_set(cd->bedit, dlg, buf);
    }
}

struct ttymodes_data {
    union control *modelist, *valradio, *valbox;
    union control *addbutton, *rembutton, *listbox;
};

static void ttymodes_handler(union control *ctrl, void *dlg,
			     void *data, int event)
{
    Config *cfg = (Config *)data;
    struct ttymodes_data *td =
	(struct ttymodes_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == td->listbox) {
	    char *p = cfg->ttymodes;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    while (*p) {
		int tabpos = strchr(p, '\t') - p;
		char *disp = dupprintf("%.*s\t%s", tabpos, p,
				       (p[tabpos+1] == 'A') ? "자동" :
				       p+tabpos+2);
		dlg_listbox_add(ctrl, dlg, disp);
		p += strlen(p) + 1;
		sfree(disp);
	    }
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == td->modelist) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; ttymodes[i]; i++)
		dlg_listbox_add(ctrl, dlg, ttymodes[i]);
	    dlg_listbox_select(ctrl, dlg, 0); /* *shrug* */
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == td->valradio) {
	    dlg_radiobutton_set(ctrl, dlg, 0);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == td->addbutton) {
	    int ind = dlg_listbox_index(td->modelist, dlg);
	    if (ind >= 0) {
		char type = dlg_radiobutton_get(td->valradio, dlg) ? 'V' : 'A';
		int slen, left;
		char *p, str[lenof(cfg->ttymodes)];
		/* Construct new entry */
		memset(str, 0, lenof(str));
		strncpy(str, ttymodes[ind], lenof(str)-3);
		slen = strlen(str);
		str[slen] = '\t';
		str[slen+1] = type;
		slen += 2;
		if (type == 'V') {
		    dlg_editbox_get(td->valbox, dlg, str+slen, lenof(str)-slen);
		}
		/* Find end of list, deleting any existing instance */
		p = cfg->ttymodes;
		left = lenof(cfg->ttymodes);
		while (*p) {
		    int t = strchr(p, '\t') - p;
		    if (t == strlen(ttymodes[ind]) &&
			strncmp(p, ttymodes[ind], t) == 0) {
			memmove(p, p+strlen(p)+1, left - (strlen(p)+1));
			continue;
		    }
		    left -= strlen(p) + 1;
		    p    += strlen(p) + 1;
		}
		/* Append new entry */
		memset(p, 0, left);
		strncpy(p, str, left - 2);
		dlg_refresh(td->listbox, dlg);
	    } else
		dlg_beep(dlg);
	} else if (ctrl == td->rembutton) {
	    char *p = cfg->ttymodes;
	    int i = 0, len = lenof(cfg->ttymodes);
	    while (*p) {
		int multisel = dlg_listbox_index(td->listbox, dlg) < 0;
		if (dlg_listbox_issel(td->listbox, dlg, i)) {
		    if (!multisel) {
			/* Populate controls with entry we're about to
			 * delete, for ease of editing.
			 * (If multiple entries were selected, don't
			 * touch the controls.) */
			char *val = strchr(p, '\t');
			if (val) {
			    int ind = 0;
			    val++;
			    while (ttymodes[ind]) {
				if (strlen(ttymodes[ind]) == val-p-1 &&
				    !strncmp(ttymodes[ind], p, val-p-1))
				    break;
				ind++;
			    }
			    dlg_listbox_select(td->modelist, dlg, ind);
			    dlg_radiobutton_set(td->valradio, dlg,
						(*val == 'V'));
			    dlg_editbox_set(td->valbox, dlg, val+1);
			}
		    }
		    memmove(p, p+strlen(p)+1, len - (strlen(p)+1));
		    i++;
		    continue;
		}
		len -= strlen(p) + 1;
		p   += strlen(p) + 1;
		i++;
	    }
	    memset(p, 0, lenof(cfg->ttymodes) - len);
	    dlg_refresh(td->listbox, dlg);
	}
    }
}

struct environ_data {
    union control *varbox, *valbox, *addbutton, *rembutton, *listbox;
};

static void environ_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    struct environ_data *ed =
	(struct environ_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == ed->listbox) {
	    char *p = cfg->environmt;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    while (*p) {
		dlg_listbox_add(ctrl, dlg, p);
		p += strlen(p) + 1;
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == ed->addbutton) {
	    char str[sizeof(cfg->environmt)];
	    char *p;
	    dlg_editbox_get(ed->varbox, dlg, str, sizeof(str)-1);
	    if (!*str) {
		dlg_beep(dlg);
		return;
	    }
	    p = str + strlen(str);
	    *p++ = '\t';
	    dlg_editbox_get(ed->valbox, dlg, p, sizeof(str)-1 - (p - str));
	    if (!*p) {
		dlg_beep(dlg);
		return;
	    }
	    p = cfg->environmt;
	    while (*p) {
		while (*p)
		    p++;
		p++;
	    }
	    if ((p - cfg->environmt) + strlen(str) + 2 <
		sizeof(cfg->environmt)) {
		strcpy(p, str);
		p[strlen(str) + 1] = '\0';
		dlg_listbox_add(ed->listbox, dlg, str);
		dlg_editbox_set(ed->varbox, dlg, "");
		dlg_editbox_set(ed->valbox, dlg, "");
	    } else {
		dlg_error_msg(dlg, "환경 변수가 너무 큽니다");
	    }
	} else if (ctrl == ed->rembutton) {
	    int i = dlg_listbox_index(ed->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
	    } else {
		char *p, *q, *str;

		dlg_listbox_del(ed->listbox, dlg, i);
		p = cfg->environmt;
		while (i > 0) {
		    if (!*p)
			goto disaster;
		    while (*p)
			p++;
		    p++;
		    i--;
		}
		q = p;
		if (!*p)
		    goto disaster;
		/* Populate controls with the entry we're about to delete
		 * for ease of editing */
		str = p;
		p = strchr(p, '\t');
		if (!p)
		    goto disaster;
		*p = '\0';
		dlg_editbox_set(ed->varbox, dlg, str);
		p++;
		str = p;
		dlg_editbox_set(ed->valbox, dlg, str);
		p = strchr(p, '\0');
		if (!p)
		    goto disaster;
		p++;
		while (*p) {
		    while (*p)
			*q++ = *p++;
		    *q++ = *p++;
		}
		*q = '\0';
		disaster:;
	    }
	}
    }
}

struct portfwd_data {
    union control *addbutton, *rembutton, *listbox;
    union control *sourcebox, *destbox, *direction;
#ifndef NO_IPV6
    union control *addressfamily;
#endif
};

static void portfwd_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    struct portfwd_data *pfd =
	(struct portfwd_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == pfd->listbox) {
	    char *p = cfg->portfwd;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    while (*p) {
		dlg_listbox_add(ctrl, dlg, p);
		p += strlen(p) + 1;
	    }
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == pfd->direction) {
	    /*
	     * Default is Local.
	     */
	    dlg_radiobutton_set(ctrl, dlg, 0);
#ifndef NO_IPV6
	} else if (ctrl == pfd->addressfamily) {
	    dlg_radiobutton_set(ctrl, dlg, 0);
#endif
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == pfd->addbutton) {
	    char str[sizeof(cfg->portfwd)];
	    char *p;
	    int i, type;
	    int whichbutton;

	    i = 0;
#ifndef NO_IPV6
	    whichbutton = dlg_radiobutton_get(pfd->addressfamily, dlg);
	    if (whichbutton == 1)
		str[i++] = '4';
	    else if (whichbutton == 2)
		str[i++] = '6';
#endif

	    whichbutton = dlg_radiobutton_get(pfd->direction, dlg);
	    if (whichbutton == 0)
		type = 'L';
	    else if (whichbutton == 1)
		type = 'R';
	    else
		type = 'D';
	    str[i++] = type;

	    dlg_editbox_get(pfd->sourcebox, dlg, str+i, sizeof(str) - i);
	    if (!str[i]) {
		dlg_error_msg(dlg, "원본 포트 번호를 적으셔야 합니다");
		return;
	    }
	    p = str + strlen(str);
	    if (type != 'D') {
		*p++ = '\t';
		dlg_editbox_get(pfd->destbox, dlg, p,
				sizeof(str) - (p - str));
		if (!*p || !strchr(p, ':')) {
		    dlg_error_msg(dlg,
				  "대상 주소는 다음 형식으로 적으셔야 합니다:\n\"호스트.이름:포트\"");
		    return;
		}
	    } else
		*p = '\0';
	    p = cfg->portfwd;
	    while (*p) {
		while (*p)
		    p++;
		p++;
	    }
	    if ((p - cfg->portfwd) + strlen(str) + 2 <=
		sizeof(cfg->portfwd)) {
		strcpy(p, str);
		p[strlen(str) + 1] = '\0';
		dlg_listbox_add(pfd->listbox, dlg, str);
		dlg_editbox_set(pfd->sourcebox, dlg, "");
		dlg_editbox_set(pfd->destbox, dlg, "");
	    } else {
		dlg_error_msg(dlg, "포워딩이 너무 많습니다");
	    }
	} else if (ctrl == pfd->rembutton) {
	    int i = dlg_listbox_index(pfd->listbox, dlg);
	    if (i < 0)
		dlg_beep(dlg);
	    else {
		char *p, *q, *src, *dst;
		char dir;

		dlg_listbox_del(pfd->listbox, dlg, i);
		p = cfg->portfwd;
		while (i > 0) {
		    if (!*p)
			goto disaster2;
		    while (*p)
			p++;
		    p++;
		    i--;
		}
		q = p;
		if (!*p)
		    goto disaster2;
		/* Populate the controls with the entry we're about to
		 * delete, for ease of editing. */
		{
		    static const char *const afs = "A46";
		    char *afp = strchr(afs, *p);
		    int idx = afp ? afp-afs : 0;
		    if (afp)
			p++;
#ifndef NO_IPV6
		    dlg_radiobutton_set(pfd->addressfamily, dlg, idx);
#endif
		}
		{
		    static const char *const dirs = "LRD";
		    dir = *p;
		    dlg_radiobutton_set(pfd->direction, dlg,
					strchr(dirs, dir) - dirs);
		}
		p++;
		if (dir != 'D') {
		    src = p;
		    p = strchr(p, '\t');
		    if (!p)
			goto disaster2;
		    *p = '\0';
		    p++;
		    dst = p;
		} else {
		    src = p;
		    dst = "";
		}
		p = strchr(p, '\0');
		if (!p)
		    goto disaster2;
		dlg_editbox_set(pfd->sourcebox, dlg, src);
		dlg_editbox_set(pfd->destbox, dlg, dst);
		p++;
		while (*p) {
		    while (*p)
			*q++ = *p++;
		    *q++ = *p++;
		}
		*q = '\0';
		disaster2:;
	    }
	}
    }
}

void setup_config_box(struct controlbox *b, int midsession,
		      int protocol, int protcfginfo)
{
    struct controlset *s;
    struct sessionsaver_data *ssd;
    struct charclass_data *ccd;
    struct colour_data *cd;
    struct ttymodes_data *td;
    struct environ_data *ed;
    struct portfwd_data *pfd;
    union control *c;
    char *str;

    ssd = (struct sessionsaver_data *)
	ctrl_alloc(b, sizeof(struct sessionsaver_data));
    memset(ssd, 0, sizeof(*ssd));
    ssd->midsession = midsession;

    /*
     * The standard panel that appears at the bottom of all panels:
     * Open, Cancel, Apply etc.
     */
    s = ctrl_getset(b, "", "", "");
    ctrl_columns(s, 5, 20, 20, 20, 20, 20);
    ssd->okbutton = ctrl_pushbutton(s,
				    (midsession ? "적용 (A)" : "열기 (O)"),
				    (char)(midsession ? 'a' : 'o'),
				    HELPCTX(no_help),
				    sessionsaver_handler, P(ssd));
    ssd->okbutton->button.isdefault = TRUE;
    ssd->okbutton->generic.column = 3;
    ssd->cancelbutton = ctrl_pushbutton(s, "취소 (C)", 'c', HELPCTX(no_help),
					sessionsaver_handler, P(ssd));
    ssd->cancelbutton->button.iscancel = TRUE;
    ssd->cancelbutton->generic.column = 4;
    /* We carefully don't close the 5-column part, so that platform-
     * specific add-ons can put extra buttons alongside Open and Cancel. */

    /*
     * The Session panel.
     */
    str = dupprintf("%s 세션 기본 옵션", appname);
    ctrl_settitle(b, "세션", str);
    sfree(str);

    if (!midsession) {
	struct hostport *hp = (struct hostport *)
	    ctrl_alloc(b, sizeof(struct hostport));

	s = ctrl_getset(b, "세션", "hostport",
			"접속 대상 정보");
	ctrl_columns(s, 2, 75, 25);
	c = ctrl_editbox(s, HOST_BOX_TITLE, 'n', 100,
			 HELPCTX(session_hostname),
			 config_host_handler, I(0), I(0));
	c->generic.column = 0;
	hp->host = c;
	c = ctrl_editbox(s, PORT_BOX_TITLE, 'p', 100,
			 HELPCTX(session_hostname),
			 config_port_handler, I(0), I(0));
	c->generic.column = 1;
	hp->port = c;
	ctrl_columns(s, 1, 100);

	if (!have_backend(PROT_SSH)) {
	    ctrl_radiobuttons(s, "접속 형식:", NO_SHORTCUT, 3,
			      HELPCTX(session_hostname),
			      config_protocolbuttons_handler, P(hp),
			      "생짜 (R)", 'r', I(PROT_RAW),
			      "Telnet", 't', I(PROT_TELNET),
			      "Rlogin", 'i', I(PROT_RLOGIN),
			      NULL);
	} else {
	    ctrl_radiobuttons(s, "접속 형식:", NO_SHORTCUT, 4,
			      HELPCTX(session_hostname),
			      config_protocolbuttons_handler, P(hp),
			      "생짜 (R)", 'r', I(PROT_RAW),
			      "Telnet", 't', I(PROT_TELNET),
			      "Rlogin", 'i', I(PROT_RLOGIN),
			      "SSH", 's', I(PROT_SSH),
			      NULL);
	}
    }

    /*
     * The Load/Save panel is available even in mid-session.
     */
    s = ctrl_getset(b, "세션", "savedsessions",
		    midsession ? "현재 세션 설정 저장" :
		    "저장된 세션의 불러오기, 저장, 지움");
    ctrl_columns(s, 2, 75, 25);
    get_sesslist(&ssd->sesslist, TRUE);
    ssd->editbox = ctrl_editbox(s, "저장된 세션 (E)", 'e', 100,
				HELPCTX(session_saved),
				sessionsaver_handler, P(ssd), P(NULL));
    ssd->editbox->generic.column = 0;
    /* Reset columns so that the buttons are alongside the list, rather
     * than alongside that edit box. */
    ctrl_columns(s, 1, 100);
    ctrl_columns(s, 2, 75, 25);
    ssd->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				HELPCTX(session_saved),
				sessionsaver_handler, P(ssd));
    ssd->listbox->generic.column = 0;
    ssd->listbox->listbox.height = 7;
    if (!midsession) {
	ssd->loadbutton = ctrl_pushbutton(s, "불러옴(L)", 'l',
					  HELPCTX(session_saved),
					  sessionsaver_handler, P(ssd));
	ssd->loadbutton->generic.column = 1;
    } else {
	/* We can't offer the Load button mid-session, as it would allow the
	 * user to load and subsequently save settings they can't see. (And
	 * also change otherwise immutable settings underfoot; that probably
	 * shouldn't be a problem, but.) */
	ssd->loadbutton = NULL;
    }
    /* "Save" button is permitted mid-session. */
    ssd->savebutton = ctrl_pushbutton(s, "저장(V)", 'v',
				      HELPCTX(session_saved),
				      sessionsaver_handler, P(ssd));
    ssd->savebutton->generic.column = 1;
    if (!midsession) {
	ssd->delbutton = ctrl_pushbutton(s, "지움(D)", 'd',
					 HELPCTX(session_saved),
					 sessionsaver_handler, P(ssd));
	ssd->delbutton->generic.column = 1;
    } else {
	/* Disable the Delete button mid-session too, for UI consistency. */
	ssd->delbutton = NULL;
    }
    ctrl_columns(s, 1, 100);

    s = ctrl_getset(b, "세션", "otheropts", NULL);
    c = ctrl_radiobuttons(s, "종료시에 창을 닫음 (W):", 'w', 4,
			  HELPCTX(session_coe),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, close_on_exit)),
			  "항상", I(FORCE_ON),
			  "안 닫음", I(FORCE_OFF),
			  "접속이 끊겼을 때만", I(AUTO), NULL);

    /*
     * The Session/Logging panel.
     */
    ctrl_settitle(b, "세션/로깅", "세션 로그 제어 옵션을 조정합니다");

    s = ctrl_getset(b, "세션/로깅", "main", NULL);
    /*
     * The logging buttons change depending on whether SSH packet
     * logging can sensibly be available.
     */
    {
	char *sshlogname, *sshrawlogname;
	if ((midsession && protocol == PROT_SSH) ||
	    (!midsession && have_backend(PROT_SSH))) {
	    sshlogname = "SSH 패킷";
	    sshrawlogname = "SSH packets and raw data";
        } else {
	    sshlogname = NULL;	       /* this will disable both buttons */
	    sshrawlogname = NULL;      /* this will just placate optimisers */
        }
	ctrl_radiobuttons(s, "세션 로그:", NO_SHORTCUT, 2,
			  HELPCTX(logging_main),
			  loggingbuttons_handler,
			  I(offsetof(Config, logtype)),
			  "기록하지 않음 (T)", 't', I(LGTYP_NONE),
			  "출력 가능한 글자만 (P)", 'p', I(LGTYP_ASCII),
			  "모든 세션 출력 (L)", 'l', I(LGTYP_DEBUG),
			  sshlogname, 's', I(LGTYP_PACKETS),
			  sshrawlogname, 'r', I(LGTYP_SSHRAW),
			  NULL);
    }
    ctrl_filesel(s, "로그 파일 이름 (F):", 'f',
		 NULL, TRUE, "세션 로그 파일 이름 선택",
		 HELPCTX(logging_filename),
		 dlg_stdfilesel_handler, I(offsetof(Config, logfilename)));
    ctrl_text(s, "(로그 파일 이름엔 날짜는 &Y, &M, &D, 시간은 &T, 호스트 이름으로는 &H를 쓸 수 있습니다)",
	      HELPCTX(logging_filename));
    ctrl_radiobuttons(s, "로그 파일이 이미 있을 때 (E):", 'e', 1,
		      HELPCTX(logging_exists),
		      dlg_stdradiobutton_handler, I(offsetof(Config,logxfovr)),
		      "늘 덮어 씀", I(LGXF_OVR),
		      "늘 파일 끝에 추가함", I(LGXF_APN),
		      "매번 사용자에게 물어 봄", I(LGXF_ASK), NULL);
    ctrl_checkbox(s, "로그 파일을 자주 뿌림", 'u',
		 HELPCTX(logging_flush),
		 dlg_stdcheckbox_handler, I(offsetof(Config,logflush)));

    if ((midsession && protocol == PROT_SSH) ||
	(!midsession && have_backend(PROT_SSH))) {
	s = ctrl_getset(b, "세션/로깅", "ssh",
			"SSH 패킷 로깅 옵션을 조정합니다");
	ctrl_checkbox(s, "패스워드 처럼 보이는 것 숨김 (K)", 'k',
		      HELPCTX(logging_ssh_omit_password),
		      dlg_stdcheckbox_handler, I(offsetof(Config,logomitpass)));
	ctrl_checkbox(s, "세션 데이터 숨김 (D)", 'd',
		      HELPCTX(logging_ssh_omit_data),
		      dlg_stdcheckbox_handler, I(offsetof(Config,logomitdata)));
    }

    /*
     * The Terminal panel.
     */
    ctrl_settitle(b, "터미널", "터미널 에뮬레이션 제어 옵션을 조정합니다");

    s = ctrl_getset(b, "터미널", "general", "다양한 터미널 옵션");
    ctrl_checkbox(s, "기본으로 자동 줄넘김 (W)", 'w',
		  HELPCTX(terminal_autowrap),
		  dlg_stdcheckbox_handler, I(offsetof(Config,wrap_mode)));
    ctrl_checkbox(s, "DEC 줄 모드를 기본으로 켬", 'd',
		  HELPCTX(terminal_decom),
		  dlg_stdcheckbox_handler, I(offsetof(Config,dec_om)));
    ctrl_checkbox(s, "CR이 없어도 LF가 발견되면 줄넘김 인정", 'r',
		  HELPCTX(terminal_lfhascr),
		  dlg_stdcheckbox_handler, I(offsetof(Config,lfhascr)));
    ctrl_checkbox(s, "배경색으로 화면 지움 (E)", 'e',
		  HELPCTX(terminal_bce),
		  dlg_stdcheckbox_handler, I(offsetof(Config,bce)));
    ctrl_checkbox(s, "글자 깜빡임 사용 (N)", 'n',
		  HELPCTX(terminal_blink),
		  dlg_stdcheckbox_handler, I(offsetof(Config,blinktext)));
    ctrl_editbox(s, "^E에 대한 응답 (S):", 's', 100,
		 HELPCTX(terminal_answerback),
		 dlg_stdeditbox_handler, I(offsetof(Config,answerback)),
		 I(sizeof(((Config *)0)->answerback)));

    s = ctrl_getset(b, "터미널", "ldisc", "행 구분 옵션");
    ctrl_radiobuttons(s, "자국 반향 (L):", 'l', 3,
		      HELPCTX(terminal_localecho),
		      dlg_stdradiobutton_handler,I(offsetof(Config,localecho)),
		      "자동", I(AUTO),
		      "항상 켬", I(FORCE_ON),
		      "항상 끔", I(FORCE_OFF), NULL);
    ctrl_radiobuttons(s, "자체 줄 편집 (T):", 't', 3,
		      HELPCTX(terminal_localedit),
		      dlg_stdradiobutton_handler,I(offsetof(Config,localedit)),
		      "자동", I(AUTO),
		      "항상 켬", I(FORCE_ON),
		      "항상 끔", I(FORCE_OFF), NULL);

    s = ctrl_getset(b, "터미널", "printing", "원격 제어 프린트");
    ctrl_combobox(s, "ANSI 프린터 출력을 보낼 프린터 (P):", 'p', 100,
		  HELPCTX(terminal_printing),
		  printerbox_handler, P(NULL), P(NULL));

    /*
     * The Terminal/Keyboard panel.
     */
    ctrl_settitle(b, "터미널/키보드",
		  "키 효과 제어 옵션을 조정합니다");

    s = ctrl_getset(b, "터미널/키보드", "mappings",
		    "키 입력 전달 방법 조절:");
    ctrl_radiobuttons(s, "백스페이스 키 (B)", 'b', 2,
		      HELPCTX(keyboard_backspace),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, bksp_is_delete)),
		      "Ctrl-H", I(0), "Ctrl-? (127)", I(1), NULL);
    ctrl_radiobuttons(s, "Home과 End키", 'e', 2,
		      HELPCTX(keyboard_homeend),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, rxvt_homeend)),
		      "표준", I(0), "rxvt", I(1), NULL);
    ctrl_radiobuttons(s, "펑션키와 키패드 (F)", 'f', 3,
		      HELPCTX(keyboard_funkeys),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, funky_type)),
		      "ESC[n~", I(0), "리눅스", I(1), "Xterm R6", I(2),
		      "VT400", I(3), "VT100+", I(4), "SCO", I(5), 
                      "한텀", I(6), NULL);

    s = ctrl_getset(b, "터미널/키보드", "appkeypad",
		    "애플리케이션 키패드 설정:");
    ctrl_radiobuttons(s, "커서 키 기본 상태 (R):", 'r', 3,
		      HELPCTX(keyboard_appcursor),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, app_cursor)),
		      "일반", I(0), "프로그램", I(1), NULL);
    ctrl_radiobuttons(s, "숫자 키패드 기본 상태 (N):", 'n', 3,
		      HELPCTX(keyboard_appkeypad),
		      numeric_keypad_handler, P(NULL),
		      "일반", I(0), "프로그램", I(1), "넷핵", I(2),
		      NULL);

    /*
     * The Terminal/Bell panel.
     */
    ctrl_settitle(b, "터미널/벨",
		  "터미널 벨 제어 옵션을 조정합니다");

    s = ctrl_getset(b, "터미널/벨", "style", "벨 스타일");
    ctrl_radiobuttons(s, "벨 울림 효과 (B):", 'b', 1,
		      HELPCTX(bell_style),
		      dlg_stdradiobutton_handler, I(offsetof(Config, beep)),
		      "없음 (벨 사용 안 함) ", I(BELL_DISABLED),
		      "기본 시스템 벨소리", I(BELL_DEFAULT),
		      "시각 벨 (창 번쩍임)", I(BELL_VISUAL), NULL);

    s = ctrl_getset(b, "터미널/벨", "overload",
		    "일시적인 벨소리 과부하 제어");
    ctrl_checkbox(s, "벨소리가 시끄러울 때 임시로 무시함 (D)", 'd',
		  HELPCTX(bell_overload),
		  dlg_stdcheckbox_handler, I(offsetof(Config,bellovl)));
    ctrl_editbox(s, "최소 벨 과부하 결정 기준", 'm', 20,
		 HELPCTX(bell_overload),
		 dlg_stdeditbox_handler, I(offsetof(Config,bellovl_n)), I(-1));
    ctrl_editbox(s, "멈춤 시간(T) (초)", 't', 20,
		 HELPCTX(bell_overload),
		 dlg_stdeditbox_handler, I(offsetof(Config,bellovl_t)),
		 I(-TICKSPERSEC));
    ctrl_text(s, "일정 시간 조용하면 벨소리 다시 복구",
	      HELPCTX(bell_overload));
    ctrl_editbox(s, "해제 시간 (S)", 's', 20,
		 HELPCTX(bell_overload),
		 dlg_stdeditbox_handler, I(offsetof(Config,bellovl_s)),
		 I(-TICKSPERSEC));

    /*
     * The Terminal/Features panel.
     */
    ctrl_settitle(b, "터미널/기능",
		  "부가적인 터미널 기능들을 켜거나 끕니다.");

    s = ctrl_getset(b, "터미널/기능", "main", NULL);
    ctrl_checkbox(s, "애플리케이션 커서 키 모드 사용 안 함 (U)", 'u',
		  HELPCTX(features_application),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_applic_c)));
    ctrl_checkbox(s, "애플리케이션 키패드 모드 사용 안 함 (K)", 'k',
		  HELPCTX(features_application),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_applic_k)));
    ctrl_checkbox(s, "xterm 스타일 마우스 사용 안 함", 'x',
		  HELPCTX(features_mouse),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_mouse_rep)));
    ctrl_checkbox(s, "원격 창 크기 조절 사용 안 함 (S)", 's',
		  HELPCTX(features_resize),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,no_remote_resize)));
    ctrl_checkbox(s, "대체 터미널 스크린 변경 기능 사용 안 함 (W)", 'w',
		  HELPCTX(features_altscreen),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_alt_screen)));
    ctrl_checkbox(s, "원격 창 제목 변경 사용 안 함 (T)", 't',
		  HELPCTX(features_retitle),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,no_remote_wintitle)));
    ctrl_radiobuttons(s, "원격에서의 창 제목 알아내기 응답 (보안) (Q)", 'q', 3,
		      HELPCTX(features_qtitle),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config,remote_qtitle_action)),
		      "응답하지 않음", I(TITLE_NONE),
		      "빈 문자열", I(TITLE_EMPTY),
		      "창 제목", I(TITLE_REAL), NULL);
    ctrl_checkbox(s, "서버가 보내는 ^? 삭제를 사용 안 함 (B)",'b',
		  HELPCTX(features_dbackspace),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_dbackspace)));
    ctrl_checkbox(s, "원격 문자 셋 설정 사용 안 함 (R)",
		  'r', HELPCTX(features_charset), dlg_stdcheckbox_handler,
		  I(offsetof(Config,no_remote_charset)));
    ctrl_checkbox(s, "아랍어 출력을 사용하지 않음 (L)",
		  'l', HELPCTX(features_arabicshaping), dlg_stdcheckbox_handler,
		  I(offsetof(Config, arabicshaping)));
    ctrl_checkbox(s, "오른쪽에서 왼쪽으로 글쓰기 사용 안 함 (D)",
		  'd', HELPCTX(features_bidi), dlg_stdcheckbox_handler,
		  I(offsetof(Config, bidi)));

    /*
     * The Window panel.
     */
    str = dupprintf("%s의 창 제어 옵션을 조정합니다", appname);
    ctrl_settitle(b, "창", str);
    sfree(str);

    s = ctrl_getset(b, "창", "size", "창 크기 조절");
    ctrl_columns(s, 2, 50, 50);
    c = ctrl_editbox(s, "가로 (M)", 'm', 100,
		     HELPCTX(window_size),
		     dlg_stdeditbox_handler, I(offsetof(Config,width)), I(-1));
    c->generic.column = 0;
    c = ctrl_editbox(s, "줄 (R)", 'r', 100,
		     HELPCTX(window_size),
		     dlg_stdeditbox_handler, I(offsetof(Config,height)),I(-1));
    c->generic.column = 1;
    ctrl_columns(s, 1, 100);

    s = ctrl_getset(b, "창", "scrollback",
		    "창 이전 내용 올려보기 설정");
    ctrl_editbox(s, "이전화면 저장 줄 수 (S)", 's', 50,
		 HELPCTX(window_scrollback),
		 dlg_stdeditbox_handler, I(offsetof(Config,savelines)), I(-1));
    ctrl_checkbox(s, "스크롤 바 보임 (D)", 'd',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler, I(offsetof(Config,scrollbar)));
    ctrl_checkbox(s, "키 입력이 있으면 이전화면 보기를 초기화 (K)", 'k',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler, I(offsetof(Config,scroll_on_key)));
    ctrl_checkbox(s, "출력이 있으면 이전화면 보기를 초기화 (P)", 'p',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler, I(offsetof(Config,scroll_on_disp)));
    ctrl_checkbox(s, "지워진 글을 이전화면으로 밀어냄 (E)", 'e',
		  HELPCTX(window_erased),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,erase_to_scrollback)));

    /*
     * The Window/Appearance panel.
     */
    str = dupprintf("%s 창 모양을 설정합니다.", appname);
    ctrl_settitle(b, "창/모양", str);
    sfree(str);

    s = ctrl_getset(b, "창/모양", "cursor",
		    "커서 동작 조절");
    ctrl_radiobuttons(s, "커서 모양:", NO_SHORTCUT, 3,
		      HELPCTX(appearance_cursor),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, cursor_type)),
		      "사각형 (B)", 'l', I(0),
		      "밑줄 (U)", 'u', I(1),
		      "세로줄 (V)", 'v', I(2), NULL);
    ctrl_checkbox(s, "커서 깜빡임 (B)", 'b',
		  HELPCTX(appearance_cursor),
		  dlg_stdcheckbox_handler, I(offsetof(Config,blink_cur)));

    s = ctrl_getset(b, "창/모양", "font",
		    "글꼴 설정");
    ctrl_fontsel(s, "터미널 기본 글꼴 (N)", 'n',
		 HELPCTX(appearance_font),
		 dlg_stdfontsel_handler, I(offsetof(Config, font)));
    ctrl_checkbox(s, "별도 유니코드 글꼴 사용 (F)", 'f',
                 HELPCTX(no_help),
                 dlg_stdcheckbox_handler, I(offsetof(Config, use_font_unicode)));
    ctrl_fontsel(s, "터미널 유니코드 글꼴 (S)", '\0',
                 HELPCTX(no_help),
                 dlg_stdfontsel_handler, I(offsetof(Config, font_unicode)));
    ctrl_editbox(s, "유니코드 글꼴 Y 좌표 미세 조정 (px)", '\0', 20,
                 HELPCTX(no_help),
                 dlg_stdeditbox_handler, I(offsetof(Config, font_unicode_adj)), I(-1));

    s = ctrl_getset(b, "창/모양", "mouse",
		    "마우스 포인터 동작 조절");
    ctrl_checkbox(s, "입력 중에는 마우스 커서 숨김 (P)", 'p',
		  HELPCTX(appearance_hidemouse),
		  dlg_stdcheckbox_handler, I(offsetof(Config,hide_mouseptr)));

    s = ctrl_getset(b, "창/모양", "border",
		    "창틀 조절");
    ctrl_editbox(s, "본문과 창틀 간격:", 'e', 20,
		 HELPCTX(appearance_border),
		 dlg_stdeditbox_handler,
		 I(offsetof(Config,window_border)), I(-1));

    /*
     * The Window/Behaviour panel.
     */
    str = dupprintf("%s 창 특성을 설정합니다.", appname);
    ctrl_settitle(b, "창/특성", str);
    sfree(str);

    s = ctrl_getset(b, "창/특성", "title",
		    "창 제목 조절");
    ctrl_editbox(s, "창 제목 (T):", 't', 100,
		 HELPCTX(appearance_title),
		 dlg_stdeditbox_handler, I(offsetof(Config,wintitle)),
		 I(sizeof(((Config *)0)->wintitle)));
    ctrl_checkbox(s, "창과 아이콘 제목을 분리 (I)", 'i',
		  HELPCTX(appearance_title),
		  dlg_stdcheckbox_handler,
		  I(CHECKBOX_INVERT | offsetof(Config,win_name_always)));

    s = ctrl_getset(b, "창/특성", "main", NULL);
    ctrl_checkbox(s, "창 닫기 전에 경고 (W)", 'w',
		  HELPCTX(behaviour_closewarn),
		  dlg_stdcheckbox_handler, I(offsetof(Config,warn_on_close)));

    /*
     * The Window/Translation panel.
     */
    ctrl_settitle(b, "창/변환",
		  "문자 셋 변환 제어 옵션을 조정합니다");

    s = ctrl_getset(b, "창/변환", "trans",
		    "수신 데이터의 문자 셋 변환");
    ctrl_combobox(s, "수신한 데이터를 이 문자셋으로 가정 (R):",
		  'r', 100, HELPCTX(translation_codepage),
		  codepage_handler, P(NULL), P(NULL));

    s = ctrl_getset(b, "창/변환", "tweaks", NULL);
    ctrl_checkbox(s, "너비가 지정되지 않은 한중일 글자를 넓게 표시 (W)", 'w',
		  HELPCTX(translation_cjk_ambig_wide),
		  dlg_stdcheckbox_handler, I(offsetof(Config,cjk_ambig_wide)));

    str = dupprintf("%s가 선그림 문자를 다루는 방법", appname);
    s = ctrl_getset(b, "창/변환", "linedraw", str);
    sfree(str);
    ctrl_radiobuttons(s, "선그림 문자 다루기:", NO_SHORTCUT,1,
		      HELPCTX(translation_linedraw),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, vtmode)),
		      "유니코드 선문자 사용 (U)",'u',I(VT_UNICODE),
		      "가난한 자의 선그림 문자 (+, - 와 |) (P)",'p',I(VT_POORMAN),
		      NULL);
    ctrl_checkbox(s, "VT100 선그림 문자를 lqqqk로 뿌림 (D)",'d',
		  HELPCTX(selection_linedraw),
		  dlg_stdcheckbox_handler, I(offsetof(Config,rawcnp)));

    /*
     * The Window/Selection panel.
     */
    ctrl_settitle(b, "창/선택", "복사, 붙여넣기 제어 옵션을 조정합니다");
	
    s = ctrl_getset(b, "창/선택", "mouse",
		    "마우스 설정");
    ctrl_checkbox(s, "쉬프트로 애플리케이션 마우스사용을 엎음 (P)", 'p',
		  HELPCTX(selection_shiftdrag),
		  dlg_stdcheckbox_handler, I(offsetof(Config,mouse_override)));
    ctrl_radiobuttons(s,
		      "기본 영역 선택 모드 (Alt+드래그는 다름):",
		      NO_SHORTCUT, 2,
		      HELPCTX(selection_rect),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, rect_select)),
		      "일반", 'n', I(0),
		      "사각 블럭 (R)", 'r', I(1), NULL);

    s = ctrl_getset(b, "창/선택", "charclass",
		    "클릭해서 한 단어만 선택하기 설정");
    ccd = (struct charclass_data *)
	ctrl_alloc(b, sizeof(struct charclass_data));
    ccd->listbox = ctrl_listbox(s, "문자 종류 (E):", 'e',
				HELPCTX(selection_charclasses),
				charclass_handler, P(ccd));
    ccd->listbox->listbox.multisel = 1;
    ccd->listbox->listbox.ncols = 4;
    ccd->listbox->listbox.percentages = snewn(4, int);
    ccd->listbox->listbox.percentages[0] = 15;
    ccd->listbox->listbox.percentages[1] = 25;
    ccd->listbox->listbox.percentages[2] = 20;
    ccd->listbox->listbox.percentages[3] = 40;
    ctrl_columns(s, 2, 67, 33);
    ccd->editbox = ctrl_editbox(s, "클래스 (T)", 't', 50,
				HELPCTX(selection_charclasses),
				charclass_handler, P(ccd), P(NULL));
    ccd->editbox->generic.column = 0;
    ccd->button = ctrl_pushbutton(s, "바꿈 (S)", 's',
				  HELPCTX(selection_charclasses),
				  charclass_handler, P(ccd));
    ccd->button->generic.column = 1;
    ctrl_columns(s, 1, 100);

    /*
     * The Window/Colours panel.
     */
    ctrl_settitle(b, "창/색깔", "색상 사용 옵션을 조정합니다");

    s = ctrl_getset(b, "창/색깔", "general",
		    "색깔관련 일반 옵션");
    ctrl_checkbox(s, "ANSI 색상 지정 허용", 'i',
		  HELPCTX(colours_ansi),
		  dlg_stdcheckbox_handler, I(offsetof(Config,ansi_colour)));
    ctrl_checkbox(s, "xterm 256색 모드 허용", '2',
		  HELPCTX(colours_xterm256), dlg_stdcheckbox_handler,
		  I(offsetof(Config,xterm_256_colour)));
    ctrl_checkbox(s, "굵은 글씨를 다른 색깔로 표시 (B)", 'b',
		  HELPCTX(colours_bold),
		  dlg_stdcheckbox_handler, I(offsetof(Config,bold_colour)));

    str = dupprintf("색상 미세 조절", appname);
    s = ctrl_getset(b, "창/색깔", "adjust", str);
    sfree(str);
    ctrl_text(s, "목록에서 색깔을 고르고 변경 버튼을 눌러 표현색을 바꾸세요.",
	      HELPCTX(colours_config));
    ctrl_columns(s, 2, 67, 33);
    cd = (struct colour_data *)ctrl_alloc(b, sizeof(struct colour_data));
    cd->listbox = ctrl_listbox(s, "조정할 색깔 선택 (U):", 'u',
			       HELPCTX(colours_config), colour_handler, P(cd));
    cd->listbox->generic.column = 0;
    cd->listbox->listbox.height = 7;
    c = ctrl_text(s, "RGB 값:", HELPCTX(colours_config));
    c->generic.column = 1;
    cd->redit = ctrl_editbox(s, "빨강", 'r', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->redit->generic.column = 1;
    cd->gedit = ctrl_editbox(s, "녹색", 'n', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->gedit->generic.column = 1;
    cd->bedit = ctrl_editbox(s, "파랑", 'e', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->bedit->generic.column = 1;
    cd->button = ctrl_pushbutton(s, "변경 (M)", 'm', HELPCTX(colours_config),
				 colour_handler, P(cd));
    cd->button->generic.column = 1;
    ctrl_columns(s, 1, 100);

    /*
     * The Connection panel. This doesn't show up if we're in a
     * non-network utility such as pterm. We tell this by being
     * passed a protocol < 0.
     */
    if (protocol >= 0) {
	ctrl_settitle(b, "접속", "접속 제어 옵션을 조정합니다");

	s = ctrl_getset(b, "접속", "keepalive",
			"세션 유지를 위한 빈패킷 보내기");
	ctrl_editbox(s, "접속 유지 간격 (초단위, 0은 끔)", 'k', 20,
		     HELPCTX(connection_keepalive),
		     dlg_stdeditbox_handler, I(offsetof(Config,ping_interval)),
		     I(-1));

	if (!midsession) {
	    s = ctrl_getset(b, "접속", "tcp",
			    "저수준 TCP 압축 옵션");
	    ctrl_checkbox(s, "네이글 알고리즘 사용 안 함 (N)",
			  'n', HELPCTX(connection_nodelay),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,tcp_nodelay)));
	    ctrl_checkbox(s, "TCP 접속유지 사용 함 (SO_KEEPALIVE 옵션)",
			  'p', HELPCTX(connection_tcpkeepalive),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,tcp_keepalives)));
#ifndef NO_IPV6
	    s = ctrl_getset(b, "접속", "ipversion",
			  "인터넷 프로토콜 버전");
	    ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
			  HELPCTX(connection_ipversion),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, addressfamily)),
			  "자동", 'u', I(ADDRTYPE_UNSPEC),
			  "IPv4", '4', I(ADDRTYPE_IPV4),
			  "IPv6", '6', I(ADDRTYPE_IPV6),
			  NULL);
#endif
	}

	/*
	 * A sub-panel Connection/Data, containing options that
	 * decide on data to send to the server.
	 */
	if (!midsession) {
	    ctrl_settitle(b, "접속/데이터", "서버에 보낼 데이터");

	    s = ctrl_getset(b, "접속/데이터", "login",
			    "로그인 정보");
	    ctrl_editbox(s, "자동 로그인 사용자 (U)", 'u', 50,
			 HELPCTX(connection_username),
			 dlg_stdeditbox_handler, I(offsetof(Config,username)),
			 I(sizeof(((Config *)0)->username)));

	    s = ctrl_getset(b, "접속/데이터", "term",
			    "터미널 정보");
	    ctrl_editbox(s, "터미널 타입 문자열 (T)", 't', 50,
			 HELPCTX(connection_termtype),
			 dlg_stdeditbox_handler, I(offsetof(Config,termtype)),
			 I(sizeof(((Config *)0)->termtype)));
	    ctrl_editbox(s, "터미널 속도 (S)", 's', 50,
			 HELPCTX(connection_termspeed),
			 dlg_stdeditbox_handler, I(offsetof(Config,termspeed)),
			 I(sizeof(((Config *)0)->termspeed)));

	    s = ctrl_getset(b, "접속/데이터", "env",
			    "환경 변수");
	    ctrl_columns(s, 2, 80, 20);
	    ed = (struct environ_data *)
		ctrl_alloc(b, sizeof(struct environ_data));
	    ed->varbox = ctrl_editbox(s, "변수(V)", 'v', 60,
				      HELPCTX(telnet_environ),
				      environ_handler, P(ed), P(NULL));
	    ed->varbox->generic.column = 0;
	    ed->valbox = ctrl_editbox(s, "값(L)", 'l', 60,
				      HELPCTX(telnet_environ),
				      environ_handler, P(ed), P(NULL));
	    ed->valbox->generic.column = 0;
	    ed->addbutton = ctrl_pushbutton(s, "추가 (D)", 'd',
					    HELPCTX(telnet_environ),
					    environ_handler, P(ed));
	    ed->addbutton->generic.column = 1;
	    ed->rembutton = ctrl_pushbutton(s, "지움 (R)", 'r',
					    HELPCTX(telnet_environ),
					    environ_handler, P(ed));
	    ed->rembutton->generic.column = 1;
	    ctrl_columns(s, 1, 100);
	    ed->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				       HELPCTX(telnet_environ),
				       environ_handler, P(ed));
	    ed->listbox->listbox.height = 3;
	    ed->listbox->listbox.ncols = 2;
	    ed->listbox->listbox.percentages = snewn(2, int);
	    ed->listbox->listbox.percentages[0] = 30;
	    ed->listbox->listbox.percentages[1] = 70;
	}

    }

    if (!midsession) {
	/*
	 * The Connection/Proxy panel.
	 */
	ctrl_settitle(b, "접속/프락시",
		      "프락시 제어 옵션을 조정합니다");

	s = ctrl_getset(b, "접속/프락시", "basics", NULL);
	ctrl_radiobuttons(s, "프락시 유형 (T):", 't', 3,
			  HELPCTX(proxy_type),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, proxy_type)),
			  "없음", I(PROXY_NONE),
			  "SOCKS 4", I(PROXY_SOCKS4),
			  "SOCKS 5", I(PROXY_SOCKS5),
			  "HTTP", I(PROXY_HTTP),
			  "Telnet", I(PROXY_TELNET),
			  NULL);
	ctrl_columns(s, 2, 80, 20);
	c = ctrl_editbox(s, "프락시 호스트 (Y)", 'y', 100,
			 HELPCTX(proxy_main),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,proxy_host)),
			 I(sizeof(((Config *)0)->proxy_host)));
	c->generic.column = 0;
	c = ctrl_editbox(s, "포트 (P)", 'p', 100,
			 HELPCTX(proxy_main),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,proxy_port)),
			 I(-1));
	c->generic.column = 1;
	ctrl_columns(s, 1, 100);
	ctrl_editbox(s, "금지된 호스트/IP (E)", 'e', 100,
		     HELPCTX(proxy_exclude),
		     dlg_stdeditbox_handler,
		     I(offsetof(Config,proxy_exclude_list)),
		     I(sizeof(((Config *)0)->proxy_exclude_list)));
	ctrl_checkbox(s, "로컬 호스트 접속을 프락시에서 고려함 (X)", 'x',
		      HELPCTX(proxy_exclude),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,even_proxy_localhost)));
	ctrl_radiobuttons(s, "프락시 접속에 대해 DNS 네임을 찾음:", 'd', 3,
			  HELPCTX(proxy_dns),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, proxy_dns)),
			  "아니요", I(FORCE_OFF),
			  "자동", I(AUTO),
			  "예", I(FORCE_ON), NULL);
	ctrl_editbox(s, "사용자명 (U)", 'u', 60,
		     HELPCTX(proxy_auth),
		     dlg_stdeditbox_handler,
		     I(offsetof(Config,proxy_username)),
		     I(sizeof(((Config *)0)->proxy_username)));
	c = ctrl_editbox(s, "암호 (W)", 'w', 60,
			 HELPCTX(proxy_auth),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,proxy_password)),
			 I(sizeof(((Config *)0)->proxy_password)));
	c->editbox.password = 1;
	ctrl_editbox(s, "텔넷 명령 (M)", 'm', 100,
		     HELPCTX(proxy_command),
		     dlg_stdeditbox_handler,
		     I(offsetof(Config,proxy_telnet_command)),
		     I(sizeof(((Config *)0)->proxy_telnet_command)));
    }

    /*
     * The Telnet panel exists in the base config box, and in a
     * mid-session reconfig box _if_ we're using Telnet.
     */
    if (!midsession || protocol == PROT_TELNET) {
	/*
	 * The Connection/Telnet panel.
	 */
	ctrl_settitle(b, "접속/텔넷",
		      "텔넷 접속 제어 옵션을 조정합니다");

	s = ctrl_getset(b, "접속/텔넷", "protocol",
			"텔넷 프로토콜 조정");

	if (!midsession) {
	    ctrl_radiobuttons(s, "OLD_ENVIRON 모호성 다루기:",
			      NO_SHORTCUT, 2,
			      HELPCTX(telnet_oldenviron),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, rfc_environ)),
			      "BSD (통상)", 'b', I(0),
			      "RFC 1408 (안 쓰임)", 'f', I(1), NULL);
	    ctrl_radiobuttons(s, "텔넷 협상 모드 (T):", 't', 2,
			      HELPCTX(telnet_passive),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, passive_telnet)),
			      "수동", I(1), "능동", I(0), NULL);
	}
	ctrl_checkbox(s, "키보드로 텔넷 특수 명령 보냄 (K)", 'k',
		      HELPCTX(telnet_specialkeys),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,telnet_keyboard)));
	ctrl_checkbox(s, "CR대신 텔넷 New Line 문자를 보냄",
		      'm', HELPCTX(telnet_newline),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,telnet_newline)));
    }

    if (!midsession) {

	/*
	 * The Connection/Rlogin panel.
	 */
	ctrl_settitle(b, "접속/rlogin",
		      "Rlogin 접속 제어 옵션을 조정합니다");

	s = ctrl_getset(b, "접속/rlogin", "data",
			"서버에 보낼 데이터");
	ctrl_editbox(s, "사용자 명 (L):", 'l', 50,
		     HELPCTX(rlogin_localuser),
		     dlg_stdeditbox_handler, I(offsetof(Config,localusername)),
		     I(sizeof(((Config *)0)->localusername)));

    }

    /*
     * All the SSH stuff is omitted in PuTTYtel, or in a reconfig
     * when we're not doing SSH.
     */

    if (have_backend(PROT_SSH) && (!midsession || protocol == PROT_SSH)) {

	/*
	 * The Connection/SSH panel.
	 */
	ctrl_settitle(b, "접속/SSH",
		      "SSH 접속 옵션을 조정합니다");

	if (midsession && protcfginfo == 1) {
	    s = ctrl_getset(b, "접속/SSH", "disclaimer", NULL);
	    ctrl_text(s, "이 화면에 있는 설정은 접속 중에는 변경되지 않습니다. 단지 하위의 다른 설정을 표시하기 위해서 나오고 있을 뿐입니다.", HELPCTX(no_help));
	}

	if (!midsession) {

	    s = ctrl_getset(b, "접속/SSH", "data",
			    "서버에 보낼 데이터");
	    ctrl_editbox(s, "원격 명령 (R):", 'r', 100,
			 HELPCTX(ssh_command),
			 dlg_stdeditbox_handler, I(offsetof(Config,remote_cmd)),
			 I(sizeof(((Config *)0)->remote_cmd)));

	    s = ctrl_getset(b, "접속/SSH", "protocol", "프로토콜 옵션");
	    ctrl_checkbox(s, "셸이나 명령어 실행을 하지 않음 (N)", 'n',
			  HELPCTX(ssh_noshell),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,ssh_no_shell)));
	}

	if (!midsession || protcfginfo != 1) {
	    s = ctrl_getset(b, "접속/SSH", "protocol", "프로토콜 옵션");

	    ctrl_checkbox(s, "압축 허용 (E)", 'e',
			  HELPCTX(ssh_compress),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,compression)));
	}

	if (!midsession) {
	    s = ctrl_getset(b, "접속/SSH", "protocol", "프로토콜 옵션");

	    ctrl_radiobuttons(s, "먼저 시도할 SSH 프로토콜 버전:", NO_SHORTCUT, 4,
			      HELPCTX(ssh_protocol),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, sshprot)),
			      "SSH1만", 'l', I(0),
			      "SSH1", '1', I(1),
			      "SSH2", '2', I(2),
			      "SSH2만", 'y', I(3), NULL);
	}

	if (!midsession || protcfginfo != 1) {
	    s = ctrl_getset(b, "접속/SSH", "encryption", "암호화 옵션");
	    c = ctrl_draglist(s, "암호 알고리즘 선택 정책 (S):", 's',
			      HELPCTX(ssh_ciphers),
			      cipherlist_handler, P(NULL));
	    c->listbox.height = 6;

	    ctrl_checkbox(s, "SSH 2에서 단일-DES 하위 호환성 사용 (I)", 'i',
			  HELPCTX(ssh_ciphers),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,ssh2_des_cbc)));
	}

	/*
	 * The Connection/SSH/Kex panel. (Owing to repeat key
	 * exchange, this is all meaningful in mid-session _if_
	 * we're using SSH-2 or haven't decided yet.)
	 */
	if (protcfginfo != 1) {
	    ctrl_settitle(b, "접속/SSH/키교환",
			  "SSH 키 교환 옵션을 조정합니다");

	    s = ctrl_getset(b, "접속/SSH/키교환", "main",
			    "키 교환 알고리즘 옵션");
	    c = ctrl_draglist(s, "암호 알고리즘 선택 정책 (S):", 's',
			      HELPCTX(ssh_kexlist),
			      kexlist_handler, P(NULL));
	    c->listbox.height = 5;

	    s = ctrl_getset(b, "접속/SSH/키교환", "repeat",
			    "키 재교환 제어 옵션을 조정합니다");

	    ctrl_editbox(s, "시간(분) 기준 키 재교환 (0: 무제한) (T)", 't', 20,
			 HELPCTX(ssh_kex_repeat),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,ssh_rekey_time)),
			 I(-1));
	    ctrl_editbox(s, "데이터 크기 기준 키 재교환 (0: 무제한) (K)", 'x', 20,
			 HELPCTX(ssh_kex_repeat),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,ssh_rekey_data)),
			 I(16));
	    ctrl_text(s, "(1메가는 1M, 1기가는 1G 같이 입력하세요)",
		      HELPCTX(ssh_kex_repeat));
	}

	if (!midsession) {

	    /*
	     * The Connection/SSH/Auth panel.
	     */
	    ctrl_settitle(b, "접속/SSH/인증",
			  "SSH 인증 제어 옵션을 조정합니다");

	    s = ctrl_getset(b, "접속/SSH/인증", "main", NULL);
	    ctrl_checkbox(s, "인증을 모두 건너 뜀 (SSH-2) (B)", 'b',
			  HELPCTX(ssh_auth_bypass),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,ssh_no_userauth)));

	    s = ctrl_getset(b, "접속/SSH/인증", "methods",
			    "인증 방법");
	    ctrl_checkbox(s, "Pageant를 이용한 인증을 시도 (P)", 'p',
			  HELPCTX(ssh_auth_pageant),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,tryagent)));
	    ctrl_checkbox(s, "SSH1에서 TIS나 CryptoCard 인증을 시도 (M)", 'm',
			  HELPCTX(ssh_auth_tis),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,try_tis_auth)));
	    ctrl_checkbox(s, "SSH2에서 \"키보드로 직접 입력\"인증을 시도 (I)",
			  'i', HELPCTX(ssh_auth_ki),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,try_ki_auth)));

	    s = ctrl_getset(b, "접속/SSH/인증", "params",
			    "인증 인자");
	    ctrl_checkbox(s, "에이전트 포워딩 허용 (F)", 'f',
			  HELPCTX(ssh_auth_agentfwd),
			  dlg_stdcheckbox_handler, I(offsetof(Config,agentfwd)));
	    ctrl_checkbox(s, "SSH2에서 사용자 이름 변경을 시도함 (U)", 'u',
			  HELPCTX(ssh_auth_changeuser),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,change_username)));
	    ctrl_filesel(s, "인증 개인키 파일 (K):", 'k',
			 FILTER_KEY_FILES, FALSE, "비밀키 파일 선택",
			 HELPCTX(ssh_auth_privkey),
			 dlg_stdfilesel_handler, I(offsetof(Config, keyfile)));
	}

	if (!midsession) {
	    /*
	     * The Connection/SSH/TTY panel.
	     */
	    ctrl_settitle(b, "접속/SSH/TTY", "원격 터미널 설정");

	    s = ctrl_getset(b, "접속/SSH/TTY", "sshtty", NULL);
	    ctrl_checkbox(s, "수도(pseudo)-터미날을 할당하지 않음", 'p',
			  HELPCTX(ssh_nopty),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,nopty)));

	    s = ctrl_getset(b, "접속/SSH/TTY", "ttymodes",
			    "터미널 모드");
	    td = (struct ttymodes_data *)
		ctrl_alloc(b, sizeof(struct ttymodes_data));
	    ctrl_columns(s, 2, 75, 25);
	    c = ctrl_text(s, "보내는 터미널 모드", HELPCTX(ssh_ttymodes));
	    c->generic.column = 0;
	    td->rembutton = ctrl_pushbutton(s, "지움 (R)", 'r',
					    HELPCTX(ssh_ttymodes),
					    ttymodes_handler, P(td));
	    td->rembutton->generic.column = 1;
	    td->rembutton->generic.tabdelay = 1;
	    ctrl_columns(s, 1, 100);
	    td->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				       HELPCTX(ssh_ttymodes),
				       ttymodes_handler, P(td));
	    td->listbox->listbox.multisel = 1;
	    td->listbox->listbox.height = 4;
	    td->listbox->listbox.ncols = 2;
	    td->listbox->listbox.percentages = snewn(2, int);
	    td->listbox->listbox.percentages[0] = 40;
	    td->listbox->listbox.percentages[1] = 60;
	    ctrl_tabdelay(s, td->rembutton);
	    ctrl_columns(s, 2, 75, 25);
	    td->modelist = ctrl_droplist(s, "모드:", 'm', 67,
					 HELPCTX(ssh_ttymodes),
					 ttymodes_handler, P(td));
	    td->modelist->generic.column = 0;
	    td->addbutton = ctrl_pushbutton(s, "추가 (D)", 'd',
					    HELPCTX(ssh_ttymodes),
					    ttymodes_handler, P(td));
	    td->addbutton->generic.column = 1;
	    td->addbutton->generic.tabdelay = 1;
	    ctrl_columns(s, 1, 100);	    /* column break */
	    /* Bit of a hack to get the value radio buttons and
	     * edit-box on the same row. */
	    ctrl_columns(s, 3, 25, 50, 25);
	    c = ctrl_text(s, "값:", HELPCTX(ssh_ttymodes));
	    c->generic.column = 0;
	    td->valradio = ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 2,
					     HELPCTX(ssh_ttymodes),
					     ttymodes_handler, P(td),
					     "자동", NO_SHORTCUT, P(NULL),
					     "입력:", NO_SHORTCUT, P(NULL),
					     NULL);
	    td->valradio->generic.column = 1;
	    td->valbox = ctrl_editbox(s, NULL, NO_SHORTCUT, 100,
				      HELPCTX(ssh_ttymodes),
				      ttymodes_handler, P(td), P(NULL));
	    td->valbox->generic.column = 2;
	    ctrl_tabdelay(s, td->addbutton);

	}

	if (!midsession) {
	    /*
	     * The Connection/SSH/X11 panel.
	     */
	    ctrl_settitle(b, "접속/SSH/X11",
			  "SSH X11 포워드 제어 옵션을 조정합니다");

	    s = ctrl_getset(b, "접속/SSH/X11", "x11", "X11 포워딩");
	    ctrl_checkbox(s, "X11 포워딩 사용 (E)", 'e',
			  HELPCTX(ssh_tunnels_x11),
			  dlg_stdcheckbox_handler,I(offsetof(Config,x11_forward)));
	    ctrl_editbox(s, "X 디스플레이 위치", 'x', 50,
			 HELPCTX(ssh_tunnels_x11),
			 dlg_stdeditbox_handler, I(offsetof(Config,x11_display)),
			 I(sizeof(((Config *)0)->x11_display)));
	    ctrl_radiobuttons(s, "원격 X11 인증 프로토콜 (U)", 'u', 2,
			      HELPCTX(ssh_tunnels_x11auth),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, x11_auth)),
			      "MIT-Magic-Cookie-1", I(X11_MIT),
			      "XDM-Authorization-1", I(X11_XDM), NULL);
	}

	/*
	 * The Tunnels panel _is_ still available in mid-session.
	 */
	ctrl_settitle(b, "접속/SSH/터널링",
		      "SSH 포트 포워드 옵션을 조정합니다");

	s = ctrl_getset(b, "접속/SSH/터널링", "portfwd",
			"포트 포워딩");
	ctrl_checkbox(s, "다른 호스트에서 우리측 포트로의 접속 (T)",'t',
		      HELPCTX(ssh_tunnels_portfwd_localhost),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,lport_acceptall)));
	ctrl_checkbox(s, "원격 포트도 같은 방법으로 (SSH2만) (P)", 'p',
		      HELPCTX(ssh_tunnels_portfwd_localhost),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,rport_acceptall)));

	ctrl_columns(s, 3, 55, 20, 25);
	c = ctrl_text(s, "포워드 포트:", HELPCTX(ssh_tunnels_portfwd));
	c->generic.column = COLUMN_FIELD(0,2);
	/* You want to select from the list, _then_ hit Remove. So tab order
	 * should be that way round. */
	pfd = (struct portfwd_data *)ctrl_alloc(b,sizeof(struct portfwd_data));
	pfd->rembutton = ctrl_pushbutton(s, "지움 (R)", 'r',
					 HELPCTX(ssh_tunnels_portfwd),
					 portfwd_handler, P(pfd));
	pfd->rembutton->generic.column = 2;
	pfd->rembutton->generic.tabdelay = 1;
	pfd->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				    HELPCTX(ssh_tunnels_portfwd),
				    portfwd_handler, P(pfd));
	pfd->listbox->listbox.height = 3;
	pfd->listbox->listbox.ncols = 2;
	pfd->listbox->listbox.percentages = snewn(2, int);
	pfd->listbox->listbox.percentages[0] = 20;
	pfd->listbox->listbox.percentages[1] = 80;
	ctrl_tabdelay(s, pfd->rembutton);
	ctrl_text(s, "포워드될 포트 추가:", HELPCTX(ssh_tunnels_portfwd));
	/* You want to enter source, destination and type, _then_ hit Add.
	 * Again, we adjust the tab order to reflect this. */
	pfd->addbutton = ctrl_pushbutton(s, "추가 (D)", 'd',
					 HELPCTX(ssh_tunnels_portfwd),
					 portfwd_handler, P(pfd));
	pfd->addbutton->generic.column = 2;
	pfd->addbutton->generic.tabdelay = 1;
	pfd->sourcebox = ctrl_editbox(s, "원 포트 (S)", 's', 40,
				      HELPCTX(ssh_tunnels_portfwd),
				      portfwd_handler, P(pfd), P(NULL));
	pfd->sourcebox->generic.column = 0;
	pfd->destbox = ctrl_editbox(s, "대상 (I)", 'i', 67,
				    HELPCTX(ssh_tunnels_portfwd),
				    portfwd_handler, P(pfd), P(NULL));
	pfd->direction = ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
					   HELPCTX(ssh_tunnels_portfwd),
					   portfwd_handler, P(pfd),
					   "로컬 (L)", 'l', P(NULL),
					   "원격 (M)", 'm', P(NULL),
					   "동적 (Y)", 'y', P(NULL),
					   NULL);
#ifndef NO_IPV6
	pfd->addressfamily =
	    ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
			      HELPCTX(ssh_tunnels_portfwd_ipversion),
			      portfwd_handler, P(pfd),
			      "자동", 'u', I(ADDRTYPE_UNSPEC),
			      "IPv4", '4', I(ADDRTYPE_IPV4),
			      "IPv6", '6', I(ADDRTYPE_IPV6),
			      NULL);
#endif
	ctrl_tabdelay(s, pfd->addbutton);
	ctrl_columns(s, 1, 100);

	if (!midsession) {
	    /*
	     * The Connection/SSH/Bugs panel.
	     */
	    ctrl_settitle(b, "접속/SSH/버그",
			  "SSH 서버 버그 피해가기");

	    s = ctrl_getset(b, "접속/SSH/버그", "main",
			    "SSH 서버의 알려진 버그 피해가기");
	    ctrl_droplist(s, "SSH1 무시 메시지 문제 (I)", 'i', 20,
			  HELPCTX(ssh_bugs_ignore1),
			  sshbug_handler, I(offsetof(Config,sshbug_ignore1)));
	    ctrl_droplist(s, "SSH1 패스워드 위장을 거부", 's', 20,
			  HELPCTX(ssh_bugs_plainpw1),
			  sshbug_handler, I(offsetof(Config,sshbug_plainpw1)));
	    ctrl_droplist(s, "SSH1 RSA 인증 문제 (R)", 'r', 20,
			  HELPCTX(ssh_bugs_rsa1),
			  sshbug_handler, I(offsetof(Config,sshbug_rsa1)));
	    ctrl_droplist(s, "SSH2 HMAC 키를 잘못 계산 (M)", 'm', 20,
			  HELPCTX(ssh_bugs_hmac2),
			  sshbug_handler, I(offsetof(Config,sshbug_hmac2)));
	    ctrl_droplist(s, "SSH2 암호화 키를 잘못 계산함 (E)", 'e', 20,
			  HELPCTX(ssh_bugs_derivekey2),
			  sshbug_handler, I(offsetof(Config,sshbug_derivekey2)));
	    ctrl_droplist(s, "SSH2 RSA 시그너춰에 패딩 요구 (P)", 'p', 20,
			  HELPCTX(ssh_bugs_rsapad2),
			  sshbug_handler, I(offsetof(Config,sshbug_rsapad2)));
	    ctrl_droplist(s, "세션 ID를 PK 인증에 오용 (N)", 'n', 20,
			  HELPCTX(ssh_bugs_pksessid2),
			  sshbug_handler, I(offsetof(Config,sshbug_pksessid2)));
	    ctrl_droplist(s, "잘못된 SSH2 키 재교환 허용 (K)", 'k', 20,
			  HELPCTX(ssh_bugs_rekey2),
			  sshbug_handler, I(offsetof(Config,sshbug_rekey2)));
	}
    }
}
