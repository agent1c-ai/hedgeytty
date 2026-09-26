#!/usr/bin/env bash
# HedgeyTTY from-source installer — canonical entrypoint for:
#   curl -fsSL https://agent1c.ai/tty.sh | sh   # tty.sh invokes bash
#   curl -fsSL .../install.sh | bash
#
# Profiles (HEDGEYTTY_PROFILE=console|pty, or auto-detected):
#   console — Linux VT + gpm (Ubuntu/Debian first-class)
#   pty     — Termux / macOS Terminal (xterm/termcap, no gpm)
#
# Requires bash (not dash). Piped `sh` ignores the shebang and breaks on `[[`.
if [ -z "${BASH_VERSION:-}" ]; then
  printf 'hedgeytty: ERROR: bash is required (try: curl ... | bash)\n' >&2
  exit 1
fi

set -euo pipefail

REPO_OWNER="${HEDGEYTTY_REPO_OWNER:-agent1c-ai}"
REPO_NAME="${HEDGEYTTY_REPO_NAME:-hedgeytty}"
REPO_BRANCH="${HEDGEYTTY_BRANCH:-main}"
REPO_URL="${HEDGEYTTY_REPO_URL:-https://github.com/${REPO_OWNER}/${REPO_NAME}}"

# Preserve Termux's PREFIX before we redefine it as the install prefix.
TERMUX_USR=""
if [[ -n "${TERMUX_VERSION:-}" ]] || [[ -d /data/data/com.termux/files/usr ]]; then
  TERMUX_USR="${PREFIX:-/data/data/com.termux/files/usr}"
fi

if [[ -n "${HEDGEYTTY_PREFIX:-}" ]]; then
  PREFIX="$HEDGEYTTY_PREFIX"
elif [[ -n "$TERMUX_USR" ]]; then
  PREFIX="$TERMUX_USR"
else
  PREFIX="${HOME}/.local"
fi
BIN_DIR="${PREFIX}/bin"
SRC_DIR="${HEDGEYTTY_SRC:-${HOME}/src/hedgeytty}"
JOBS="${HEDGEYTTY_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}"
PROFILE="${HEDGEYTTY_PROFILE:-}"

# Cap parallelism on memory-constrained hosts
if [[ "${JOBS}" -gt 2 ]] && [[ -f /proc/meminfo ]]; then
  avail_kb=$(awk '/MemAvailable:/ {print $2}' /proc/meminfo)
  if [[ "${avail_kb:-0}" -lt 6000000 ]]; then
    JOBS=2
  fi
fi

log() { printf 'hedgeytty: %s\n' "$*"; }
die() { printf 'hedgeytty: ERROR: %s\n' "$*" >&2; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"; }

is_termux() {
  [[ -n "${TERMUX_VERSION:-}" ]] || [[ -n "$TERMUX_USR" ]]
}

detect_profile() {
  local u
  u=$(uname -s 2>/dev/null || echo unknown)
  case "${PROFILE}" in
    console|pty)
      return
      ;;
    "")
      ;;
    *)
      die "HEDGEYTTY_PROFILE must be 'console' or 'pty' (got '${PROFILE}')"
      ;;
  esac
  case "$u" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
      die "Windows / PowerShell is not supported. Use Linux, Termux, or macOS."
      ;;
  esac
  if is_termux; then
    PROFILE=pty
  elif [[ "$u" == Darwin ]]; then
    PROFILE=pty
  elif [[ "$u" == Linux ]]; then
    PROFILE=console
  else
    die "unsupported OS '$u'. Use Linux (console), Termux, or macOS (pty)."
  fi
}

print_manual_deps_console() {
  cat >&2 <<'EOF'
hedgeytty: install these build deps (names vary by distro), then re-run:

  compilers:  gcc g++ make pkg-config autoconf automake libtool
  libraries:  zlib headers, ncurses headers, libltdl headers
  mouse:      gpm daemon + gpm.h + libgpm.so  (Linux console)
  wallpaper:  ImageMagick (magick or convert)
  network:    curl ca-certificates

  Debian/Ubuntu:  build-essential autoconf automake libtool pkg-config
                  gpm libgpm-dev imagemagick curl zlib1g-dev libncurses-dev libltdl-dev
  Fedora:         gcc gcc-c++ make autoconf automake libtool pkgconf
                  gpm gpm-devel ImageMagick curl zlib-devel ncurses-devel libtool-ltdl-devel
  Arch:           base-devel gpm imagemagick curl zlib ncurses libtool
EOF
}

print_manual_deps_pty() {
  cat >&2 <<'EOF'
hedgeytty: install these pty-profile build deps, then re-run:

  Termux:  pkg install clang make autoconf automake libtool pkg-config
                    ncurses zlib imagemagick curl git
  macOS:   xcode-select --install   # once
           brew install autoconf automake libtool pkg-config ncurses imagemagick
EOF
}

have_libgpm() {
  [[ -e /usr/lib/x86_64-linux-gnu/libgpm.so ]] && return 0
  [[ -e /usr/lib/aarch64-linux-gnu/libgpm.so ]] && return 0
  [[ -e /usr/lib/libgpm.so ]] && return 0
  [[ -e /usr/lib64/libgpm.so ]] && return 0
  ls /usr/lib/*/libgpm.so >/dev/null 2>&1 && return 0
  return 1
}

have_zlib_h() {
  [[ -f /usr/include/zlib.h ]] && return 0
  [[ -n "$TERMUX_USR" && -f "$TERMUX_USR/include/zlib.h" ]] && return 0
  local bp
  bp=$(brew --prefix 2>/dev/null || true)
  [[ -n "$bp" && -f "$bp/include/zlib.h" ]] && return 0
  [[ -f /usr/local/include/zlib.h ]] && return 0
  [[ -f /opt/homebrew/include/zlib.h ]] && return 0
  return 1
}

have_cxx() {
  command -v g++ >/dev/null && return 0
  command -v clang++ >/dev/null && return 0
  return 1
}

have_cc() {
  command -v gcc >/dev/null && return 0
  command -v clang >/dev/null && return 0
  return 1
}

deps_present_common() {
  have_cc || return 1
  have_cxx || return 1
  command -v make >/dev/null || return 1
  command -v pkg-config >/dev/null || return 1
  command -v magick >/dev/null || command -v convert >/dev/null || return 1
  command -v curl >/dev/null || return 1
  have_zlib_h || return 1
  return 0
}

deps_present() {
  deps_present_common || return 1
  if [[ "$PROFILE" == console ]]; then
    command -v gpm >/dev/null || return 1
    [[ -f /usr/include/gpm.h ]] || return 1
    have_libgpm || return 1
  fi
  if [[ "${HEDGEYTTY_WITH_X11:-0}" == 1 ]]; then
    [[ -f /usr/include/X11/Xlib.h ]] || return 1
  fi
  return 0
}

sudo_cmd() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
    return
  fi
  command -v sudo >/dev/null 2>&1 || die "need root or sudo for: $*"
  if [[ ! -t 0 ]] && [[ -z "${SUDO_ASKPASS:-}" ]]; then
    local ap
    ap=$(ls -d "${HOME}/.local/share/cursor-agent/versions/"*/cursor-askpass 2>/dev/null | sort -V | tail -1 || true)
    if [[ -n "$ap" && -x "$ap" ]]; then
      export SUDO_ASKPASS="$ap"
      export SUDO_ASKPASS_REQUIRE=force
    fi
  fi
  if [[ -n "${SUDO_ASKPASS:-}" ]]; then
    sudo -A "$@" || die "sudo failed for: $*"
  else
    sudo "$@" || die "sudo failed for: $*"
  fi
}

install_build_deps_termux() {
  need_cmd pkg
  log "installing Termux packages via pkg"
  pkg install -y clang make autoconf automake libtool pkg-config \
    ncurses zlib imagemagick curl git || die "pkg install failed"
}

install_build_deps_brew() {
  need_cmd brew
  log "installing Homebrew packages"
  brew install autoconf automake libtool pkg-config ncurses imagemagick \
    || die "brew install failed"
}

install_build_deps_apt() {
  log "installing build dependencies (not the twin binary)"
  export DEBIAN_FRONTEND=noninteractive
  local pkgs=(
    build-essential autoconf automake libtool pkg-config
    imagemagick curl ca-certificates
    zlib1g-dev libncurses-dev
    libltdl-dev
  )
  if [[ "$PROFILE" == console ]]; then
    pkgs+=(gpm libgpm-dev)
  fi
  if [[ "${HEDGEYTTY_WITH_X11:-0}" == 1 ]]; then
    pkgs+=(libx11-dev libxft-dev)
  fi
  sudo_cmd apt-get update -qq
  sudo_cmd apt-get install -y "${pkgs[@]}"
}

install_build_deps() {
  if deps_present; then
    log "build dependencies already present"
    return
  fi
  if is_termux; then
    install_build_deps_termux
    return
  fi
  if [[ "$(uname -s)" == Darwin ]]; then
    install_build_deps_brew
    return
  fi
  if command -v apt-get >/dev/null; then
    install_build_deps_apt
    return
  fi
  log "no known package manager and dependencies are incomplete"
  if [[ "$PROFILE" == pty ]]; then
    print_manual_deps_pty
  else
    print_manual_deps_console
  fi
  die "install build deps manually, then re-run"
}

fetch_tree() {
  if [[ -n "${HEDGEYTTY_LOCAL:-}" && -f "${HEDGEYTTY_LOCAL}/configure.ac" ]]; then
    SRC_DIR=$HEDGEYTTY_LOCAL
    log "using local tree: $SRC_DIR"
    return
  fi
  need_cmd git
  mkdir -p "$(dirname "$SRC_DIR")"
  if [[ -d "$SRC_DIR/.git" ]]; then
    if ! git -C "$SRC_DIR" rev-parse HEAD >/dev/null 2>&1; then
      log "corrupt git tree at $SRC_DIR — recloning"
      rm -rf "$SRC_DIR"
    fi
  fi
  if [[ -d "$SRC_DIR/.git" ]]; then
    log "updating $SRC_DIR"
    if ! git -C "$SRC_DIR" fetch origin "$REPO_BRANCH" 2>/dev/null || \
       ! git -C "$SRC_DIR" checkout "$REPO_BRANCH" 2>/dev/null || \
       ! git -C "$SRC_DIR" pull --ff-only origin "$REPO_BRANCH" 2>/dev/null; then
      log "git update failed — recloning $SRC_DIR"
      rm -rf "$SRC_DIR"
    fi
  fi
  if [[ ! -d "$SRC_DIR/.git" ]]; then
    [[ -e "$SRC_DIR" ]] && rm -rf "$SRC_DIR"
    log "cloning $REPO_URL → $SRC_DIR"
    git clone --branch "$REPO_BRANCH" "$REPO_URL" "$SRC_DIR"
  fi
  if [[ ! -s "$SRC_DIR/clients/findtwin.c" || ! -s "$SRC_DIR/server/wrapper.c" ]]; then
    die "source tree looks empty/corrupt at $SRC_DIR — delete it and re-run"
  fi
}

install_user_rc() {
  local rc_src="$SRC_DIR/hedgeyttyrc"
  local rc_user="${HOME}/.config/hedgeytty/hedgeyttyrc"
  local rc_dist="${HOME}/.config/hedgeytty/hedgeyttyrc.dist"
  local tmp

  mkdir -p "${HOME}/.config/hedgeytty"
  tmp=$(mktemp)
  sed -e "s|Exec \"hedgeytty-hitomi-bg\"|Exec \"$BIN_DIR/hedgeytty-hitomi-bg\"|" \
      -e "s|Exec \"hedgeytty-dock\"|Exec \"$BIN_DIR/hedgeytty-dock\"|" \
    "$rc_src" >"$tmp"
  install -m 0644 "$tmp" "$rc_dist"
  if [[ ! -f "$rc_user" ]]; then
    install -m 0644 "$tmp" "$rc_user"
    log "installed $rc_user"
  else
    log "keeping existing $rc_user (new default in hedgeyttyrc.dist)"
    # Ensure dock autostart exists on upgrades (do not touch other customizations).
    if ! grep -qE 'Exec[[:space:]]+".*hedgeytty-dock"' "$rc_user" 2>/dev/null; then
      printf '\n# HedgeyTTY dock (added by installer)\nExec "%s/hedgeytty-dock"\n' \
        "$BIN_DIR" >>"$rc_user"
      log "appended hedgeytty-dock Exec to $rc_user"
    fi
  fi
  rm -f "$tmp"
  printf '%s\n' "$PROFILE" >"${HOME}/.config/hedgeytty/profile"
}

configure_args() {
  # Prints one argument per line (bash 3.2–safe; no mapfile).
  echo --prefix="$PREFIX"
  echo --enable-socket
  echo --enable-hw-tty
  if [[ "${HEDGEYTTY_WITH_X11:-0}" == 1 ]]; then
    echo --enable-hw-x11
    echo --enable-hw-xft
  else
    echo --disable-hw-x11
    echo --disable-hw-xft
  fi
  if [[ "$PROFILE" == pty ]]; then
    echo --enable-hw-tty-termcap
    echo --disable-hw-tty-linux
    echo --disable-hw-tty-lrawkbd
  else
    echo --enable-hw-tty-linux
  fi
}

build_and_install() {
  need_cmd make
  cd "$SRC_DIR"
  if [[ ! -x configure ]]; then
    log "bootstrapping autotools"
    if [[ -x ./autogen.sh ]]; then
      ./autogen.sh
    else
      autoreconf -fi
    fi
  fi
  # Homebrew/Termux: help configure find headers/libs
  if [[ "$(uname -s)" == Darwin ]]; then
    local bp
    bp=$(brew --prefix 2>/dev/null || true)
    if [[ -n "$bp" ]]; then
      export CPPFLAGS="${CPPFLAGS:-} -I${bp}/include"
      export LDFLAGS="${LDFLAGS:-} -L${bp}/lib"
      export PKG_CONFIG_PATH="${bp}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    fi
  fi
  if [[ -n "$TERMUX_USR" ]]; then
    export CPPFLAGS="${CPPFLAGS:-} -I${TERMUX_USR}/include"
    export LDFLAGS="${LDFLAGS:-} -L${TERMUX_USR}/lib"
    export PKG_CONFIG_PATH="${TERMUX_USR}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
  fi

  local -a carg=()
  while IFS= read -r line; do
    [[ -n "$line" ]] && carg+=("$line")
  done < <(configure_args)
  log "configure (${PROFILE}): ${carg[*]}"
  ./configure "${carg[@]}"
  log "make -j$JOBS"
  # nice may be missing or restricted on some hosts
  if command -v nice >/dev/null 2>&1; then
    nice -n 10 make -j"$JOBS" || die "make failed"
    nice -n 10 make install || die "make install failed"
  else
    make -j"$JOBS" || die "make failed"
    make install || die "make install failed"
  fi

  rm -f "$BIN_DIR/hedgeytty-fork" "$BIN_DIR/hedgeytty-fork.bin" \
        "$BIN_DIR/hedgeytty.fork-broken"
  if [[ ! -x "$BIN_DIR/hedgeytty" ]]; then
    die "make install did not produce $BIN_DIR/hedgeytty"
  fi
  if head -c 4 "$BIN_DIR/hedgeytty" | grep -q '#!'; then
    die "$BIN_DIR/hedgeytty is a script; expected the forked ELF client"
  fi

  mkdir -p "${HOME}/.config/hedgeytty" "${HOME}/.local/share/hedgeytty" \
           "${HOME}/.local/state/hedgeytty"
  install_user_rc
  # Always refresh htenvrc for pty TERM hints (small file).
  install -m 0644 "$SRC_DIR/htenvrc.sh" "${HOME}/.config/hedgeytty/htenvrc.sh"
  install -m 0644 "$SRC_DIR/assets/hitomi-icon.png" \
    "${HOME}/.local/share/hedgeytty/hitomi-icon.png"
  if [[ "$PROFILE" == console && -f "$SRC_DIR/hedgeytty/scripts/setup-gpm.sh" ]]; then
    install -m 0755 "$SRC_DIR/hedgeytty/scripts/setup-gpm.sh" "$BIN_DIR/hedgeytty-setup-gpm"
  fi
}

setup_mouse() {
  if [[ "$PROFILE" != console ]]; then
    log "pty profile — skip gpm (xterm mouse sequences)"
    return
  fi
  if [[ "${HEDGEYTTY_SKIP_GPM:-}" == 1 ]]; then
    log "skipping gpm (HEDGEYTTY_SKIP_GPM=1)"
    return
  fi
  if [[ ! -x "$BIN_DIR/hedgeytty-setup-gpm" ]]; then
    return
  fi
  if ! command -v systemctl >/dev/null 2>&1; then
    log "systemctl not found — skip gpm setup"
    return
  fi
  if [[ "$(id -u)" -eq 0 ]]; then
    "$BIN_DIR/hedgeytty-setup-gpm" || log "gpm setup failed — see hedgeytty/docs/mouse.md"
  elif command -v sudo >/dev/null; then
    log "configuring gpm via sudo"
    sudo "$BIN_DIR/hedgeytty-setup-gpm" || log "gpm setup failed — see hedgeytty/docs/mouse.md"
  fi
}

main() {
  detect_profile
  log "HedgeyTTY from-source installer (fork of Twin)"
  log "profile: $PROFILE  repo: $REPO_URL @$REPO_BRANCH → prefix $PREFIX"
  install_build_deps
  fetch_tree
  build_and_install
  setup_mouse
  if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    log "add to PATH: export PATH=\"$BIN_DIR:\$PATH\""
  fi
  if [[ "$PROFILE" == pty ]]; then
    cat <<MSG

Installed (pty profile — Termux / macOS Terminal).
  run:       $BIN_DIR/hedgeytty
  config:    ~/.config/hedgeytty/hedgeyttyrc
  profile:   ~/.config/hedgeytty/profile ($PROFILE)
  display:   HTDISPLAY  sockets: \$TMPDIR/.HedgeyTTY:*

Runs in the current terminal (xterm/termcap). Alt-Up opens a shell.
Under tmux/screen, mouse may need a real terminal or -hw=tty,mouse=xterm.
Override profile: HEDGEYTTY_PROFILE=console|pty

MSG
  else
    cat <<MSG

Installed (console profile — Linux VT + gpm).
  run:       $BIN_DIR/hedgeytty          # fork client → hedgeytty_server
  config:    ~/.config/hedgeytty/hedgeyttyrc
  profile:   ~/.config/hedgeytty/profile ($PROFILE)
  display:   HTDISPLAY  sockets: \$TMPDIR/.HedgeyTTY:* (default /tmp)
  server log:~/.local/state/hedgeytty/server.log

First boot is windowless (menubar + hedgehog). Alt-Up opens a terminal.
Do not start hedgeytty from inside Twin — Quit Twin first, then run on the bare console.
Headless --nohw leftovers are reclaimed on the next start (see README).
Override profile: HEDGEYTTY_PROFILE=console|pty

MSG
  fi
}

main "$@"
