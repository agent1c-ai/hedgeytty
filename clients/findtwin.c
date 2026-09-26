/*
 *  findtwin.c  --  find a running HedgeyTTY (or Twin) server
 *
 *  This program is placed in the public domain.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <Ht/autoconf.h>

#ifdef TW_HAVE_DIRENT_H
#include <dirent.h>
#else
#ifdef TW_HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#ifdef TW_HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#ifdef TW_HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#include <Ht/Tw.h>
#include <Ht/Twerrno.h>

TW_DECL_MAGIC(findtwin_magic);

static void try_TwOpen(const char *dpy) {
  if (dpy || (dpy = getenv("HTDISPLAY"))) {
    if (TwOpen(dpy)) {
      printf("%s\n", dpy);
      exit(0);
    }
  }
}

#define ishex(c) (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f'))

/* Match .HedgeyTTY:N or legacy .Twin:N (display hex up to 3 digits). */
static int match_display_socket(const struct dirent *d) {
  const char *s = d->d_name;
  const char *colon;

  if (!strncmp(s, ".HedgeyTTY:", 11))
    colon = s + 10;
  else if (!strncmp(s, ".Twin:", 6))
    colon = s + 5;
  else
    return 0;
  if (*colon != ':')
    return 0;
  colon++;
  if (!ishex(colon[0]))
    return 0;
  if (!colon[1])
    return 1;
  if (!ishex(colon[1]))
    return 0;
  if (!colon[2])
    return 1;
  return ishex(colon[2]) && !colon[3];
}

#if defined(TW_HAVE_SCANDIR) && (defined(TW_HAVE_VERSIONSORT) || defined(TW_HAVE_ALPHASORT))
static const char *tmpdir(void) {
  const char *tmp = getenv("TMPDIR");
  if (tmp == NULL)
    tmp = "/tmp";
  return tmp;
}

static void search_unix_socket(void) {

#ifdef TW_HAVE_VERSIONSORT
#define my_sort versionsort
#else
#define my_sort alphasort
#endif
  int my_sort(const struct dirent **, const struct dirent **);

  struct dirent **namelist;
  char *s;
  int n = scandir(tmpdir(), &namelist, match_display_socket, my_sort);

  while (n > 0) {
    s = namelist[0]->d_name;
    /* TwOpen wants ":N" — skip ".HedgeyTTY" or ".Twin" prefix before ':' */
    {
      const char *colon = strchr(s, ':');
      if (colon)
        try_TwOpen(colon);
    }

    namelist++;
    n--;
  }
}
#endif

int main(int argc, char *argv[]) {
  (void)argc;

  if (*++argv) {
    do {
      try_TwOpen(*argv);
    } while (*++argv);

    return 1;
  }

  if (!TwCheckMagic(findtwin_magic)) {
    fprintf(stderr, "htfindtwin: %s%s\n", TwStrError(TwErrno),
            TwStrErrorDetail(TwErrno, TwErrnoDetail));
    return 1;
  }

  try_TwOpen(NULL);

#if defined(TW_HAVE_SCANDIR) && (defined(TW_HAVE_VERSIONSORT) || defined(TW_HAVE_ALPHASORT))
  search_unix_socket();
#endif

  return 1;
}
