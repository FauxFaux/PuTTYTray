/*
 * winctrls.c: routines to self-manage the controls in a dialog
 * box.
 */

#include <windows.h>
#include <commctrl.h>

#include "winstuff.h"

#define GAPBETWEEN 3
#define GAPWITHIN 1
#define GAPXBOX 7
#define GAPYBOX 4
#define DLGWIDTH 168
#define STATICHEIGHT 8
#define CHECKBOXHEIGHT 8
#define RADIOHEIGHT 8
#define EDITHEIGHT 12
#define COMBOHEIGHT 12
#define PUSHBTNHEIGHT 14
#define PROGBARHEIGHT 14

void ctlposinit(struct ctlpos *cp,
                HWND hwnd,
                int leftborder,
                int rightborder,
                int topborder)
{
  RECT r, r2;
  cp->hwnd = hwnd;
  cp->font = SendMessage(hwnd, WM_GETFONT, 0, 0);
  cp->ypos = topborder;
  GetClientRect(hwnd, &r);
  r2.left = r2.top = 0;
  r2.right = 4;
  r2.bottom = 8;
  MapDialogRect(hwnd, &r2);
  cp->dlu4inpix = r2.right;
  cp->width = (r.right * 4) / (r2.right) - 2 * GAPBETWEEN;
  cp->xoff = leftborder;
  cp->width -= leftborder + rightborder;
}

void doctl(struct ctlpos *cp,
           RECT r,
           char *wclass,
           int wstyle,
           int exstyle,
           char *wtext,
           int wid)
{
  HWND ctl;
  /*
   * Note nonstandard use of RECT. This is deliberate: by
   * transforming the width and height directly we arrange to
   * have all supposedly same-sized controls really same-sized.
   */

  r.left += cp->xoff;
  MapDialogRect(cp->hwnd, &r);

  ctl = CreateWindowEx(exstyle,
                       wclass,
                       wtext,
                       wstyle,
                       r.left,
                       r.top,
                       r.right,
                       r.bottom,
                       cp->hwnd,
                       (HMENU)wid,
                       hinst,
                       NULL);
  SendMessage(ctl, WM_SETFONT, cp->font, MAKELPARAM(TRUE, 0));
}

/*
 * A title bar across the top of a sub-dialog.
 */
void bartitle(struct ctlpos *cp, char *name, int id)
{
  RECT r;

  r.left = GAPBETWEEN;
  r.right = cp->width;
  r.top = cp->ypos;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPBETWEEN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, name, id);
}

/*
 * Begin a grouping box, with or without a group title.
 */
void beginbox(struct ctlpos *cp, char *name, int idbox, int idtext)
{
  if (name)
    cp->ypos += STATICHEIGHT / 2;
  cp->boxystart = cp->ypos;
  if (name)
    cp->ypos += STATICHEIGHT - (STATICHEIGHT / 2);
  cp->ypos += GAPYBOX;
  cp->width -= 2 * GAPXBOX;
  cp->xoff += GAPXBOX;
  cp->boxid = idbox;
  cp->boxtextid = idtext;
  cp->boxtext = name;
}

/*
 * End a grouping box.
 */
void endbox(struct ctlpos *cp)
{
  RECT r;
  cp->xoff -= GAPXBOX;
  cp->width += 2 * GAPXBOX;
  cp->ypos += GAPYBOX - GAPBETWEEN;
  r.left = GAPBETWEEN;
  r.right = cp->width;
  r.top = cp->boxystart;
  r.bottom = cp->ypos - cp->boxystart;
  doctl(cp,
        r,
        "STATIC",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
        0,
        "",
        cp->boxid);
  if (cp->boxtext) {
    SIZE s;
    HDC hdc;
    HFONT oldfont, dlgfont;
    hdc = GetDC(cp->hwnd);
    dlgfont = (HFONT)cp->font;
    oldfont = SelectObject(hdc, dlgfont);
    GetTextExtentPoint32(hdc, cp->boxtext, strlen(cp->boxtext), &s);
    SelectObject(hdc, oldfont);
    DeleteDC(hdc);
    r.left = GAPXBOX + GAPBETWEEN;
    r.right = (s.cx * 4 + cp->dlu4inpix - 1) / cp->dlu4inpix;

    r.top = cp->boxystart - STATICHEIGHT / 2;
    r.bottom = STATICHEIGHT;
    doctl(
        cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, cp->boxtext, cp->boxtextid);
  }
  cp->ypos += GAPYBOX;
}

/*
 * Some edit boxes. Each one has a static above it. The percentages
 * of the horizontal space are provided.
 */
void multiedit(struct ctlpos *cp, ...)
{
  RECT r;
  va_list ap;
  int percent, xpos;

  percent = xpos = 0;
  va_start(ap, cp);
  while (1) {
    char *text;
    int staticid, editid, pcwidth;
    text = va_arg(ap, char *);
    if (!text)
      break;
    staticid = va_arg(ap, int);
    editid = va_arg(ap, int);
    pcwidth = va_arg(ap, int);

    r.left = xpos + GAPBETWEEN;
    percent += pcwidth;
    xpos = (cp->width + GAPBETWEEN) * percent / 100;
    r.right = xpos - r.left;

    r.top = cp->ypos;
    r.bottom = STATICHEIGHT;
    doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, text, staticid);
    r.top = cp->ypos + 8 + GAPWITHIN;
    r.bottom = EDITHEIGHT;
    doctl(cp,
          r,
          "EDIT",
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
          WS_EX_CLIENTEDGE,
          "",
          editid);
  }
  va_end(ap);
  cp->ypos += 8 + GAPWITHIN + 12 + GAPBETWEEN;
}

/*
 * A set of radio buttons on the same line, with a static above
 * them. `nacross' dictates how many parts the line is divided into
 * (you might want this not to equal the number of buttons if you
 * needed to line up some 2s and some 3s to look good in the same
 * panel).
 */
void radioline(struct ctlpos *cp, char *text, int id, int nacross, ...)
{
  RECT r;
  va_list ap;
  int group;
  int i;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, text, id);
  va_start(ap, nacross);
  group = WS_GROUP;
  i = 0;
  while (1) {
    char *btext;
    int bid;
    btext = va_arg(ap, char *);
    if (!btext)
      break;
    bid = va_arg(ap, int);
    r.left = GAPBETWEEN + i * (cp->width + GAPBETWEEN) / nacross;
    r.right = (i + 1) * (cp->width + GAPBETWEEN) / nacross - r.left;
    r.top = cp->ypos;
    r.bottom = RADIOHEIGHT;
    doctl(cp,
          r,
          "BUTTON",
          BS_AUTORADIOBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP | group,
          0,
          btext,
          bid);
    group = 0;
    i++;
  }
  va_end(ap);
  cp->ypos += r.bottom + GAPBETWEEN;
}

/*
 * A set of radio buttons on multiple lines, with a static above
 * them.
 */
void radiobig(struct ctlpos *cp, char *text, int id, ...)
{
  RECT r;
  va_list ap;
  int group;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, text, id);
  va_start(ap, id);
  group = WS_GROUP;
  while (1) {
    char *btext;
    int bid;
    btext = va_arg(ap, char *);
    if (!btext)
      break;
    bid = va_arg(ap, int);
    r.left = GAPBETWEEN;
    r.top = cp->ypos;
    r.right = cp->width;
    r.bottom = STATICHEIGHT;
    cp->ypos += r.bottom + GAPWITHIN;
    doctl(cp,
          r,
          "BUTTON",
          BS_AUTORADIOBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP | group,
          0,
          btext,
          bid);
    group = 0;
  }
  va_end(ap);
  cp->ypos += GAPBETWEEN - GAPWITHIN;
}

/*
 * A single standalone checkbox.
 */
void checkbox(struct ctlpos *cp, char *text, int id)
{
  RECT r;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = CHECKBOXHEIGHT;
  cp->ypos += r.bottom + GAPBETWEEN;
  doctl(cp,
        r,
        "BUTTON",
        BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0,
        text,
        id);
}

/*
 * A single standalone static text control.
 */
void statictext(struct ctlpos *cp, char *text, int id)
{
  RECT r;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPBETWEEN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, text, id);
}

/*
 * A button on the right hand side, with a static to its left.
 */
void staticbtn(struct ctlpos *cp, char *stext, int sid, char *btext, int bid)
{
  const int height =
      (PUSHBTNHEIGHT > STATICHEIGHT ? PUSHBTNHEIGHT : STATICHEIGHT);
  RECT r;
  int lwid, rwid, rpos;

  rpos = GAPBETWEEN + 3 * (cp->width + GAPBETWEEN) / 4;
  lwid = rpos - 2 * GAPBETWEEN;
  rwid = cp->width + GAPBETWEEN - rpos;

  r.left = GAPBETWEEN;
  r.top = cp->ypos + (height - STATICHEIGHT) / 2;
  r.right = lwid;
  r.bottom = STATICHEIGHT;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  r.left = rpos;
  r.top = cp->ypos + (height - PUSHBTNHEIGHT) / 2;
  r.right = rwid;
  r.bottom = PUSHBTNHEIGHT;
  doctl(cp,
        r,
        "BUTTON",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        btext,
        bid);

  cp->ypos += height + GAPBETWEEN;
}

/*
 * An edit control on the right hand side, with a static to its left.
 */
static void staticedit_internal(struct ctlpos *cp,
                                char *stext,
                                int sid,
                                int eid,
                                int percentedit,
                                int style)
{
  const int height = (EDITHEIGHT > STATICHEIGHT ? EDITHEIGHT : STATICHEIGHT);
  RECT r;
  int lwid, rwid, rpos;

  rpos = GAPBETWEEN + (100 - percentedit) * (cp->width + GAPBETWEEN) / 100;
  lwid = rpos - 2 * GAPBETWEEN;
  rwid = cp->width + GAPBETWEEN - rpos;

  r.left = GAPBETWEEN;
  r.top = cp->ypos + (height - STATICHEIGHT) / 2;
  r.right = lwid;
  r.bottom = STATICHEIGHT;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  r.left = rpos;
  r.top = cp->ypos + (height - EDITHEIGHT) / 2;
  r.right = rwid;
  r.bottom = EDITHEIGHT;
  doctl(cp,
        r,
        "EDIT",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | style,
        WS_EX_CLIENTEDGE,
        "",
        eid);

  cp->ypos += height + GAPBETWEEN;
}

void staticedit(
    struct ctlpos *cp, char *stext, int sid, int eid, int percentedit)
{
  staticedit_internal(cp, stext, sid, eid, percentedit, 0);
}

void staticpassedit(
    struct ctlpos *cp, char *stext, int sid, int eid, int percentedit)
{
  staticedit_internal(cp, stext, sid, eid, percentedit, ES_PASSWORD);
}

/*
 * A big multiline edit control with a static labelling it.
 */
void bigeditctrl(struct ctlpos *cp, char *stext, int sid, int eid, int lines)
{
  RECT r;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = EDITHEIGHT + (lines - 1) * STATICHEIGHT;
  cp->ypos += r.bottom + GAPBETWEEN;
  doctl(cp,
        r,
        "EDIT",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE,
        WS_EX_CLIENTEDGE,
        "",
        eid);
}

/*
 * A tab-control substitute when a real tab control is unavailable.
 */
void ersatztab(struct ctlpos *cp, char *stext, int sid, int lid, int s2id)
{
  const int height = (COMBOHEIGHT > STATICHEIGHT ? COMBOHEIGHT : STATICHEIGHT);
  RECT r;
  int bigwid, lwid, rwid, rpos;
  static const int BIGGAP = 15;
  static const int MEDGAP = 3;

  bigwid = cp->width + 2 * GAPBETWEEN - 2 * BIGGAP;
  cp->ypos += MEDGAP;
  rpos = BIGGAP + (bigwid + BIGGAP) / 2;
  lwid = rpos - 2 * BIGGAP;
  rwid = bigwid + BIGGAP - rpos;

  r.left = BIGGAP;
  r.top = cp->ypos + (height - STATICHEIGHT) / 2;
  r.right = lwid;
  r.bottom = STATICHEIGHT;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  r.left = rpos;
  r.top = cp->ypos + (height - COMBOHEIGHT) / 2;
  r.right = rwid;
  r.bottom = COMBOHEIGHT * 10;
  doctl(cp,
        r,
        "COMBOBOX",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        WS_EX_CLIENTEDGE,
        "",
        lid);

  cp->ypos += height + MEDGAP + GAPBETWEEN;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = 2;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 0, "", s2id);
}

/*
 * A static line, followed by an edit control on the left hand side
 * and a button on the right.
 */
void editbutton(
    struct ctlpos *cp, char *stext, int sid, int eid, char *btext, int bid)
{
  const int height = (EDITHEIGHT > PUSHBTNHEIGHT ? EDITHEIGHT : PUSHBTNHEIGHT);
  RECT r;
  int lwid, rwid, rpos;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  rpos = GAPBETWEEN + 3 * (cp->width + GAPBETWEEN) / 4;
  lwid = rpos - 2 * GAPBETWEEN;
  rwid = cp->width + GAPBETWEEN - rpos;

  r.left = GAPBETWEEN;
  r.top = cp->ypos + (height - EDITHEIGHT) / 2;
  r.right = lwid;
  r.bottom = EDITHEIGHT;
  doctl(cp,
        r,
        "EDIT",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        WS_EX_CLIENTEDGE,
        "",
        eid);

  r.left = rpos;
  r.top = cp->ypos + (height - PUSHBTNHEIGHT) / 2;
  r.right = rwid;
  r.bottom = PUSHBTNHEIGHT;
  doctl(cp,
        r,
        "BUTTON",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        btext,
        bid);

  cp->ypos += height + GAPBETWEEN;
}

/*
 * Special control which was hard to describe generically: the
 * session-saver assembly. A static; below that an edit box; below
 * that a list box. To the right of the list box, a column of
 * buttons.
 */
void sesssaver(
    struct ctlpos *cp, char *text, int staticid, int editid, int listid, ...)
{
  RECT r;
  va_list ap;
  int lwid, rwid, rpos;
  int y;
  const int LISTDEFHEIGHT = 66;

  rpos = GAPBETWEEN + 3 * (cp->width + GAPBETWEEN) / 4;
  lwid = rpos - 2 * GAPBETWEEN;
  rwid = cp->width + GAPBETWEEN - rpos;

  /* The static control. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = lwid;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, text, staticid);

  /* The edit control. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = lwid;
  r.bottom = EDITHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp,
        r,
        "EDIT",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        WS_EX_CLIENTEDGE,
        "",
        editid);

  /*
   * The buttons (we should hold off on the list box until we
   * know how big the buttons are).
   */
  va_start(ap, listid);
  y = cp->ypos;
  while (1) {
    char *btext = va_arg(ap, char *);
    int bid;
    if (!btext)
      break;
    bid = va_arg(ap, int);
    r.left = rpos;
    r.top = y;
    r.right = rwid;
    r.bottom = PUSHBTNHEIGHT;
    y += r.bottom + GAPWITHIN;
    doctl(cp,
          r,
          "BUTTON",
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
          0,
          btext,
          bid);
  }

  /* Compute list box height. LISTDEFHEIGHT, or height of buttons. */
  y -= cp->ypos;
  y -= GAPWITHIN;
  if (y < LISTDEFHEIGHT)
    y = LISTDEFHEIGHT;
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = lwid;
  r.bottom = y;
  cp->ypos += y + GAPBETWEEN;
  doctl(cp,
        r,
        "LISTBOX",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY |
            LBS_HASSTRINGS,
        WS_EX_CLIENTEDGE,
        "",
        listid);
}

/*
 * Another special control: the environment-variable setter. A
 * static line first; then a pair of edit boxes with associated
 * statics, and two buttons; then a list box.
 */
void envsetter(struct ctlpos *cp,
               char *stext,
               int sid,
               char *e1stext,
               int e1sid,
               int e1id,
               char *e2stext,
               int e2sid,
               int e2id,
               int listid,
               char *b1text,
               int b1id,
               char *b2text,
               int b2id)
{
  RECT r;
  const int height =
      (STATICHEIGHT > EDITHEIGHT && STATICHEIGHT > PUSHBTNHEIGHT
           ? STATICHEIGHT
           : EDITHEIGHT > PUSHBTNHEIGHT ? EDITHEIGHT : PUSHBTNHEIGHT);
  const static int percents[] = {20, 35, 10, 25};
  int i, j, xpos, percent;
  const int LISTHEIGHT = 42;

  /* The static control. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  /* The statics+edits+buttons. */
  for (j = 0; j < 2; j++) {
    percent = 10;
    for (i = 0; i < 4; i++) {
      xpos = (cp->width + GAPBETWEEN) * percent / 100;
      r.left = xpos + GAPBETWEEN;
      percent += percents[i];
      xpos = (cp->width + GAPBETWEEN) * percent / 100;
      r.right = xpos - r.left;
      r.top = cp->ypos;
      r.bottom = (i == 0 ? STATICHEIGHT : i == 1 ? EDITHEIGHT : PUSHBTNHEIGHT);
      r.top += (height - r.bottom) / 2;
      if (i == 0) {
        doctl(cp,
              r,
              "STATIC",
              WS_CHILD | WS_VISIBLE,
              0,
              j == 0 ? e1stext : e2stext,
              j == 0 ? e1sid : e2sid);
      } else if (i == 1) {
        doctl(cp,
              r,
              "EDIT",
              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
              WS_EX_CLIENTEDGE,
              "",
              j == 0 ? e1id : e2id);
      } else if (i == 3) {
        doctl(cp,
              r,
              "BUTTON",
              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
              0,
              j == 0 ? b1text : b2text,
              j == 0 ? b1id : b2id);
      }
    }
    cp->ypos += height + GAPWITHIN;
  }

  /* The list box. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = LISTHEIGHT;
  cp->ypos += r.bottom + GAPBETWEEN;
  doctl(cp,
        r,
        "LISTBOX",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_HASSTRINGS |
            LBS_USETABSTOPS,
        WS_EX_CLIENTEDGE,
        "",
        listid);
}

/*
 * Yet another special control: the character-class setter. A
 * static, then a list, then a line containing a
 * button-and-static-and-edit.
 */
void charclass(struct ctlpos *cp,
               char *stext,
               int sid,
               int listid,
               char *btext,
               int bid,
               int eid,
               char *s2text,
               int s2id)
{
  RECT r;
  const int height =
      (STATICHEIGHT > EDITHEIGHT && STATICHEIGHT > PUSHBTNHEIGHT
           ? STATICHEIGHT
           : EDITHEIGHT > PUSHBTNHEIGHT ? EDITHEIGHT : PUSHBTNHEIGHT);
  const static int percents[] = {30, 40, 30};
  int i, xpos, percent;
  const int LISTHEIGHT = 66;

  /* The static control. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  /* The list box. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = LISTHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp,
        r,
        "LISTBOX",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_HASSTRINGS |
            LBS_USETABSTOPS,
        WS_EX_CLIENTEDGE,
        "",
        listid);

  /* The button+static+edit. */
  percent = xpos = 0;
  for (i = 0; i < 3; i++) {
    r.left = xpos + GAPBETWEEN;
    percent += percents[i];
    xpos = (cp->width + GAPBETWEEN) * percent / 100;
    r.right = xpos - r.left;
    r.top = cp->ypos;
    r.bottom = (i == 0 ? PUSHBTNHEIGHT : i == 1 ? STATICHEIGHT : EDITHEIGHT);
    r.top += (height - r.bottom) / 2;
    if (i == 0) {
      doctl(cp,
            r,
            "BUTTON",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            btext,
            bid);
    } else if (i == 1) {
      doctl(
          cp, r, "STATIC", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, s2text, s2id);
    } else if (i == 2) {
      doctl(cp,
            r,
            "EDIT",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            WS_EX_CLIENTEDGE,
            "",
            eid);
    }
  }
  cp->ypos += height + GAPBETWEEN;
}

/*
 * A special control (horrors!). The colour editor. A static line;
 * then on the left, a list box, and on the right, a sequence of
 * two-part statics followed by a button.
 */
void colouredit(struct ctlpos *cp,
                char *stext,
                int sid,
                int listid,
                char *btext,
                int bid,
                ...)
{
  RECT r;
  int y;
  va_list ap;
  int lwid, rwid, rpos;
  const int LISTHEIGHT = 66;

  /* The static control. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = STATICHEIGHT;
  cp->ypos += r.bottom + GAPWITHIN;
  doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, stext, sid);

  rpos = GAPBETWEEN + 2 * (cp->width + GAPBETWEEN) / 3;
  lwid = rpos - 2 * GAPBETWEEN;
  rwid = cp->width + GAPBETWEEN - rpos;

  /* The list box. */
  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = lwid;
  r.bottom = LISTHEIGHT;
  doctl(cp,
        r,
        "LISTBOX",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_HASSTRINGS |
            LBS_USETABSTOPS | LBS_NOTIFY,
        WS_EX_CLIENTEDGE,
        "",
        listid);

  /* The statics. */
  y = cp->ypos;
  va_start(ap, bid);
  while (1) {
    char *ltext;
    int lid, rid;
    ltext = va_arg(ap, char *);
    if (!ltext)
      break;
    lid = va_arg(ap, int);
    rid = va_arg(ap, int);
    r.top = y;
    r.bottom = STATICHEIGHT;
    y += r.bottom + GAPWITHIN;
    r.left = rpos;
    r.right = rwid / 2;
    doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE, 0, ltext, lid);
    r.left = rpos + r.right;
    r.right = rwid - r.right;
    doctl(cp, r, "STATIC", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, "", rid);
  }
  va_end(ap);

  /* The button. */
  r.top = y + 2 * GAPWITHIN;
  r.bottom = PUSHBTNHEIGHT;
  r.left = rpos;
  r.right = rwid;
  doctl(cp,
        r,
        "BUTTON",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        btext,
        bid);

  cp->ypos += LISTHEIGHT + GAPBETWEEN;
}

/*
 * A progress bar (from Common Controls). We like our progress bars
 * to be smooth and unbroken, without those ugly divisions; some
 * older compilers may not support that, but that's life.
 */
void progressbar(struct ctlpos *cp, int id)
{
  RECT r;

  r.left = GAPBETWEEN;
  r.top = cp->ypos;
  r.right = cp->width;
  r.bottom = PROGBARHEIGHT;
  cp->ypos += r.bottom + GAPBETWEEN;

  doctl(cp,
        r,
        PROGRESS_CLASS,
        WS_CHILD | WS_VISIBLE
#ifdef PBS_SMOOTH
            | PBS_SMOOTH
#endif
        ,
        WS_EX_CLIENTEDGE,
        "",
        id);
}
