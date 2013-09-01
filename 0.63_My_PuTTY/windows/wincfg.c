/*
 * wincfg.c - the Windows-specific parts of the PuTTY configuration
 * box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

#ifdef PERSOPORT
int get_param( const char * val ) ;
char * get_param_str( const char * val ) ;
#if (defined IMAGEPORT) && (!defined FDJ)
void DisableBackgroundImage( void ) ;
#endif
#endif

static void about_handler(union control *ctrl, void *dlg,
			  void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->generic.context.p;

    if (event == EVENT_ACTION) {
	modal_about_box(*hwndp);
    }
}

static void help_handler(union control *ctrl, void *dlg,
			 void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->generic.context.p;

    if (event == EVENT_ACTION) {
	show_help(*hwndp);
    }
}

static void variable_pitch_handler(union control *ctrl, void *dlg,
                                   void *data, int event)
{
    if (event == EVENT_REFRESH) {
	dlg_checkbox_set(ctrl, dlg, !dlg_get_fixed_pitch_flag(dlg));
    } else if (event == EVENT_VALCHANGE) {
	dlg_set_fixed_pitch_flag(dlg, !dlg_checkbox_get(ctrl, dlg));
    }
}

void win_setup_config_box(struct controlbox *b, HWND *hwndp, int has_help,
			  int midsession, int protocol)
{
    struct controlset *s;
    union control *c;
    char *str;

    if (!midsession) {
	/*
	 * Add the About and Help buttons to the standard panel.
	 */
	s = ctrl_getset(b, "", "", "");
	c = ctrl_pushbutton(s, "About", 'a', HELPCTX(no_help),
			    about_handler, P(hwndp));
	c->generic.column = 0;
	if (has_help) {
	    c = ctrl_pushbutton(s, "Help", 'h', HELPCTX(no_help),
				help_handler, P(hwndp));
#ifdef PERSOPORT
		if( get_param("CONFIGBOXHEIGHT")>7 ) c->generic.column = 0 ; else 
#endif
	    c->generic.column = 1;
	}
    }

    /*
     * Full-screen mode is a Windows peculiarity; hence
     * scrollbar_in_fullscreen is as well.
     */
    s = ctrl_getset(b, "Window", "scrollback",
		    "Control the scrollback in the window");
    ctrl_checkbox(s, "Display scrollbar in full screen mode", 'i',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,scrollbar_in_fullscreen)));
    /*
     * Really this wants to go just after `Display scrollbar'. See
     * if we can find that control, and do some shuffling.
     */
    {
        int i;
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->generic.type == CTRL_CHECKBOX &&
                c->generic.context.i == offsetof(Config,scrollbar)) {
                /*
                 * Control i is the scrollbar checkbox.
                 * Control s->ncontrols-1 is the scrollbar-in-FS one.
                 */
                if (i < s->ncontrols-2) {
                    c = s->ctrls[s->ncontrols-1];
                    memmove(s->ctrls+i+2, s->ctrls+i+1,
                            (s->ncontrols-i-2)*sizeof(union control *));
                    s->ctrls[i+1] = c;
                }
                break;
            }
        }
    }

    /*
     * Windows has the AltGr key, which has various Windows-
     * specific options.
     */
    s = ctrl_getset(b, "Terminal/Keyboard", "features",
		    "Enable extra keyboard features:");
    ctrl_checkbox(s, "AltGr acts as Compose key", 't',
		  HELPCTX(keyboard_compose),
		  dlg_stdcheckbox_handler, I(offsetof(Config,compose_key)));
    ctrl_checkbox(s, "Control-Alt is different from AltGr", 'd',
		  HELPCTX(keyboard_ctrlalt),
		  dlg_stdcheckbox_handler, I(offsetof(Config,ctrlaltkeys)));
#ifdef CYGTERMPORT
    ctrl_checkbox(s, "Set meta bit on alt (instead of escape)", 'm',
		  HELPCTX(no_help),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_metabit)));
#endif
    /*
     * Windows allows an arbitrary .WAV to be played as a bell, and
     * also the use of the PC speaker. For this we must search the
     * existing controlset for the radio-button set controlling the
     * `beep' option, and add extra buttons to it.
     * 
     * Note that although this _looks_ like a hideous hack, it's
     * actually all above board. The well-defined interface to the
     * per-platform dialog box code is the _data structures_ `union
     * control', `struct controlset' and so on; so code like this
     * that reaches into those data structures and changes bits of
     * them is perfectly legitimate and crosses no boundaries. All
     * the ctrl_* routines that create most of the controls are
     * convenient shortcuts provided on the cross-platform side of
     * the interface, and template creation code is under no actual
     * obligation to use them.
     */
    s = ctrl_getset(b, "Terminal/Bell", "style", "Set the style of bell");
    {
	int i;
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == offsetof(Config, beep)) {
		assert(c->generic.handler == dlg_stdradiobutton_handler);
		c->radio.nbuttons += 2;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("Play a custom sound file");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("Beep using the PC speaker");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-1] = I(BELL_WAVEFILE);
		c->radio.buttondata[c->radio.nbuttons-2] = I(BELL_PCSPEAKER);
		if (c->radio.shortcuts) {
		    c->radio.shortcuts =
			sresize(c->radio.shortcuts, c->radio.nbuttons, char);
		    c->radio.shortcuts[c->radio.nbuttons-1] = NO_SHORTCUT;
		    c->radio.shortcuts[c->radio.nbuttons-2] = NO_SHORTCUT;
		}
		break;
	    }
	}
    }
    ctrl_filesel(s, "Custom sound file to play as a bell:", NO_SHORTCUT,
		 FILTER_WAVE_FILES, FALSE, "Select bell sound file",
		 HELPCTX(bell_style),
		 dlg_stdfilesel_handler, I(offsetof(Config, bell_wavefile)));

    /*
     * While we've got this box open, taskbar flashing on a bell is
     * also Windows-specific.
     */
    ctrl_radiobuttons(s, "Taskbar/caption indication on bell:", 'i', 3,
		      HELPCTX(bell_taskbar),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, beep_ind)),
		      "Disabled", I(B_IND_DISABLED),
		      "Flashing", I(B_IND_FLASH),
		      "Steady", I(B_IND_STEADY), NULL);

    /*
     * The sunken-edge border is a Windows GUI feature.
     */
    s = ctrl_getset(b, "Window/Appearance", "border",
		    "Adjust the window border");
    ctrl_checkbox(s, "Sunken-edge border (slightly thicker)", 's',
		  HELPCTX(appearance_border),
		  dlg_stdcheckbox_handler, I(offsetof(Config,sunken_edge)));

    /*
     * Configurable font quality settings for Windows.
     */
    s = ctrl_getset(b, "Window/Appearance", "font",
		    "Font settings");
    ctrl_checkbox(s, "Allow selection of variable-pitch fonts", NO_SHORTCUT,
                  HELPCTX(appearance_font), variable_pitch_handler, I(0));
    ctrl_radiobuttons(s, "Font quality:", 'q', 2,
		      HELPCTX(appearance_font),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, font_quality)),
		      "Antialiased", I(FQ_ANTIALIASED),
		      "Non-Antialiased", I(FQ_NONANTIALIASED),
		      "ClearType", I(FQ_CLEARTYPE),
		      "Default", I(FQ_DEFAULT), NULL);

    /*
     * Cyrillic Lock is a horrid misfeature even on Windows, and
     * the least we can do is ensure it never makes it to any other
     * platform (at least unless someone fixes it!).
     */
    s = ctrl_getset(b, "Window/Translation", "tweaks", NULL);
    ctrl_checkbox(s, "Caps Lock acts as Cyrillic switch", 's',
		  HELPCTX(translation_cyrillic),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,xlat_capslockcyr)));

    /*
     * On Windows we can use but not enumerate translation tables
     * from the operating system. Briefly document this.
     */
    s = ctrl_getset(b, "Window/Translation", "trans",
		    "Character set translation on received data");
    ctrl_text(s, "(Codepages supported by Windows but not listed here, "
	      "such as CP866 on many systems, can be entered manually)",
	      HELPCTX(translation_codepage));

    /*
     * Windows has the weird OEM font mode, which gives us some
     * additional options when working with line-drawing
     * characters.
     */
    str = dupprintf("Adjust how %s displays line drawing characters", appname);
    s = ctrl_getset(b, "Window/Translation", "linedraw", str);
    sfree(str);
    {
	int i;
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == offsetof(Config, vtmode)) {
		assert(c->generic.handler == dlg_stdradiobutton_handler);
		c->radio.nbuttons += 3;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-3] =
		    dupstr("Font has XWindows encoding");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("Use font in both ANSI and OEM modes");
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("Use font in OEM mode only");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-3] = I(VT_XWINDOWS);
		c->radio.buttondata[c->radio.nbuttons-2] = I(VT_OEMANSI);
		c->radio.buttondata[c->radio.nbuttons-1] = I(VT_OEMONLY);
		if (!c->radio.shortcuts) {
		    int j;
		    c->radio.shortcuts = snewn(c->radio.nbuttons, char);
		    for (j = 0; j < c->radio.nbuttons; j++)
			c->radio.shortcuts[j] = NO_SHORTCUT;
		} else {
		    c->radio.shortcuts = sresize(c->radio.shortcuts,
						 c->radio.nbuttons, char);
		}
		c->radio.shortcuts[c->radio.nbuttons-3] = 'x';
		c->radio.shortcuts[c->radio.nbuttons-2] = 'b';
		c->radio.shortcuts[c->radio.nbuttons-1] = 'e';
		break;
	    }
	}
    }

    /*
     * RTF paste is Windows-specific.
     */
    s = ctrl_getset(b, "Window/Selection", "format",
		    "Formatting of pasted characters");
    ctrl_checkbox(s, "Paste to clipboard in RTF as well as plain text", 'f',
		  HELPCTX(selection_rtf),
		  dlg_stdcheckbox_handler, I(offsetof(Config,rtf_paste)));

    /*
     * Windows often has no middle button, so we supply a selection
     * mode in which the more critical Paste action is available on
     * the right button instead.
     */
    s = ctrl_getset(b, "Window/Selection", "mouse",
		    "Control use of mouse");
    ctrl_radiobuttons(s, "Action of mouse buttons:", 'm', 1,
		      HELPCTX(selection_buttons),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, mouse_is_xterm)),
		      "Windows (Middle extends, Right brings up menu)", I(2),
		      "Compromise (Middle extends, Right pastes)", I(0),
		      "xterm (Right extends, Middle pastes)", I(1), NULL);
    /*
     * This really ought to go at the _top_ of its box, not the
     * bottom, so we'll just do some shuffling now we've set it
     * up...
     */
    c = s->ctrls[s->ncontrols-1];      /* this should be the new control */
    memmove(s->ctrls+1, s->ctrls, (s->ncontrols-1)*sizeof(union control *));
    s->ctrls[0] = c;

    /*
     * Logical palettes don't even make sense anywhere except Windows.
     */
    s = ctrl_getset(b, "Window/Colours", "general",
		    "General options for colour usage");
    ctrl_checkbox(s, "Attempt to use logical palettes", 'l',
		  HELPCTX(colours_logpal),
		  dlg_stdcheckbox_handler, I(offsetof(Config,try_palette)));
    ctrl_checkbox(s, "Use system colours", 's',
                  HELPCTX(colours_system),
                  dlg_stdcheckbox_handler, I(offsetof(Config,system_colour)));


    /*
     * Resize-by-changing-font is a Windows insanity.
     */
    s = ctrl_getset(b, "Window", "size", "Set the size of the window");
    ctrl_radiobuttons(s, "When window is resized:", 'z', 1,
		      HELPCTX(window_resize),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, resize_action)),
		      "Change the number of rows and columns", I(RESIZE_TERM),
		      "Change the size of the font", I(RESIZE_FONT),
		      "Change font size only when maximised", I(RESIZE_EITHER),
		      "Forbid resizing completely", I(RESIZE_DISABLED), NULL);

    /*
     * Most of the Window/Behaviour stuff is there to mimic Windows
     * conventions which PuTTY can optionally disregard. Hence,
     * most of these options are Windows-specific.
     */
    s = ctrl_getset(b, "Window/Behaviour", "main", NULL);
    ctrl_checkbox(s, "Window closes on ALT-F4", '4',
		  HELPCTX(behaviour_altf4),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_f4)));
    ctrl_checkbox(s, "System menu appears on ALT-Space", 'y',
		  HELPCTX(behaviour_altspace),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_space)));
    ctrl_checkbox(s, "System menu appears on ALT alone", 'l',
		  HELPCTX(behaviour_altonly),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_only)));
    ctrl_checkbox(s, "Ensure window is always on top", 'e',
		  HELPCTX(behaviour_alwaysontop),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alwaysontop)));
#ifdef PERSOPORT
	if( !get_param("PUTTY") ) {
		ctrl_checkbox(s, "Send to tray on startup", '`',
		  HELPCTX(behaviour_sendtotray),
		  dlg_stdcheckbox_handler, I(offsetof(Config,sendtotray)));
		ctrl_checkbox(s, "Save position and size on exit", '=',
		  HELPCTX(behaviour_saveonexit),
		  dlg_stdcheckbox_handler, I(offsetof(Config,saveonexit)));
		}
#endif
    ctrl_checkbox(s, "Full screen on Alt-Enter", 'f',
		  HELPCTX(behaviour_altenter),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,fullscreenonaltenter)));

#ifdef HYPERLINKPORT
	/*
	 * HACK: PuttyTray / Nutty
	 * Hyperlink stuff: The Window/Hyperlinks panel.
	 */
	if( !get_param("PUTTY") ) {
	ctrl_settitle(b, "Window/Hyperlinks", "Options controlling behaviour of hyperlinks");
	s = ctrl_getset(b, "Window/Hyperlinks", "general", "General options for hyperlinks");

	ctrl_radiobuttons(s, "Underline hyperlinks:", NO_SHORTCUT, 1,
			  HELPCTX(no_help),
			  dlg_stdradiobutton_handler,
 			  I(offsetof(Config, url_underline)),
			  "Always", NO_SHORTCUT, I(URLHACK_UNDERLINE_ALWAYS),
			  "When hovered upon", NO_SHORTCUT, I(URLHACK_UNDERLINE_HOVER),
			  "Never", NO_SHORTCUT, I(URLHACK_UNDERLINE_NEVER),
			  NULL);

	ctrl_checkbox(s, "Use ctrl+click to launch hyperlinks", 'l',
		  HELPCTX(no_help),
		  dlg_stdcheckbox_handler, I(offsetof(Config,url_ctrl_click)));

	s = ctrl_getset(b, "Window/Hyperlinks", "browser", "Browser application");

	ctrl_checkbox(s, "Use the default browser", 'b',
		  HELPCTX(no_help),
		  dlg_stdcheckbox_handler, I(offsetof(Config,url_defbrowser)));

	ctrl_filesel(s, "or specify an application to open hyperlinks with:", 's',
		"Application (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0", TRUE,
		"Select executable to open hyperlinks with", HELPCTX(no_help),
		 dlg_stdfilesel_handler, I(offsetof(Config, url_browser)));

	s = ctrl_getset(b, "Window/Hyperlinks", "regexp", "Regular expression");

	ctrl_checkbox(s, "Use the default regular expression", 'r',
		  HELPCTX(no_help),
		  dlg_stdcheckbox_handler, I(offsetof(Config,url_defregex)));

	ctrl_editbox(s, "or specify your own:", NO_SHORTCUT, 100,
		 HELPCTX(no_help),
		 dlg_stdeditbox_handler, I(offsetof(Config,url_regex)),
		 I(sizeof(((Config *)0)->url_regex)));

	ctrl_text(s, "The single white space will be cropped in front of the link, if exists.",
		  HELPCTX(no_help));
	}
#endif
    /*
     * Windows supports a local-command proxy. This also means we
     * must adjust the text on the `Telnet command' control.
     */
    if (!midsession) {
	int i;
        s = ctrl_getset(b, "Connection/Proxy", "basics", NULL);
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == offsetof(Config, proxy_type)) {
		assert(c->generic.handler == dlg_stdradiobutton_handler);
		c->radio.nbuttons++;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("Local");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-1] = I(PROXY_CMD);
		break;
	    }
	}

	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_EDITBOX &&
		c->generic.context.i ==
		offsetof(Config, proxy_telnet_command)) {
		assert(c->generic.handler == dlg_stdeditbox_handler);
		sfree(c->generic.label);
		c->generic.label = dupstr("Telnet command, or local"
					  " proxy command");
		break;
	    }
	}
    }

    /*
     * Serial back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_SERIAL))
        ser_setup_config_box(b, midsession, 0x1F, 0x0F);

    /*
     * $XAUTHORITY is not reliable on Windows, so we provide a
     * means to override it.
     */
    if (!midsession && backend_from_proto(PROT_SSH)) {
	s = ctrl_getset(b, "Connection/SSH/X11", "x11", "X11 forwarding");
	ctrl_filesel(s, "X authority file for local display", 't',
		     NULL, FALSE, "Select X authority file",
		     HELPCTX(ssh_tunnels_xauthority),
		     dlg_stdfilesel_handler, I(offsetof(Config, xauthfile)));
    }
#ifdef CYGTERMPORT
    /*
     * cygterm back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_CYGTERM))
        cygterm_setup_config_box(b, midsession);
#endif
#ifdef PERSOPORT
	if( !get_param("PUTTY") && 0 ) {

	ctrl_settitle(b, get_param_str("NAME"), "Specific program options");
	s = ctrl_getset(b, get_param_str("NAME"), "general", "PATH definitions");

	ctrl_filesel(s, "Full path to scp", 's',
		"Application (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0", TRUE,
		"Select executable to scp/sftp transfer", HELPCTX(no_help),
		 dlg_stdfilesel_handler, I(offsetof(Config, url_browser)));
		
	//ctrl_checkbox(s, "Use the default browser", 'b',
	//	  HELPCTX(no_help),
	//	  dlg_stdcheckbox_handler, I(offsetof(Config,url_defbrowser)));

	//ctrl_text(s, "The single white space will be cropped in front of the link, if exists.",
	//	  HELPCTX(no_help));
	}

#if (defined IMAGEPORT) && (!defined FDJ)
	/* Le patch Background image ne marche plus bien sur la version PuTTY 0.61
		- il est en erreur lorsqu'on passe par la config box
		- il est ok lorsqu'on d�marrer par -load ou par duplicate session
	   On le d�sactive dans la config box
	*/
	// DisableBackgroundImage() ;
	/* Un fix a �t� appliqu� dans CONFIG.C. Lorsqu'on clique sur Open ou qu'on double-clique on charge la session par -load.
	*/
#endif
#endif
}
