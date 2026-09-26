# HedgeyTTY

**HedgeyTTY is a fork of [Twin](https://github.com/cosmos72/twin)** — the
Textmode WINdow environment by Massimiliano Ghilardi — packaged with the
defaults we use for a mouse-friendly Linux console desktop, including the
Hitomi hedgehog wallpaper.

Upstream Twin: https://github.com/cosmos72/twin  
This fork: https://github.com/agent1c-ai/hedgeytty

Twin’s own documentation lives in [`README.twin.md`](README.twin.md) (the
original upstream README).

## Quick install

```bash
curl -fsSL https://agent1c.ai/tty.sh | sh
```

`https://agent1c.ai/tty.sh` is a **thin redirect** into this repository’s
[`install.sh`](install.sh). Edit install behaviour here — not on the website —
so the curl entrypoint never drifts.

Equivalent:

```bash
curl -fsSL https://raw.githubusercontent.com/agent1c-ai/hedgeytty/main/install.sh | sh
```

Then on a text console:

```bash
hedgeytty
```

## What this fork adds

| Piece | Why |
|---|---|
| **gpm console mouse** (`exps2` on `/dev/input/mice`) | Twin’s tty driver needs gpm for a usable mouse on the Linux VT. Documented in [`hedgeytty/docs/mouse.md`](hedgeytty/docs/mouse.md); installer runs `hedgeytty-setup-gpm`. |
| **Socket + term modules on by default** | External clients (`twterm`, agents, etc.) can open windows without hunting the Modules menu. |
| **Hitomi hedgehog desktop** | `hedgeytty-hitomi-bg` paints truecolor UTF-8 half-blocks through libtw (Twin 1.0’s twinrc/ANSI colors truncate Magenta→Blue and break `twsetroot` ANSI). |
| **TurboVision-style menus** | Always-visible menubar, left-click to open (from upstream sample, kept as default). |
| **`hedgeytty` launcher** | Thin wrapper around `twin` with PATH + mouse hints. |

We intentionally keep Twin’s server/protocol intact. HedgeyTTY is Twin plus
opinionated packaging, config, and desktop chrome.

## Layout

```
install.sh                 # curl|sh canonical installer
hedgeytty/
  bin/hedgeytty            # launcher
  bin/hedgeytty-hitomi-bg.c
  config/twinrc            # installed to ~/.config/twin/twinrc
  config/twenvrc.sh
  assets/hitomi-icon.png
  scripts/setup-gpm.sh
  docs/mouse.md
…                          # full Twin v1.0.0 source tree (upstream)
```

## Requirements

- Linux with a virtual console (tested on Ubuntu)
- Packages the installer pulls on Debian/Ubuntu: `twin`, `gpm`, `imagemagick`,
  build tools, `libgpm-dev` (and Twin’s headers via the `twin` package)

## Developer install (local tree)

```bash
HEDGEYTTY_LOCAL=/path/to/hedgeytty ./install.sh
```

## License

Twin is GPL-2.0-or-later; see [`COPYING`](COPYING) / [`COPYING.LIB`](COPYING.LIB).
HedgeyTTY packaging scripts and assets in `hedgeytty/` are offered under the
same terms unless otherwise noted. The Hitomi icon is fetched/bundled from
[hitomi.love](https://hitomi.love) for desktop branding — respect upstream
branding rights if you redistribute.
