# HedgeyTTY

**HedgeyTTY is a fork of [Twin](https://github.com/cosmos72/twin)** (Textmode
WINdow environment by Massimiliano Ghilardi), packaged as its own runtime —
not a shell wrapper around Ubuntu’s `twin` package.

Upstream Twin baseline in this tree: **v1.0.0** (see [`README.twin.md`](README.twin.md)).  
This fork: https://github.com/agent1c-ai/hedgeytty

## Quick install

```bash
curl -fsSL https://agent1c.ai/tty.sh | sh
```

`https://agent1c.ai/tty.sh` is a **thin redirect** into this repository’s
[`install.sh`](install.sh) (it runs the installer under **bash**). The installer
auto-selects a profile and builds from source (build deps only — **not** the
`twin` binary).

| Profile | Platforms | Display |
|---|---|---|
| **console** | Linux VT (Ubuntu/Debian) | linux tty + gpm |
| **pty** | Termux, macOS Terminal/iTerm | xterm / termcap |

Override with `HEDGEYTTY_PROFILE=console|pty`.

Equivalent:

```bash
curl -fsSL https://raw.githubusercontent.com/agent1c-ai/hedgeytty/main/install.sh | bash
```

### Linux console

On a **bare** Linux text console (not inside Twin):

```bash
hedgeytty
```

### Termux

```bash
pkg install curl bash git   # if needed
curl -fsSL https://agent1c.ai/tty.sh | bash
hedgeytty                   # takes over the Termux terminal
```

Installs into Termux `$PREFIX` by default (already on `PATH`).

### macOS (Terminal / iTerm)

```bash
xcode-select --install      # once
brew install curl git       # if needed; installer pulls the rest via brew
curl -fsSL https://agent1c.ai/tty.sh | bash
hedgeytty
```

On macOS, orphan reclaim is best-effort (no `/proc`): a live socket refuses a
second start; a stale socket file is removed. Prefer Quit from the menubar.

`hedgeytty` is the forked client binary (it execs `hedgeytty_server`). It is **not** a
shell wrapper around apt `twin`. Starting it while Twin already owns the console
is refused (nested start + Quit breaks the outer TTY).

First boot is **windowless** (menubar + Hitomi hedgehog). **Alt-Up** opens a terminal.
Under `tmux`/`screen`, mouse may need a real terminal or `-hw=tty,mouse=xterm`.

## Naming (coexistence with apt Twin)

Stock Twin and HedgeyTTY can both be installed. They use different sockets and
env vars, so they do not fight for `:0`.

| Twin (apt / upstream) | HedgeyTTY |
|---|---|
| `twin` / `twin_server` | `hedgeytty` / `hedgeytty_server` |
| `twterm`, `twattach`, … | `htterm`, `htattach`, … |
| `TWDISPLAY`, `/tmp/.Twin:N` | `HTDISPLAY`, `$TMPDIR/.HedgeyTTY:N` |
| `~/.TwinAuth` | `~/.HedgeyTTYAuth` |
| `~/.config/twin/twinrc` | `~/.config/hedgeytty/hedgeyttyrc` |
| `libtw.so` | `libht.so` (headers under `Ht/`) |

Wire protocol stays Twin-compatible in spirit; on-disk and env names differ.

## What this fork adds

| Piece | Why |
|---|---|
| **Namespaced runtime** | Own binaries, sockets, auth, config, and `libht` so apt `twin` can stay installed. |
| **gpm console mouse** | Documented in [`hedgeytty/docs/mouse.md`](hedgeytty/docs/mouse.md); installer runs `hedgeytty-setup-gpm` on **console** profile. |
| **Socket + term modules on** | Clients (`htterm`, agents) can open windows without hunting the Modules menu. |
| **Hitomi hedgehog desktop** | `hedgeytty-hitomi-bg` paints truecolor UTF-8 half-blocks via `libht` (Twin 1.0 twinrc/ANSI colors truncate Magenta→Blue). |
| **App dock** | `hedgeytty-dock` — bind shell commands to slots; state in `~/.config/hedgeytty/dock.slots`. |
| **TurboVision-style menus** | Always-visible menubar, left-click to open. |
| **First boot** | No auto `ExecTty` — menubar + hedgehog wallpaper + dock. |
| **pty profile** | Termux / macOS run in the current terminal via Twin’s xterm/termcap stack. |

## Layout

```
install.sh                 # curl|sh from-source installer → prefix
hedgeyttyrc                # package default RC (sysconfdir + user copy)
htenvrc.sh
assets/hitomi-icon.png
clients/hitomi-bg.c        # hedgeytty-hitomi-bg
clients/dock/              # hedgeytty-dock (modular: state / render / input / app)
server/wrapper.c           # `hedgeytty` client → exec hedgeytty_server
hedgeytty/
  scripts/setup-gpm.sh
  docs/mouse.md
…                          # Twin v1.0.0-derived source (forked)
```

## Requirements

| Mode | Platforms | Needs |
|---|---|---|
| **console** | Linux VT | AF_UNIX, linux tty driver, **gpm** |
| **pty** | Termux, macOS Terminal/iTerm | AF_UNIX, ncurses/`tgetent`, xterm-capable `TERM` |
| **Unsupported** | Windows / PowerShell / MSYS | no product path |

WSL is usually a pty — use `HEDGEYTTY_PROFILE=pty` if you try it; it is not the
Linux console UX.

**console** packages (Debian/Ubuntu installer): `build-essential`, `autoconf`,
`automake`, `libtool`, `pkg-config`, `gpm`, `libgpm-dev`, `imagemagick`,
`zlib1g-dev`, `libncurses-dev`, `libltdl-dev`.

**pty** packages: Termux `pkg` or Homebrew `autoconf automake libtool
pkg-config ncurses imagemagick` (see install sections above).

Skip mouse setup with `HEDGEYTTY_SKIP_GPM=1` (console only; ignored on pty).

User config: reinstall keeps an existing `~/.config/hedgeytty/hedgeyttyrc` and
writes the packaged default to `hedgeyttyrc.dist` only. The active profile is
stored in `~/.config/hedgeytty/profile`.

**`--nohw`:** for attach/debug. On Linux/Termux, headless servers (no controlling
tty) are **reclaimed** on the next `hedgeytty` start. On macOS, a live socket
refuses a second start until Quit (or remove the stale `$TMPDIR/.HedgeyTTY:*`
after killing the process). `HEDGEYTTY_ALLOW_NESTED=1` bypasses refuse (unsafe
on a shared console).

Build time is a few minutes on a CPU laptop; prefix defaults to `~/.local`
(Termux: `$PREFIX`).

## Developer install (local tree)

```bash
HEDGEYTTY_LOCAL=/path/to/hedgeytty ./install.sh
```

Or manually:

```bash
./configure --prefix="$HOME/.local" --enable-socket --enable-term --disable-ttlib
make -j2 && make install
```

On memory-tight hosts, prefer `lab-run -- make -j2` when available.

## License

Twin is GPL-2.0-or-later; see [`COPYING`](COPYING) / [`COPYING.LIB`](COPYING.LIB).
HedgeyTTY packaging scripts and assets are offered under the same terms unless
otherwise noted. The Hitomi icon is fetched/bundled from
[hitomi.love](https://hitomi.love) for desktop branding — respect upstream
branding rights if you redistribute.
