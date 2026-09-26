#!/usr/bin/env bash
# Configure gpm so Twin/HedgeyTTY can use a USB/PS2 mouse on the Linux console.
# Twin's tty driver reads mouse events through gpm.
set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
  echo "Run as root (sudo). Example: sudo $0" >&2
  exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
  echo "hedgeytty-setup-gpm: systemctl not found — cannot enable gpm on this host." >&2
  echo "Install/configure gpm with your init system, or set HEDGEYTTY_SKIP_GPM=1." >&2
  exit 0
fi

export DEBIAN_FRONTEND=noninteractive
if command -v apt-get >/dev/null 2>&1; then
  apt-get install -y gpm
fi

CONF=/etc/gpm.conf
if [[ -f "$CONF" ]]; then
  # Ubuntu/Debian gpm.conf is sourced by the init script.
  sed -i \
    -e 's|^device=.*|device=/dev/input/mice|' \
    -e 's|^type=.*|type=exps2|' \
    -e 's|^repeat_type=.*|repeat_type=none|' \
    "$CONF"
else
  cat >"$CONF" <<'EOF'
device=/dev/input/mice
responsiveness=
repeat_type=none
type=exps2
append=''
sample_rate=
EOF
fi

systemctl enable gpm
systemctl restart gpm
systemctl --no-pager --full status gpm || true
echo "gpm configured: device=/dev/input/mice type=exps2"
