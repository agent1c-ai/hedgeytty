/* hedgeytty-hitomi-bg — paint Hitomi hedgehog on Twin desktop (top-right).
 *
 * Avoids twsetroot ANSI: Twin 1.0 treats ANSI 0..7 as RGB values, so
 * img2txt art becomes invisible. We load PNG via ImageMagick, then set
 * per-cell truecolor half-blocks through libtw.
 */
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Ht/Tw.h>
#include <Ht/Twerrno.h>

TW_DECL_MAGIC(hedgeytty_hitomi_magic);

#ifndef ICON_URL
#define ICON_URL "https://hitomi.love/assets/hitomi-icon.png"
#endif

/* Desktop field */
static const trgb DESK_R = 180, DESK_G = 60, DESK_B = 140; /* magenta-ish */

/* Bridge apt Twin sockets/auth into HedgeyTTY names so libht can attach
 * while both stacks coexist (same wire protocol, different on-disk names). */
static void bridge_apt_twin(void) {
  const char *home = getenv("HOME");
  char twin_sock[64], ht_sock[64], twin_auth[512], ht_auth[512];
  struct stat st;
  int n;

  for (n = 0; n < 8; n++) {
    snprintf(twin_sock, sizeof twin_sock, "/tmp/.Twin:%d", n);
    snprintf(ht_sock, sizeof ht_sock, "/tmp/.HedgeyTTY:%d", n);
    if (stat(twin_sock, &st) == 0 && S_ISSOCK(st.st_mode)) {
      if (stat(ht_sock, &st) != 0)
        symlink(twin_sock, ht_sock);
    }
  }
  if (!home || !*home)
    return;
  snprintf(twin_auth, sizeof twin_auth, "%s/.TwinAuth", home);
  snprintf(ht_auth, sizeof ht_auth, "%s/.HedgeyTTYAuth", home);
  if (stat(twin_auth, &st) == 0 && S_ISREG(st.st_mode) && stat(ht_auth, &st) != 0)
    symlink(twin_auth, ht_auth);
}

static byte open_twin(void) {
  const char *dpy;
  char buf[64];
  FILE *fp;

  bridge_apt_twin();

  if ((dpy = getenv("HTDISPLAY")) && TwOpen(dpy))
    return ttrue;
  if ((dpy = getenv("TWDISPLAY")) && TwOpen(dpy))
    return ttrue;
  if (TwOpen(NULL))
    return ttrue;
  if (TwOpen(":0"))
    return ttrue;
  if (TwOpen(":1"))
    return ttrue;
  fp = popen("htfindtwin 2>/dev/null; twfindtwin 2>/dev/null", "r");
  if (fp) {
    if (fgets(buf, sizeof buf, fp)) {
      size_t n = strlen(buf);
      while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
      pclose(fp);
      if (n && TwOpen(buf))
        return ttrue;
    } else {
      pclose(fp);
    }
  }
  return tfalse;
}

static trgb rgb(byte r, byte g, byte b) {
  return TRGB(r, g, b);
}

static void blend(byte *r, byte *g, byte *b, byte a, byte br, byte bg, byte bb) {
  /* a is 0..255 coverage of foreground */
  unsigned ia = 255 - a;
  *r = (byte)((*r * a + br * ia) / 255);
  *g = (byte)((*g * a + bg * ia) / 255);
  *b = (byte)((*b * a + bb * ia) / 255);
}

/* Load trimmed icon as RGBA via magick; *out_w / *out_h are pixel size. */
static byte *load_rgba(const char *png, int target_cols, int target_cell_rows, int *out_w,
                       int *out_h) {
  char cmd[1024];
  FILE *fp;
  byte *buf;
  size_t need, got;
  int pw = target_cols;
  int ph = target_cell_rows * 2; /* half-block = 2 pixels tall */

  /* Resize preserving aspect into a box; then we letterbox onto desk. */
  snprintf(cmd, sizeof cmd,
           "magick '%s' -trim +repage -resize %dx%d -background none -gravity center "
           "-extent %dx%d RGBA:-",
           png, pw, ph, pw, ph);

  fp = popen(cmd, "r");
  if (!fp) {
    fprintf(stderr, "hedgeytty-hitomi-bg: popen magick failed: %s\n", strerror(errno));
    return NULL;
  }
  need = (size_t)pw * (size_t)ph * 4;
  buf = (byte *)malloc(need);
  if (!buf) {
    pclose(fp);
    return NULL;
  }
  got = fread(buf, 1, need, fp);
  pclose(fp);
  if (got != need) {
    fprintf(stderr, "hedgeytty-hitomi-bg: magick RGBA read %zu/%zu bytes\n", got, need);
    free(buf);
    return NULL;
  }
  *out_w = pw;
  *out_h = ph;
  return buf;
}

static int file_ok(const char *p) {
  struct stat st;
  return p && p[0] && stat(p, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

static const char *ensure_icon(char *path, size_t pathlen) {
  const char *home = getenv("HOME") ? getenv("HOME") : ".";
  const char *xdg = getenv("XDG_DATA_HOME");
  char dir[512];
  char urlcmd[1024];

#ifndef DATADIR
#define DATADIR "/usr/local/share/hedgeytty"
#endif

  /* Prefer installed / user icon; fall back to download. */
  snprintf(path, pathlen, "%s/hitomi-icon.png", DATADIR);
  if (file_ok(path))
    return path;
  if (xdg && *xdg)
    snprintf(dir, sizeof dir, "%s/hedgeytty", xdg);
  else
    snprintf(dir, sizeof dir, "%s/.local/share/hedgeytty", home);
  snprintf(path, pathlen, "%s/hitomi-icon.png", dir);
  if (file_ok(path))
    return path;
  snprintf(urlcmd, sizeof urlcmd,
           "mkdir -p '%s' && curl -fsSL -o '%s' '%s'", dir, path, ICON_URL);
  if (system(urlcmd) != 0) {
    fprintf(stderr, "hedgeytty-hitomi-bg: failed to fetch icon\n");
    return NULL;
  }
  return path;
}

int main(int argc, char **argv) {
  char iconpath[512];
  byte *rgba;
  tcell *cells;
  int sw, sh, iw, ih, px, py;
  int x0, y0, cx, cy;
  uldat err;
  tscreen scr;

  (void)argc;
  (void)argv;

  if (!ensure_icon(iconpath, sizeof iconpath))
    return 1;

  if (!TwCheckMagic(hedgeytty_hitomi_magic) || !open_twin()) {
    err = TwErrno;
    fprintf(stderr, "hedgeytty-hitomi-bg: libtw error: %s%s\n", TwStrError(err),
            TwStrErrorDetail(err, TwErrnoDetail));
    return 1;
  }

  scr = TwFirstScreen();
  sw = TwGetDisplayWidth();
  sh = TwGetDisplayHeight();
  if (sw < 40 || sh < 20) {
    sw = 240;
    sh = 67;
  }

  /* Hedgehog occupies ~45% of width, nearly full height minus menu. */
  iw = sw * 45 / 100;
  if (iw < 48)
    iw = 48;
  if (iw > sw - 4)
    iw = sw - 4;
  ih = sh - 3; /* cell rows for art */
  if (ih < 20)
    ih = 20;

  rgba = load_rgba(iconpath, iw, ih, &px, &py);
  if (!rgba) {
    TwClose();
    return 1;
  }

  cells = (tcell *)malloc((size_t)sw * (size_t)sh * sizeof(tcell));
  if (!cells) {
    free(rgba);
    TwClose();
    return 1;
  }

  /* Fill desktop */
  {
    tcolor desk = TCOL(rgb(220, 200, 210), rgb(DESK_R, DESK_G, DESK_B));
    tcell fill = TCELL(desk, ' ');
    int i;
    for (i = 0; i < sw * sh; i++)
      cells[i] = fill;
  }

  /* Top-right placement */
  x0 = sw - iw - 2;
  if (x0 < 0)
    x0 = 0;
  y0 = 1;

  for (cy = 0; cy < ih; cy++) {
    int sy = y0 + cy;
    if (sy < 0 || sy >= sh)
      continue;
    for (cx = 0; cx < iw; cx++) {
      int sx = x0 + cx;
      int ypix = cy * 2;
      byte *u, *d;
      byte ur, ug, ub, ua, dr, dg, db, da;
      trgb fg, bg;
      tcolor col;
      trune rune;

      if (sx < 0 || sx >= sw)
        continue;

      u = rgba + ((ypix)*iw + cx) * 4;
      d = rgba + ((ypix + 1) * iw + cx) * 4;
      ur = u[0];
      ug = u[1];
      ub = u[2];
      ua = u[3];
      dr = d[0];
      dg = d[1];
      db = d[2];
      da = d[3];

      if (ua < 8 && da < 8)
        continue; /* leave desktop */

      blend(&ur, &ug, &ub, ua, DESK_R, DESK_G, DESK_B);
      blend(&dr, &dg, &db, da, DESK_R, DESK_G, DESK_B);

      /* ▀ upper half: fg = upper pixel, bg = lower pixel */
      fg = rgb(ur, ug, ub);
      bg = rgb(dr, dg, db);
      col = TCOL(fg, bg);
      rune = 0x2580; /* UPPER HALF BLOCK */
      cells[sy * sw + sx] = TCELL(col, rune);
    }
  }

  TwBgImageScreen(scr, (dat)sw, (dat)sh, cells);
  TwFlush();
  TwClose();
  free(cells);
  free(rgba);
  return 0;
}
