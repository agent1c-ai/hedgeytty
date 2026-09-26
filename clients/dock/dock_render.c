#include "dock.h"

#include <stdlib.h>
#include <string.h>

static trgb rgb(byte r, byte g, byte b) { return TRGB(r, g, b); }

static tcell cell(byte fr, byte fg, byte fb, byte br, byte bg, byte bb, trune ch) {
  return TCELL(TCOL(rgb(fr, fg, fb), rgb(br, bg, bb)), ch);
}

tcell *dock_make_plus_icon(void) {
  static const char *shape[DOCK_ICON_ROWS] = {
      "          ",
      "    ##    ",
      "  ######  ",
      "    ##    ",
      "          ",
  };
  tcell *cells = calloc((size_t)DOCK_ICON_COLS * DOCK_ICON_ROWS, sizeof(tcell));
  int y, x;
  if (!cells)
    return NULL;
  for (y = 0; y < DOCK_ICON_ROWS; y++) {
    for (x = 0; x < DOCK_ICON_COLS; x++) {
      if (shape[y][x] == '#')
        cells[y * DOCK_ICON_COLS + x] =
            cell(DOCK_GREY, DOCK_GREY, DOCK_GREY, DOCK_GREY, DOCK_GREY, DOCK_GREY, ' ');
      else
        cells[y * DOCK_ICON_COLS + x] = cell(0, 0, 0, 0, 0, 0, ' ');
    }
  }
  return cells;
}

void dock_layout_slots(Dock *d) {
  int i;
  if (!d)
    return;
  for (i = 0; i < DOCK_N_ICONS; i++) {
    d->slot_x[i] =
        DOCK_PAD_X +
        (int)((long)i * (d->win_w - 2 * DOCK_PAD_X - DOCK_ICON_COLS) / (DOCK_N_ICONS - 1));
    if (d->slot_x[i] < 0)
      d->slot_x[i] = 0;
    if (d->slot_x[i] + DOCK_ICON_COLS > d->win_w)
      d->slot_x[i] = d->win_w - DOCK_ICON_COLS;
  }
}

void dock_fill_background(Dock *d) {
  tcell *row;
  int i;
  if (!d || !d->win)
    return;
  row = malloc((size_t)d->win_w * sizeof(tcell));
  if (!row)
    return;
  for (i = 0; i < d->win_w; i++)
    row[i] = cell(0, 0, 0, 0, 0, 0, ' ');
  for (i = 0; i < d->win_h; i++)
    TwWriteTCellWindow(d->win, 0, i, (udat)d->win_w, row);
  free(row);
}

static void clear_slot_area(Dock *d, int x0) {
  tcell blank[DOCK_ICON_COLS];
  int x, y;
  for (x = 0; x < DOCK_ICON_COLS; x++)
    blank[x] = cell(0, 0, 0, 0, 0, 0, ' ');
  for (y = 0; y < DOCK_ICON_ROWS; y++)
    TwWriteTCellWindow(d->win, (dat)x0, (dat)(DOCK_PAD_Y + y), (udat)DOCK_ICON_COLS, blank);
}

static void draw_label(Dock *d, int x0, const char *text, int cursor) {
  tcell row[DOCK_ICON_COLS];
  int i, len, start, mid = DOCK_ICON_ROWS / 2, tlen;
  char shown[DOCK_ICON_COLS + 1];

  clear_slot_area(d, x0);

  tlen = (int)strlen(text);
  len = tlen;
  if (len > DOCK_ICON_COLS)
    len = DOCK_ICON_COLS;
  memset(shown, ' ', DOCK_ICON_COLS);
  start = (DOCK_ICON_COLS - len) / 2;
  if (start < 0)
    start = 0;
  memcpy(shown + start, text, (size_t)len);
  shown[DOCK_ICON_COLS] = '\0';

  for (i = 0; i < DOCK_ICON_COLS; i++) {
    trune ch = (trune)(unsigned char)shown[i];
    row[i] = cell(DOCK_GREY, DOCK_GREY, DOCK_GREY, 0, 0, 0, ch ? ch : ' ');
  }
  TwWriteTCellWindow(d->win, (dat)x0, (dat)(DOCK_PAD_Y + mid), (udat)DOCK_ICON_COLS, row);

  if (cursor) {
    int cx = start + tlen;
    tcell cur;
    if (cx >= DOCK_ICON_COLS)
      cx = DOCK_ICON_COLS - 1;
    if (cx < 0)
      cx = 0;
    cur = cell(0, 0, 0, DOCK_GREY, DOCK_GREY, DOCK_GREY, ' ');
    TwWriteTCellWindow(d->win, (dat)(x0 + cx), (dat)(DOCK_PAD_Y + mid), 1, &cur);
  }
}

void dock_draw_slot(Dock *d, int slot) {
  int x0, cy;
  if (!d || !d->win || slot < 0 || slot >= DOCK_N_ICONS)
    return;
  x0 = d->slot_x[slot];

  if (d->editing == slot) {
    draw_label(d, x0, d->editbuf, 1);
    return;
  }
  if (d->cmd[slot][0]) {
    draw_label(d, x0, d->cmd[slot], 0);
    return;
  }
  if (!d->plus)
    return;
  for (cy = 0; cy < DOCK_ICON_ROWS; cy++)
    TwWriteTCellWindow(d->win, (dat)x0, (dat)(DOCK_PAD_Y + cy), (udat)DOCK_ICON_COLS,
                       d->plus + cy * DOCK_ICON_COLS);
}

void dock_draw_all(Dock *d) {
  int i;
  if (!d)
    return;
  for (i = 0; i < DOCK_N_ICONS; i++)
    dock_draw_slot(d, i);
  TwFlush();
}
