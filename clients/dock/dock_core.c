#include "dock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Dock *dock_new(void) {
  Dock *d = calloc(1, sizeof *d);
  if (!d)
    return NULL;
  d->editing = -1;
  d->lock_fd = -1;
  return d;
}

void dock_free(Dock *d) {
  if (!d)
    return;
  free(d->plus);
  d->plus = NULL;
  if (d->lock_fd >= 0) {
    close(d->lock_fd);
    d->lock_fd = -1;
  }
  free(d);
}

const char *dock_slot_cmd(const Dock *d, int slot) {
  if (!d || slot < 0 || slot >= DOCK_N_ICONS)
    return "";
  return d->cmd[slot];
}

void dock_slot_set(Dock *d, int slot, const char *cmd) {
  if (!d || slot < 0 || slot >= DOCK_N_ICONS)
    return;
  if (!cmd)
    cmd = "";
  snprintf(d->cmd[slot], sizeof d->cmd[slot], "%s", cmd);
}

int dock_slot_at(const Dock *d, dat x, dat y) {
  int i;
  if (!d || y < DOCK_PAD_Y || y >= DOCK_PAD_Y + DOCK_ICON_ROWS)
    return -1;
  for (i = 0; i < DOCK_N_ICONS; i++) {
    if (x >= d->slot_x[i] && x < d->slot_x[i] + DOCK_ICON_COLS)
      return i;
  }
  return -1;
}
