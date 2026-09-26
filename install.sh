#!/usr/bin/env bash
# HedgeyTTY from-source installer — canonical entrypoint for:
#   curl -fsSL https://agent1c.ai/tty.sh | sh   # tty.sh invokes bash
#   curl -fsSL .../install.sh | bash
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

PREFIX="${HEDGEYTTY_PREFIX:-${HOME}/.local}"
BIN_DIR="${PREFIX}/bin"
SRC_DIR="${HEDGEYTTY_SRC:-${HOME}/src/hedgeytty}"
JOBS="${HEDGEYTTY_JOBS:-$(nproc 2>/dev/null || echo 2)}"

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

sudo_cmd() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
    return
  fi
  command -v sudo >/dev/null 2>&1 || die "need root or sudo for: $*"
  # Prefer Cursor/askpass when no tty (agents); otherwise normal sudo.
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

install_build_deps() {
  local missing=0
  command -v gcc >/dev/null || missing=1
  command -v g++ >/dev/null || missing=1
  command -v make >/dev/null || missing=1
  command -v pkg-config >/dev/null || missing=1
  command -v gpm >/dev/null || missing=1
  command -v magick >/dev/null || command -v convert >/dev/null || missing=1
  command -v curl >/dev/null || missing=1
  [[ -f /usr/include/zlib.h ]] || missing=1
  # gpm daemon alone is not enough — without headers/lib, mouse compiles out
  [[ -f /usr/include/gpm.h ]] || missing=1
  [[ -e /usr/lib/x86_64-linux-gnu/libgpm.so || -e /usr/lib/libgpm.so ]] || missing=1
  # X11/Xft are optional (console Twin needs only tty+gpm). Soft-check for nicer builds.
  local want_x=0
  if [[ "${HEDGEYTTY_WITH_X11:-0}" == 1 ]]; then
    want_x=1
    [[ -f /usr/include/X11/Xlib.h ]] || missing=1
  fi

  if [[ "$missing" -eq 0 ]]; then
    log "build dependencies already present"
    return
  fi
  command -v apt-get >/dev/null || die "apt-get not found; install build deps manually"
  log "installing build dependencies (not the twin binary)"
  export DEBIAN_FRONTEND=noninteractive
  local pkgs=(
    build-essential autoconf automake libtool pkg-config
    gpm libgpm-dev
    imagemagick curl ca-certificates
    zlib1g-dev libncurses-dev
    libltdl-dev
  )
  if [[ "$want_x" -eq 1 ]]; then
    pkgs+=(libx11-dev libxft-dev)
  fi
  sudo_cmd apt-get update -qq
  sudo_cmd apt-get install -y "${pkgs[@]}"
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
    log "updating $SRC_DIR"
    git -C "$SRC_DIR" fetch origin "$REPO_BRANCH" 2>/dev/null || true
    git -C "$SRC_DIR" checkout "$REPO_BRANCH" 2>/dev/null || true
    git -C "$SRC_DIR" pull --ff-only origin "$REPO_BRANCH" 2>/dev/null || true
  else
    [[ -e "$SRC_DIR" ]] && die "$SRC_DIR exists but is not a git repo"
    log "cloning $REPO_URL → $SRC_DIR"
    git clone --branch "$REPO_BRANCH" "$REPO_URL" "$SRC_DIR"
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
  log "configure --prefix=$PREFIX"
  ./configure --prefix="$PREFIX" \
    --enable-socket \
    --enable-hw-tty \
    --enable-hw-tty-linux \
    --disable-hw-x11 \
    --disable-hw-xft
  log "make -j$JOBS"
  # Prefer plain nice make: lab-run's 'make -j' pgrep can false-positive on the
  # parent shell/agent cmdline. Cap JOBS above already; this is user-attended.
  nice -n 10 make -j"$JOBS" || die "make failed"
  nice -n 10 make install || die "make install failed"

  # The real client is the ELF built from server/wrapper.c (execs hedgeytty_server).
  # Remove any leftover apt-twin shell wrappers / recovery names from older installs.
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
  if [[ -f "${HOME}/.config/hedgeytty/hedgeyttyrc" ]]; then
    cp -a "${HOME}/.config/hedgeytty/hedgeyttyrc" \
      "${HOME}/.config/hedgeytty/hedgeyttyrc.bak"
  fi
  install -m 0644 "$SRC_DIR/hedgeyttyrc" "${HOME}/.config/hedgeytty/hedgeyttyrc"
  install -m 0644 "$SRC_DIR/hedgeyttyrc" "${HOME}/.config/hedgeytty/hedgeyttyrc.dist"
  [[ -f "${HOME}/.config/hedgeytty/htenvrc.sh" ]] || \
    install -m 0644 "$SRC_DIR/htenvrc.sh" "${HOME}/.config/hedgeytty/htenvrc.sh"
  install -m 0644 "$SRC_DIR/assets/hitomi-icon.png" \
    "${HOME}/.local/share/hedgeytty/hitomi-icon.png"
  if [[ -f "$SRC_DIR/hedgeytty/scripts/setup-gpm.sh" ]]; then
    install -m 0755 "$SRC_DIR/hedgeytty/scripts/setup-gpm.sh" "$BIN_DIR/hedgeytty-setup-gpm"
  fi
}

setup_mouse() {
  if [[ "${HEDGEYTTY_SKIP_GPM:-}" == 1 ]]; then
    log "skipping gpm"
    return
  fi
  if [[ -x "$BIN_DIR/hedgeytty-setup-gpm" ]]; then
    if [[ "$(id -u)" -eq 0 ]]; then
      "$BIN_DIR/hedgeytty-setup-gpm"
    elif command -v sudo >/dev/null; then
      log "configuring gpm via sudo"
      sudo "$BIN_DIR/hedgeytty-setup-gpm" || log "gpm setup failed — see docs"
    fi
  fi
}

main() {
  log "HedgeyTTY from-source installer (fork of Twin)"
  log "repo: $REPO_URL @$REPO_BRANCH → prefix $PREFIX"
  install_build_deps
  fetch_tree
  build_and_install
  setup_mouse
  if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    log "add to PATH: export PATH=\"$BIN_DIR:\$PATH\""
  fi
  cat <<MSG

Installed.
  run:       $BIN_DIR/hedgeytty          # fork client → hedgeytty_server
  config:    ~/.config/hedgeytty/hedgeyttyrc
  display:   HTDISPLAY  sockets: /tmp/.HedgeyTTY:*
  server log:~/.local/state/hedgeytty/server.log

First boot is windowless (menubar + hedgehog). Alt-Up opens a terminal.
Do not start hedgeytty from inside Twin — Quit Twin first, then run on the bare console.

MSG
}

main "$@"
