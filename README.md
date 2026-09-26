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
builds from source into `~/.local` (build deps only — **not** the `twin` binary).

Equivalent:

```bash
curl -fsSL https://raw.githubusercontent.com/agent1c-ai/hedgeytty/main/install.sh | bash
```

Then on a **bare** Linux text console (not inside Twin / not on a pty):

```bash
hedgeytty
```

`hedgeytty` is the forked client binary (it execs `hedgeytty_server`). It is **not** a
shell wrapper around apt `twin`. Starting it while Twin already owns the console
is refused (nested start + Quit breaks the outer TTY).

First boot is **windowless** (menubar + Hitomi hedgehog). **Alt-Up** opens a terminal.

## Naming (coexistence with apt Twin)

Stock Twin and HedgeyTTY can both be installed. They use different sockets and
env vars, so they do not fight for `:0`.

| Twin (apt / upstream) | HedgeyTTY |
|---|---|
| `twin` / `twin_server` | `hedgeytty` / `hedgeytty_server` |
| `twterm`, `twattach`, … | `htterm`, `htattach`, … |
| `TWDISPLAY`, `/tmp/.Twin:N` | `HTDISPLAY`, `/tmp/.HedgeyTTY:N` |
| `~/.TwinAuth` | `~/.HedgeyTTYAuth` |
| `~/.config/twin/twinrc` | `~/.config/hedgeytty/hedgeyttyrc` |
| `libtw.so` | `libht.so` (headers under `Ht/`) |

Wire protocol stays Twin-compatible in spirit; on-disk and env names differ.

## What this fork adds

| Piece | Why |
|---|---|
| **Namespaced runtime** | Own binaries, sockets, auth, config, and `libht` so apt `twin` can stay installed. |
| **gpm console mouse** | Documented in [`hedgeytty/docs/mouse.md`](hedgeytty/docs/mouse.md); installer runs `hedgeytty-setup-gpm`. |
| **Socket + term modules on** | Clients (`htterm`, agents) can open windows without hunting the Modules menu. |
| **Hitomi hedgehog desktop** | `hedgeytty-hitomi-bg` paints truecolor UTF-8 half-blocks via `libht` (Twin 1.0 twinrc/ANSI colors truncate Magenta→Blue). |
| **TurboVision-style menus** | Always-visible menubar, left-click to open. |
| **Windowless first boot** | No auto `ExecTty` — menubar + hedgehog only. |

## Layout

```
install.sh                 # curl|sh from-source installer → ~/.local
hedgeyttyrc                # package default RC (sysconfdir + user copy)
htenvrc.sh
assets/hitomi-icon.png
clients/hitomi-bg.c        # hedgeytty-hitomi-bg
server/wrapper.c           # `hedgeytty` client → exec hedgeytty_server
hedgeytty/
  scripts/setup-gpm.sh
  docs/mouse.md
…                          # Twin v1.0.0-derived source (forked)
```

## Requirements

- Linux virtual console (tested on Ubuntu)
- Build packages (installer pulls on Debian/Ubuntu): `build-essential`,
  `autoconf`, `automake`, `libtool`, `pkg-config`, `gpm`, `libgpm-dev`,
  `imagemagick`, `zlib1g-dev`, `libncurses-dev`, `libltdl-dev` — **not** the
  `twin` WM binary. X11 (`libx11-dev` / `libxft-dev`) is optional
  (`HEDGEYTTY_WITH_X11=1`).

Build time is a few minutes on a CPU laptop; prefix defaults to `~/.local`.

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
