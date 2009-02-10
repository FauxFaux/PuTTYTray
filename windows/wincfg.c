/*
 * wincfg.c - the Windows-specific parts of the PuTTY configuration
 * box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

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
	c = ctrl_pushbutton(s, "도움", 'a', HELPCTX(no_help),
			    about_handler, P(hwndp));
	c->generic.column = 0;
	if (has_help) {
	    c = ctrl_pushbutton(s, "도움 (H)", 'h', HELPCTX(no_help),
				help_handler, P(hwndp));
	    c->generic.column = 1;
	}
    }

    /*
     * Full-screen mode is a Windows peculiarity; hence
     * scrollbar_in_fullscreen is as well.
     */
    s = ctrl_getset(b, "창", "scrollback",
		    "창 이전 내용 올려보기 설정");
    ctrl_checkbox(s, "전화면 모드에서 스크롤 바 보임 (I)", 'i',
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
    s = ctrl_getset(b, "터미널/키보드", "features",
		    "키보드 부가 기능:");
    ctrl_checkbox(s, "회색 Alt를 조합 키로 사용 (T)", 't',
		  HELPCTX(keyboard_compose),
		  dlg_stdcheckbox_handler, I(offsetof(Config,compose_key)));
    ctrl_checkbox(s, "Ctrl-Alt를 회색 Alt와 구분 (D)", 'd',
		  HELPCTX(keyboard_ctrlalt),
		  dlg_stdcheckbox_handler, I(offsetof(Config,ctrlaltkeys)));

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
    s = ctrl_getset(b, "터미널/벨", "style", "벨 스타일");
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
		    dupstr("다른 사운드 파일 사용");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("PC 스피커를 이용해서 벨소리 냄");
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
    ctrl_filesel(s, "벨 소리로 사용할 소리 파일:", NO_SHORTCUT,
		 FILTER_WAVE_FILES, FALSE, "벨 소리 파일 선택",
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
    s = ctrl_getset(b, "창/모양", "border",
		    "창틀 조절");
    ctrl_checkbox(s, "튀어나온 창틀 (좀 더 두터움) (S)", 's',
		  HELPCTX(appearance_border),
		  dlg_stdcheckbox_handler, I(offsetof(Config,sunken_edge)));

    /*
     * Configurable font quality settings for Windows.
     */
    s = ctrl_getset(b, "창/모양", "font",
		    "글꼴 설정");
    ctrl_radiobuttons(s, "글꼴 품질 (Q):", 'q', 2,
		      HELPCTX(appearance_font),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, font_quality)),
		      "Antialiased", I(FQ_ANTIALIASED),
		      "Non-Antialiased", I(FQ_NONANTIALIASED),
		      "ClearType", I(FQ_CLEARTYPE),
		      "기본", I(FQ_DEFAULT), NULL);

    /*
     * Cyrillic Lock is a horrid misfeature even on Windows, and
     * the least we can do is ensure it never makes it to any other
     * platform (at least unless someone fixes it!).
     */
    s = ctrl_getset(b, "창/변환", "tweaks", NULL);
    ctrl_checkbox(s, "CapsLock을 키릴 전환키로 사용", 's',
		  HELPCTX(translation_cyrillic),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,xlat_capslockcyr)));

    /*
     * On Windows we can use but not enumerate translation tables
     * from the operating system. Briefly document this.
     */
    s = ctrl_getset(b, "창/변환", "trans",
		    "수신 데이터의 문자 셋 변환");
    ctrl_text(s, "(여기에 없지만 윈도우가 지원하는 CP866 같은 인코딩은 직접 입력하면 쓸 수 있습니다.)",
	      HELPCTX(translation_codepage));

    /*
     * Windows has the weird OEM font mode, which gives us some
     * additional options when working with line-drawing
     * characters.
     */
    str = dupprintf("%s가 선그림 문자를 출력하는 방법", appname);
    s = ctrl_getset(b, "창/변환", "linedraw", str);
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
		    dupstr("X윈도우 인코딩사용 (X)");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("ANSI와 OEM모드 모두 사용 (B)");
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("글꼴을 OEM 모드로만 사용");
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
    s = ctrl_getset(b, "창/선택", "format",
		    "붙여넣기 글자 변환");
    ctrl_checkbox(s, "클립보드로 텍스트와 RTF를 동시에 복사", 'f',
		  HELPCTX(selection_rtf),
		  dlg_stdcheckbox_handler, I(offsetof(Config,rtf_paste)));

    /*
     * Windows often has no middle button, so we supply a selection
     * mode in which the more critical Paste action is available on
     * the right button instead.
     */
    s = ctrl_getset(b, "창/선택", "mouse",
		    "마우스 설정");
    ctrl_radiobuttons(s, "마우스 버튼 특성:", 'm', 1,
		      HELPCTX(selection_buttons),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, mouse_is_xterm)),
		      "윈도우 (휠 확장, 오른쪽 메뉴 띄움)", I(2),
		      "어중간 (휠 확장, 오른쪽 붙여넣기)", I(0),
		      "xterm (오른쪽 확장, 휠 붙여넣기)", I(1), NULL);
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
    s = ctrl_getset(b, "창/색깔", "general",
		    "색깔관련 일반 옵션");
    ctrl_checkbox(s, "논리 팔레트 사용을 시도 (L)", 'l',
		  HELPCTX(colours_logpal),
		  dlg_stdcheckbox_handler, I(offsetof(Config,try_palette)));
    ctrl_checkbox(s, "시스템 컬러 씀 (S)", 's',
                  HELPCTX(colours_system),
                  dlg_stdcheckbox_handler, I(offsetof(Config,system_colour)));


    /*
     * Resize-by-changing-font is a Windows insanity.
     */
    s = ctrl_getset(b, "창", "size", "창 크기 조절");
    ctrl_radiobuttons(s, "창 크기가 바뀌었을 때 (Z):", 'z', 1,
		      HELPCTX(window_resize),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, resize_action)),
		      "가로 세로 크기 조절", I(RESIZE_TERM),
		      "글꼴 크기를 조절", I(RESIZE_FONT),
		      "최대화 되었을 때에만 글꼴 크기 조정", I(RESIZE_EITHER),
		      "전혀 크기 조절 하지 않음", I(RESIZE_DISABLED), NULL);

    /*
     * Most of the Window/Behaviour stuff is there to mimic Windows
     * conventions which PuTTY can optionally disregard. Hence,
     * most of these options are Windows-specific.
     */
    s = ctrl_getset(b, "창/특성", "main", NULL);
    ctrl_checkbox(s, "Alt-F4를 누르면 창을 닫음", '4',
		  HELPCTX(behaviour_altf4),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_f4)));
    ctrl_checkbox(s, "ALT-스페이스를 누르면 시스템 메뉴 나옴 (Y)", 'y',
		  HELPCTX(behaviour_altspace),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_space)));
    ctrl_checkbox(s, "ALT만 누르면 시스템 메뉴 나옴 (L)", 'l',
		  HELPCTX(behaviour_altonly),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alt_only)));
    ctrl_checkbox(s, "창 항상 맨 위에 (E)", 'e',
		  HELPCTX(behaviour_alwaysontop),
		  dlg_stdcheckbox_handler, I(offsetof(Config,alwaysontop)));
    ctrl_checkbox(s, "Alt-Enter를 누르면 전체 화면으로 전환 (F)", 'f',
		  HELPCTX(behaviour_altenter),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,fullscreenonaltenter)));

    /*
     * Windows supports a local-command proxy. This also means we
     * must adjust the text on the `Telnet command' control.
     */
    if (!midsession) {
	int i;
        s = ctrl_getset(b, "접속/프락시", "basics", NULL);
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
		c->generic.label = dupstr("텔넷 명령어나 로컬"
					  " 프락시 명령어 (M)");
		break;
	    }
	}
    }

    /*
     * Serial back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_SERIAL))
        ser_setup_config_box(b, midsession, 0x1F, 0x0F);
}
