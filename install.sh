#!/usr/bin/env bash
# HedgeyTTY installer — canonical entrypoint for:
#   curl -fsSL https://agent1c.ai/tty.sh | sh
#   curl -fsSL https://raw.githubusercontent.com/agent1c-ai/hedgeytty/main/install.sh | sh
#
# agent1c.ai/tty.sh is a thin redirect to THIS file so it never drifts.
set -euo pipefail

REPO_OWNER="${HEDGEYTTY_REPO_OWNER:-agent1c-ai}"
REPO_NAME="${HEDGEYTTY_REPO_NAME:-hedgeytty}"
REPO_BRANCH="${HEDGEYTTY_BRANCH:-main}"
REPO_URL="${HEDGEYTTY_REPO_URL:-https://github.com/${REPO_OWNER}/${REPO_NAME}}"
RAW_BASE="${HEDGEYTTY_RAW_BASE:-https://raw.githubusercontent.com/${REPO_OWNER}/${REPO_NAME}/${REPO_BRANCH}}"

PREFIX="${HEDGEYTTY_PREFIX:-${HOME}/.local}"
BIN_DIR="${PREFIX}/bin"
SHARE_DIR="${PREFIX}/share/hedgeytty"
LIBEXEC_DIR="${PREFIX}/libexec/hedgeytty"
CONFIG_DIR="${HOME}/.config/twin"
SRC_DIR="${HEDGEYTTY_SRC:-${HOME}/src/hedgeytty}"

log() { printf 'hedgeytty: %s\n' "$*"; }
die() { printf 'hedgeytty: ERROR: %s\n' "$*" >&2; exit 1; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

have_pkg() {
  command -v "$1" >/dev/null 2>&1
}

install_pkgs() {
  local missing=0
  have_pkg twin || missing=1
  have_pkg gpm || missing=1
  have_pkg magick || have_pkg convert || missing=1
  have_pkg curl || missing=1
  have_pkg cc || have_pkg gcc || missing=1
  [[ -f /usr/include/Tw/Tw.h || -f /usr/local/include/Tw/Tw.h ]] || missing=1

  if [[ "$missing" -eq 0 ]]; then
    log "runtime dependencies already present"
    return
  fi

  if [[ "$(id -u)" -eq 0 ]]; then
    SUDO=""
  elif command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
  else
    die "need root or sudo to install packages (twin, gpm, imagemagick, build tools)"
  fi

  if command -v apt-get >/dev/null 2>&1; then
    log "installing packages with apt"
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update -qq
    $SUDO apt-get install -y \
      twin gpm imagemagick curl ca-certificates \
      build-essential pkg-config \
      libx11-dev libxft-dev zlib1g-dev libgpm-dev libncurses-dev
  else
    die "apt-get not found — install twin, gpm, ImageMagick, Tw headers, and a C compiler"
  fi
}

fetch_tree() {
  # Prefer an existing checkout (developer machine); else clone/update.
  if [[ -n "${HEDGEYTTY_LOCAL:-}" && -d "${HEDGEYTTY_LOCAL}/hedgeytty" ]]; then
    SRC_DIR=$HEDGEYTTY_LOCAL
    log "using local tree: $SRC_DIR"
    return
  fi

  if [[ -d "$SRC_DIR/.git" ]]; then
    log "updating $SRC_DIR"
    git -C "$SRC_DIR" fetch --depth 1 origin "$REPO_BRANCH" 2>/dev/null || true
    git -C "$SRC_DIR" checkout "$REPO_BRANCH" 2>/dev/null || true
    git -C "$SRC_DIR" pull --ff-only origin "$REPO_BRANCH" 2>/dev/null || true
  else
    need_cmd git
    mkdir -p "$(dirname "$SRC_DIR")"
    if [[ -d "$SRC_DIR" ]]; then
      die "$SRC_DIR exists but is not a git repo"
    fi
    log "cloning $REPO_URL → $SRC_DIR"
    git clone --depth 1 --branch "$REPO_BRANCH" "$REPO_URL" "$SRC_DIR"
  fi
}

install_files() {
  local root="$SRC_DIR/hedgeytty"
  [[ -d "$root" ]] || die "missing $root (is this the HedgeyTTY repo?)"

  mkdir -p "$BIN_DIR" "$SHARE_DIR" "$LIBEXEC_DIR" "$CONFIG_DIR" \
    "${HOME}/.local/share/hedgeytty"

  install -m 0755 "$root/bin/hedgeytty" "$BIN_DIR/hedgeytty"
  install -m 0755 "$root/scripts/setup-gpm.sh" "$BIN_DIR/hedgeytty-setup-gpm"
  install -m 0644 "$root/config/twinrc" "$CONFIG_DIR/twinrc"
  install -m 0644 "$root/config/twenvrc.sh" "$CONFIG_DIR/twenvrc.sh"
  install -m 0644 "$root/assets/hitomi-icon.png" \
    "${HOME}/.local/share/hedgeytty/hitomi-icon.png"
  install -m 0644 "$root/assets/hitomi-icon.png" "$SHARE_DIR/hitomi-icon.png"
  cp -a "$root/docs" "$SHARE_DIR/"

  # Build hedgehog desktop painter (needs libtw from the twin package).
  if [[ -f /usr/include/Tw/Tw.h ]] || [[ -f /usr/local/include/Tw/Tw.h ]]; then
    log "building hedgeytty-hitomi-bg"
    cc -O2 -o "$BIN_DIR/hedgeytty-hitomi-bg" \
      "$root/bin/hedgeytty-hitomi-bg.c" -ltw \
      || die "failed to build hedgeytty-hitomi-bg (is libtw installed?)"
  else
    die "Tw headers not found; install the twin development files / twin package"
  fi
}

setup_mouse() {
  if [[ "${HEDGEYTTY_SKIP_GPM:-}" == 1 ]]; then
    log "skipping gpm setup (HEDGEYTTY_SKIP_GPM=1)"
    return
  fi
  if [[ "$(id -u)" -eq 0 ]]; then
    "$BIN_DIR/hedgeytty-setup-gpm"
  elif command -v sudo >/dev/null 2>&1; then
    log "configuring gpm (console mouse) via sudo"
    sudo "$BIN_DIR/hedgeytty-setup-gpm" || log "gpm setup failed — see $SHARE_DIR/docs/mouse.md"
  else
    log "no sudo — configure gpm later: see $SHARE_DIR/docs/mouse.md"
  fi
}

path_hint() {
  case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *)
      log "add to your shell profile: export PATH=\"$BIN_DIR:\$PATH\""
      ;;
  esac
}

main() {
  log "HedgeyTTY installer (fork of Twin — https://github.com/cosmos72/twin)"
  log "repo: $REPO_URL @$REPO_BRANCH"

  install_pkgs
  fetch_tree
  install_files
  setup_mouse
  path_hint

  cat <<EOF

Installed.
  launcher:  $BIN_DIR/hedgeytty
  config:    $CONFIG_DIR/twinrc
  hedgehog:  $BIN_DIR/hedgeytty-hitomi-bg
  mouse doc: $SHARE_DIR/docs/mouse.md

Start on a text console:
  hedgeytty

Or:
  curl -fsSL https://agent1c.ai/tty.sh | sh

EOF
}

main "$@"
