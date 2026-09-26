/* HedgeyTTY dock — composable pieces (state, render, launch, twin app). */
#ifndef HEDGEYTTY_DOCK_H
#define HEDGEYTTY_DOCK_H

#include <Ht/Tw.h>

#define DOCK_ICON_COLS 10
#define DOCK_ICON_ROWS 5
#define DOCK_N_ICONS 12
#define DOCK_PAD_X 2
#define DOCK_PAD_Y 1
#define DOCK_CMD_MAX 240
#define DOCK_GREY 160

typedef struct Dock {
  char cmd[DOCK_N_ICONS][DOCK_CMD_MAX];
  int slot_x[DOCK_N_ICONS];
  int editing; /* slot index or -1 */
  char editbuf[DOCK_CMD_MAX];
  int editlen;
  int win_w, win_h;
  tcell *plus;
  twindow win;
  int lock_fd;
} Dock;

/* lifecycle */
Dock *dock_new(void);
void dock_free(Dock *d);

/* state + singleton lock (~/.config/hedgeytty/dock.*) */
int dock_acquire_lock(Dock *d);
void dock_load(Dock *d);
int dock_save(Dock *d);
const char *dock_state_path(void);

/* slot model */
const char *dock_slot_cmd(const Dock *d, int slot);
void dock_slot_set(Dock *d, int slot, const char *cmd);
int dock_slot_at(const Dock *d, dat x, dat y);

/* render */
tcell *dock_make_plus_icon(void);
void dock_layout_slots(Dock *d);
void dock_fill_background(Dock *d);
void dock_draw_slot(Dock *d, int slot);
void dock_draw_all(Dock *d);

/* actions */
void dock_launch(const char *cmd);
void dock_begin_edit(Dock *d, int slot);
void dock_cancel_edit(Dock *d);
void dock_commit_edit(Dock *d);
void dock_on_key(Dock *d, tevent_keyboard ev);
void dock_on_mouse(Dock *d, tevent_mouse ev);

/* twin window + event loop (returns 0 on clean quit) */
int dock_run(Dock *d);

#endif /* HEDGEYTTY_DOCK_H */
