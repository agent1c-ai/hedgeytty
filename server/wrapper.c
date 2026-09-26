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

#include <ctype.h>
#include <dirent.h>
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

/*
 * Map AF_UNIX path → kernel socket inode via /proc/net/unix.
 * stat(2) st_ino on the path file is NOT the socket inode.
 */
static int unix_path_inode(const char *path, unsigned long *out_ino) {
  FILE *f;
  char line[768];
  size_t plen;

  if (!path || !out_ino)
    return -1;
  plen = strlen(path);
  f = fopen("/proc/net/unix", "r");
  if (!f)
    return -1;
  if (!fgets(line, sizeof line, f)) { /* header */
    fclose(f);
    return -1;
  }
  while (fgets(line, sizeof line, f)) {
    char *p = line;
    unsigned long ino = 0;
    int slot = 0;
    char *save = NULL;
    char *tok;

    /* fields: Num RefCount Protocol Flags Type St Inode Path */
    for (tok = strtok_r(p, " \t\n", &save); tok; tok = strtok_r(NULL, " \t\n", &save)) {
      if (slot == 6) {
        ino = strtoul(tok, NULL, 10);
      } else if (slot >= 7) {
        if (strcmp(tok, path) == 0) {
          *out_ino = ino;
          fclose(f);
          return 0;
        }
        break;
      }
      slot++;
    }
    (void)plen;
  }
  fclose(f);
  return -1;
}

/* Find the process holding the listening socket for path (not an accept() helper). */
static pid_t sock_listener_pid(const char *path) {
  unsigned long ino;
  char want[64];
  DIR *proc;
  struct dirent *pe;
  pid_t found = (pid_t)-1;

  if (unix_path_inode(path, &ino) != 0)
    return (pid_t)-1;
  snprintf(want, sizeof want, "socket:[%lu]", ino);

  proc = opendir("/proc");
  if (!proc)
    return (pid_t)-1;
  while ((pe = readdir(proc)) != NULL) {
    char fdpath[128], link[96], full[160];
    DIR *fd;
    struct dirent *fe;
    pid_t pid;
    const char *s;

    if (!isdigit((unsigned char)pe->d_name[0]))
      continue;
    pid = (pid_t)atoi(pe->d_name);
    if (pid <= 1)
      continue;
    snprintf(fdpath, sizeof fdpath, "/proc/%s/fd", pe->d_name);
    fd = opendir(fdpath);
    if (!fd)
      continue;
    while ((fe = readdir(fd)) != NULL) {
      ssize_t n;
      if (!isdigit((unsigned char)fe->d_name[0]))
        continue;
      snprintf(full, sizeof full, "%s/%s", fdpath, fe->d_name);
      n = readlink(full, link, sizeof link - 1);
      if (n < 0)
        continue;
      link[n] = '\0';
      if (strcmp(link, want) != 0)
        continue;
      /* Prefer hedgeytty_server / twin; otherwise first match. */
      {
        char cpath[64], comm[64];
        int cfd, cn;
        snprintf(cpath, sizeof cpath, "/proc/%s/comm", pe->d_name);
        cfd = open(cpath, O_RDONLY);
        comm[0] = '\0';
        if (cfd >= 0) {
          cn = (int)read(cfd, comm, sizeof comm - 1);
          close(cfd);
          if (cn > 0) {
            comm[cn] = '\0';
            if (comm[cn - 1] == '\n')
              comm[cn - 1] = '\0';
          }
        }
        s = comm;
        if (strstr(s, "hedgeytty") || strstr(s, "twin")) {
          found = pid;
          closedir(fd);
          closedir(proc);
          return found;
        }
        if (found < 0)
          found = pid;
      }
    }
    closedir(fd);
  }
  closedir(proc);
  return found;
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
  if (path)
    unlink(path);
}

/* Kill every hedgeytty_server with no controlling tty (crash / --nohw leftovers). */
static void reap_orphan_hedgeytty_servers(void) {
  DIR *proc;
  struct dirent *pe;

  proc = opendir("/proc");
  if (!proc)
    return;
  while ((pe = readdir(proc)) != NULL) {
    char cpath[64], comm[64];
    int cfd, cn;
    pid_t pid;

    if (!isdigit((unsigned char)pe->d_name[0]))
      continue;
    pid = (pid_t)atoi(pe->d_name);
    if (pid <= 1)
      continue;
    snprintf(cpath, sizeof cpath, "/proc/%s/comm", pe->d_name);
    cfd = open(cpath, O_RDONLY);
    if (cfd < 0)
      continue;
    cn = (int)read(cfd, comm, sizeof comm - 1);
    close(cfd);
    if (cn <= 0)
      continue;
    comm[cn] = '\0';
    if (comm[cn - 1] == '\n')
      comm[cn - 1] = '\0';
    /* Linux truncates comm to 15 chars: "hedgeytty_serve" */
    if (!strstr(comm, "hedgeytty"))
      continue;
    if (proc_tty_nr(pid) <= 0)
      reclaim_server(NULL, pid);
  }
  closedir(proc);
}

/*
 * Connect to path. Returns:
 *   0  — free (missing or stale; stale unlinked)
 *   1  — live server we must not displace (same console)
 *  -1  — live but reclaimed / other tty (caller may proceed)
 */
static int probe_server_sock(const char *path) {
  struct stat st;
  struct sockaddr_un addr;
  int fd, connected;
  pid_t peer;
  int our_tty, peer_tty;
  int have_node;

  have_node = (lstat(path, &st) == 0);
  if (have_node) {
    if (S_ISLNK(st.st_mode)) {
      if (stat(path, &st) != 0 || !S_ISSOCK(st.st_mode)) {
        unlink(path);
        have_node = 0;
      }
    } else if (!S_ISSOCK(st.st_mode)) {
      return 0;
    }
  }

  /* Listener may still exist after a half-unlink; always resolve via /proc. */
  peer = sock_listener_pid(path);
  our_tty = proc_tty_nr(getpid());
  peer_tty = proc_tty_nr(peer);

  if (peer > 1 && peer_tty <= 0) {
    reclaim_server(path, peer);
    return -1;
  }
  if (our_tty > 0 && peer > 1 && peer_tty == our_tty)
    return 1;

  connected = 0;
  if (have_node) {
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      return 1;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof addr.sun_path - 1);
    connected = connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0;
    close(fd);
  }

  if (!connected) {
    /* Path gone or dead name — drop the node; orphans already reaped above. */
    if (have_node)
      unlink(path);
    /* Listener on other tty without connectable path: leave it alone. */
    return 0;
  }

  /* Connected: refresh listener identity (path inode may have changed). */
  peer = sock_listener_pid(path);
  peer_tty = proc_tty_nr(peer);
  if (peer > 1 && peer_tty <= 0) {
    reclaim_server(path, peer);
    return -1;
  }
  if (our_tty > 0 && peer > 1 && peer_tty == our_tty)
    return 1;
  if (peer <= 1) {
    reclaim_server(path, (pid_t)-1);
    return -1;
  }
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

  /* Belt-and-suspenders: never leave headless servers bricking :0. */
  reap_orphan_hedgeytty_servers();

  for (i = 0; i < 8; i++) {
    snprintf(path, sizeof path, "/tmp/.Twin:%d", i);
    if (probe_server_sock(path) == 1) {
      say("refusing to start — apt Twin already running on this console.");
      say("Quit Twin (File → Quit), then run hedgeytty on the bare console.");
      return 1;
    }
  }
  for (i = 0; i < 8; i++) {
    snprintf(path, sizeof path, "/tmp/.HedgeyTTY:%d", i);
    if (probe_server_sock(path) == 1) {
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
  slash = strrchr(logpath, '/');
  if (!slash)
    return;
  *slash = '\0';
  slash2 = strrchr(logpath, '/');
  if (slash2) {
    *slash2 = '\0';
    (void)mkdir(logpath, 0700);
    *slash2 = '/';
  }
  (void)mkdir(logpath, 0700);
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
