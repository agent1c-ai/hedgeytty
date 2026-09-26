#include "dock.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Ht/Twkeys.h>
#include <Ht/mouse.h>

#ifndef BINDIR
#define BINDIR "/usr/local/bin"
#endif

void dock_launch(const char *cmd) {
  pid_t pid;
  char htterm[512];
  char title[64];
  size_t i;

  if (!cmd || !*cmd)
    return;

  for (i = 0; cmd[i] && !isspace((unsigned char)cmd[i]) && i + 1 < sizeof title; i++)
    title[i] = cmd[i];
  title[i] = '\0';
  if (!title[0])
    snprintf(title, sizeof title, "term");

  snprintf(htterm, sizeof htterm, "%s/htterm", BINDIR);

  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "dock: fork: %s\n", strerror(errno));
    return;
  }
  if (pid == 0) {
    setsid();
    if (access(htterm, X_OK) != 0)
      snprintf(htterm, sizeof htterm, "htterm");
    execlp(htterm, "htterm", "-t", title, "-e", "sh", "-c", cmd, (char *)0);
    _exit(127);
  }
}

void dock_begin_edit(Dock *d, int slot) {
  if (!d || slot < 0 || slot >= DOCK_N_ICONS)
    return;
  d->editing = slot;
  d->editlen = 0;
  d->editbuf[0] = '\0';
  dock_draw_slot(d, slot);
  TwFlush();
}

void dock_cancel_edit(Dock *d) {
  int s;
  if (!d)
    return;
  s = d->editing;
  d->editing = -1;
  d->editbuf[0] = '\0';
  d->editlen = 0;
  if (s >= 0)
    dock_draw_slot(d, s);
  TwFlush();
}

void dock_commit_edit(Dock *d) {
  int s, i;
  if (!d || d->editing < 0)
    return;
  s = d->editing;

  while (d->editlen > 0 && isspace((unsigned char)d->editbuf[d->editlen - 1]))
    d->editbuf[--d->editlen] = '\0';
  i = 0;
  while (d->editbuf[i] && isspace((unsigned char)d->editbuf[i]))
    i++;
  if (i > 0) {
    memmove(d->editbuf, d->editbuf + i, (size_t)(d->editlen - i + 1));
    d->editlen -= i;
  }

  dock_slot_set(d, s, d->editbuf);
  d->editing = -1;
  d->editbuf[0] = '\0';
  d->editlen = 0;
  dock_save(d);
  dock_draw_slot(d, s);
  TwFlush();
}

void dock_on_key(Dock *d, tevent_keyboard ev) {
  udat code;
  if (!d || !ev || d->editing < 0)
    return;
  code = ev->Code;

  if (code == TW_Return || code == TW_Linefeed) {
    dock_commit_edit(d);
    return;
  }
  if (code == TW_Escape) {
    dock_cancel_edit(d);
    return;
  }
  if (code == TW_BackSpace || code == TW_Delete) {
    if (d->editlen > 0) {
      d->editbuf[--d->editlen] = '\0';
      dock_draw_slot(d, d->editing);
      TwFlush();
    }
    return;
  }
  if (ev->SeqLen == 1) {
    unsigned char ch = (unsigned char)ev->AsciiSeq[0];
    if (ch >= 32 && ch < 127 && d->editlen + 1 < DOCK_CMD_MAX) {
      d->editbuf[d->editlen++] = (char)ch;
      d->editbuf[d->editlen] = '\0';
      dock_draw_slot(d, d->editing);
      TwFlush();
    }
  }
}

void dock_on_mouse(Dock *d, tevent_mouse ev) {
  int slot;
  if (!d || !ev)
    return;
  if (!isPRESS(ev->Code) || (ev->Code & PRESS_ANY) != PRESS_LEFT)
    return;

  slot = dock_slot_at(d, ev->X, ev->Y);
  if (slot < 0) {
    if (d->editing >= 0)
      dock_cancel_edit(d);
    return;
  }

  if (d->editing >= 0 && d->editing != slot)
    dock_cancel_edit(d);

  if (d->cmd[slot][0] && d->editing != slot) {
    dock_launch(d->cmd[slot]);
    return;
  }

  if (!d->cmd[slot][0])
    dock_begin_edit(d, slot);
}
