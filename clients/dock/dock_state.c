#include "dock.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static void config_dir(char *out, size_t n) {
  const char *home = getenv("HOME");
  if (!home || !*home)
    home = ".";
  snprintf(out, n, "%s/.config/hedgeytty", home);
}

const char *dock_state_path(void) {
  static char path[768];
  char dir[640];
  config_dir(dir, sizeof dir);
  snprintf(path, sizeof path, "%s/dock.slots", dir);
  return path;
}

static const char *lock_path(void) {
  static char path[768];
  char dir[640];
  config_dir(dir, sizeof dir);
  snprintf(path, sizeof path, "%s/dock.lock", dir);
  return path;
}

int dock_acquire_lock(Dock *d) {
  char dir[640];
  char buf[32];
  int n;

  if (!d)
    return 0;
  config_dir(dir, sizeof dir);
  mkdir(dir, 0755);

  d->lock_fd = open(lock_path(), O_RDWR | O_CREAT, 0644);
  if (d->lock_fd < 0) {
    fprintf(stderr, "dock: lock open: %s\n", strerror(errno));
    return 0;
  }
  if (flock(d->lock_fd, LOCK_EX | LOCK_NB) != 0) {
    fprintf(stderr, "dock: already running\n");
    close(d->lock_fd);
    d->lock_fd = -1;
    return 0;
  }
  if (ftruncate(d->lock_fd, 0) == 0) {
    n = snprintf(buf, sizeof buf, "%d\n", (int)getpid());
    if (n > 0) {
      ssize_t w = write(d->lock_fd, buf, (size_t)n);
      (void)w;
    }
  }
  return 1;
}

void dock_load(Dock *d) {
  FILE *fp;
  int i;
  char line[DOCK_CMD_MAX];

  if (!d)
    return;
  for (i = 0; i < DOCK_N_ICONS; i++)
    d->cmd[i][0] = '\0';

  fp = fopen(dock_state_path(), "r");
  if (!fp)
    return;
  for (i = 0; i < DOCK_N_ICONS; i++) {
    size_t n;
    if (!fgets(line, sizeof line, fp))
      break;
    n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = '\0';
    if (n >= DOCK_CMD_MAX)
      n = DOCK_CMD_MAX - 1;
    memcpy(d->cmd[i], line, n);
    d->cmd[i][n] = '\0';
  }
  fclose(fp);
}

int dock_save(Dock *d) {
  FILE *fp;
  char dir[640];
  int i;

  if (!d)
    return 0;
  config_dir(dir, sizeof dir);
  mkdir(dir, 0755);
  fp = fopen(dock_state_path(), "w");
  if (!fp) {
    fprintf(stderr, "dock: save %s: %s\n", dock_state_path(), strerror(errno));
    return 0;
  }
  for (i = 0; i < DOCK_N_ICONS; i++)
    fprintf(fp, "%s\n", d->cmd[i]);
  fclose(fp);
  return 1;
}
