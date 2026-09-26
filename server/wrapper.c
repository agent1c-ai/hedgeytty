/*
 *  wrapper.c  --  thin client that execs hedgeytty_server
 *
 *  This program is in the public domain
 *
 *  Also:
 *   - refuses to steal a console already owned by Twin/HedgeyTTY
 *     (nested start + Quit leaves the outer session broken)
 *   - reclaims orphaned servers (no controlling tty) left after crashes
 *     or detached --nohw runs, so a stale socket cannot brick startup
 *   - redirects stderr to a state log so server chatter does not paint
 *     over the menubar on the Linux console
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <Ht/autoconf.h>
#include "compiler.h"

#ifdef TW_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef BINDIR
#warning BINDIR is not #defined, assuming "/usr/local/bin"
#define BINDIR "/usr/local/bin"
#endif

static char bindir_hedgeytty_server[] = BINDIR "/hedgeytty_server";

static void say(const char *msg) {
  int fd = open("/dev/tty", O_WRONLY | O_NOCTTY);
  if (fd < 0)
    fd = 2;
  {
    char buf[512];
    int n = snprintf(buf, sizeof buf, "hedgeytty: %s\n", msg);
    if (n > 0)
      (void)write(fd, buf, (size_t)(n < (int)sizeof buf ? n : (int)sizeof buf - 1));
  }
  if (fd != 2)
    close(fd);
}

static int env_truthy(const char *name) {
  const char *v = getenv(name);
  return v && v[0] == '1' && v[1] == '\0';
}

/* Linux /proc/<pid>/stat field tty_nr; 0 = no controlling terminal. -1 on error. */
static int proc_tty_nr(pid_t pid) {
  char path[64], buf[512];
  char *rp;
  int fd, n, tty = -1;
  char state;

  if (pid <= 0)
    return -1;
  snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
  fd = open(path, O_RDONLY);
  if (fd < 0)
    return -1;
  n = (int)read(fd, buf, sizeof buf - 1);
  close(fd);
  if (n <= 0)
    return -1;
  buf[n] = '\0';
  rp = strrchr(buf, ')');
  if (!rp || rp[1] != ' ')
    return -1;
  /* after ") ": state ppid pgrp session tty_nr */
  if (sscanf(rp + 2, "%c %*d %*d %*d %d", &state, &tty) != 2)
    return -1;
  return tty;
}

static pid_t sock_peer_pid(int fd) {
  struct ucred cr;
  socklen_t len = sizeof cr;

  memset(&cr, 0, sizeof cr);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &len) != 0)
    return (pid_t)-1;
  return cr.pid;
}

static void reclaim_server(const char *path, pid_t peer) {
  char msg[160];
  int i;

  if (peer > 1) {
    snprintf(msg, sizeof msg, "reclaiming orphaned server (pid %d).", (int)peer);
    say(msg);
    (void)kill(peer, SIGTERM);
    for (i = 0; i < 40; i++) {
      if (kill(peer, 0) != 0)
        break;
      usleep(50000);
    }
    if (kill(peer, 0) == 0)
      (void)kill(peer, SIGKILL);
  }
  unlink(path);
}

/*
 * Connect to path. Returns:
 *   0  — free (missing or stale; stale unlinked)
 *   1  — live server we must not displace (same console)
 *  -1  — live but reclaimed / other tty (caller may proceed)
 *
 * out_peer: peer pid when connected, else -1.
 */
static int probe_server_sock(const char *path, pid_t *out_peer) {
  struct stat st;
  struct sockaddr_un addr;
  int fd, connected;
  pid_t peer = (pid_t)-1;
  int our_tty, peer_tty;

  if (out_peer)
    *out_peer = (pid_t)-1;

  if (lstat(path, &st) != 0)
    return 0;
  if (S_ISLNK(st.st_mode)) {
    /* Hitomi bridge leftover — busy if target is a live sock */
    if (stat(path, &st) != 0 || !S_ISSOCK(st.st_mode)) {
      unlink(path);
      return 0;
    }
  } else if (!S_ISSOCK(st.st_mode)) {
    return 0;
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return 1; /* assume busy */

  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof addr.sun_path - 1);
  connected = connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0;
  if (connected)
    peer = sock_peer_pid(fd);
  close(fd);

  if (!connected) {
    unlink(path); /* stale after crash / nested Quit */
    return 0;
  }

  if (out_peer)
    *out_peer = peer;

  our_tty = proc_tty_nr(getpid());
  peer_tty = proc_tty_nr(peer);

  /* Orphan: holds the socket but has no console (crash / --nohw leftover). */
  if (peer > 1 && peer_tty == 0) {
    reclaim_server(path, peer);
    return -1;
  }

  /*
   * Only refuse when the live server owns *our* console. A session on
   * another tty (or when we have no CTTY) must not brick startup forever.
   */
  if (our_tty > 0 && peer_tty == our_tty)
    return 1;

  /* Connected but cannot identify peer — do not displace blindly. */
  if (peer <= 1)
    return 1;

  return -1;
}

/*
 * Starting on top of an existing Twin/HedgeyTTY console is unsafe: the tty
 * driver grabs the same /dev/ttyN, and Quit restores keyboard/video state
 * the outer server still expects.
 */
static int refuse_nested_console(void) {
  int i;
  char path[64];

  if (env_truthy("HEDGEYTTY_ALLOW_NESTED"))
    return 0;

  for (i = 0; i < 8; i++) {
    snprintf(path, sizeof path, "/tmp/.Twin:%d", i);
    if (probe_server_sock(path, NULL) == 1) {
      say("refusing to start — apt Twin already running on this console.");
      say("Quit Twin (File → Quit), then run hedgeytty on the bare console.");
      return 1;
    }
  }
  for (i = 0; i < 8; i++) {
    snprintf(path, sizeof path, "/tmp/.HedgeyTTY:%d", i);
    if (probe_server_sock(path, NULL) == 1) {
      say("refusing to start — HedgeyTTY already running on this console.");
      say("Quit that session first (File → Quit).");
      return 1;
    }
  }
  return 0;
}

static void ensure_state_dir(char *logpath) {
  char *slash;
  char *slash2;
  /* logpath = .../state/hedgeytty/server.log — mkdir parents then restore */
  slash = strrchr(logpath, '/');
  if (!slash)
    return;
  *slash = '\0'; /* .../hedgeytty */
  slash2 = strrchr(logpath, '/');
  if (slash2) {
    *slash2 = '\0';
    (void)mkdir(logpath, 0700); /* .../state (or XDG_STATE_HOME) */
    *slash2 = '/';
  }
  (void)mkdir(logpath, 0700); /* .../hedgeytty */
  *slash = '/';
}

static void redirect_stderr_to_log(void) {
  const char *state = getenv("XDG_STATE_HOME");
  const char *home = getenv("HOME");
  char path[512];
  int fd;

  if (state && *state)
    snprintf(path, sizeof path, "%s/hedgeytty/server.log", state);
  else if (home && *home)
    snprintf(path, sizeof path, "%s/.local/state/hedgeytty/server.log", home);
  else
    return;

  ensure_state_dir(path);

  fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0) {
    (void)dup2(fd, STDERR_FILENO);
    if (fd != STDERR_FILENO)
      close(fd);
  }
}

int main(int argc, char *argv[]) {
  (void)argc;

  if (refuse_nested_console())
    return 75;

  redirect_stderr_to_log();

  argv[0] = bindir_hedgeytty_server;
  execv(argv[0], argv);
  printf("failed to exec %s: %s\n", argv[0], strerror(errno));
  return 1;
}
