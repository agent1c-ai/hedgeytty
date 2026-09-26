#include "dock.h"

#include <stdio.h>
#include <string.h>

#include <Ht/Twerrno.h>

TW_DECL_MAGIC(dock_magic);

int dock_run(Dock *d) {
  const char *title = "Dock";
  tcolor title_cols[8];
  tmsgport mp;
  tmenu menu;
  tscreen scr;
  tmsg msg;
  int sw, sh, i, tlen;

  if (!d)
    return 1;

  tlen = (int)strlen(title);
  for (i = 0; i < tlen && i < 8; i++)
    title_cols[i] = TCOL(tWHITE, tBLUE);

  if (!TwCheckMagic(dock_magic) || !TwOpen(NULL)) {
    fprintf(stderr, "dock: open: %s%s\n", TwStrError(TwErrno),
            TwStrErrorDetail(TwErrno, TwErrnoDetail));
    return 1;
  }

  scr = TwFirstScreen();
  sw = TwGetDisplayWidth();
  sh = TwGetDisplayHeight();
  if (sw < 80)
    sw = 80;
  if (sh < 20)
    sh = 20;

  d->win_w = sw - 4;
  if (d->win_w < DOCK_N_ICONS * DOCK_ICON_COLS + 4)
    d->win_w = DOCK_N_ICONS * DOCK_ICON_COLS + 4;
  d->win_h = DOCK_ICON_ROWS + 2 * DOCK_PAD_Y;

  d->plus = dock_make_plus_icon();
  if (!d->plus) {
    TwClose();
    return 1;
  }
  dock_layout_slots(d);

  /* TwCreateMsgPort(NameLen, name) — first arg is name length, not queue size. */
  if (!(mp = TwCreateMsgPort(6, "htdock")) ||
      !(menu = TwCreateMenu(TCOL(tblack, twhite), TCOL(tblack, tgreen), TCOL(tBLACK, twhite),
                            TCOL(tBLACK, tblack), TCOL(tred, twhite), TCOL(tred, tgreen),
                            (byte)0)) ||
      !TwItem4MenuCommon(menu)) {
    fprintf(stderr, "dock: menu setup failed: %s\n", TwStrError(TwErrno));
    TwClose();
    return 1;
  }
  (void)mp;

  d->win = TwCreateWindow(
      (dat)tlen, title, title_cols, menu, TCOL(twhite, tblack), TW_NOCURSOR,
      TW_WINDOW_DRAG | TW_WINDOW_RESIZE | TW_WINDOW_CLOSE | TW_WINDOW_WANT_KEYS |
          TW_WINDOW_WANT_MOUSE,
      TW_WINDOWFL_USECONTENTS, d->win_w, d->win_h, 0);
  if (!d->win) {
    fprintf(stderr, "dock: create failed: %s\n", TwStrError(TwErrno));
    TwClose();
    return 1;
  }

  TwSetColorsWindow(d->win, 0x1FF, TCOL(tYELLOW, tcyan), TCOL(tGREEN, tBLUE), TCOL(twhite, tBLUE),
                    TCOL(tWHITE, tBLUE), TCOL(tWHITE, tBLUE), TCOL(twhite, tblack),
                    TCOL(twhite, tBLACK), TCOL(tBLACK, tblack), TCOL(tblack, tBLACK));

  TwConfigureWindow(d->win, 0x3, 2, sh - d->win_h - 3, 0, 0, 0, 0);
  TwMapWindow(d->win, scr);

  dock_fill_background(d);
  dock_draw_all(d);

  while ((msg = TwReadMsg(ttrue))) {
    if (msg->Type == TW_MSG_WIDGET_GADGET && msg->Event.EventGadget.Code == 0)
      break;
    if (msg->Type == TW_MSG_WIDGET_KEY)
      dock_on_key(d, &msg->Event.EventKeyboard);
    else if (msg->Type == TW_MSG_WIDGET_MOUSE)
      dock_on_mouse(d, &msg->Event.EventMouse);
  }

  TwClose();
  d->win = (twindow)0;
  return 0;
}
