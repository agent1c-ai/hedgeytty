# Console mouse with HedgeyTTY / Twin

Twin on the Linux virtual console does **not** talk to `/dev/input/mice`
directly. It uses **gpm** (General Purpose Mouse).

## What we use on DinkPad / Ubuntu

| Setting | Value |
|---|---|
| Package | `gpm` |
| Device | `/dev/input/mice` |
| Protocol | `exps2` (Explorer PS/2 — works for most USB mice via the kernel mousedev) |
| Service | `gpm.service` enabled + running |

Install / repair:

```bash
sudo "$(dirname "$0")/../scripts/setup-gpm.sh"
# or after HedgeyTTY install:
sudo hedgeytty-setup-gpm
```

## Verify

```bash
systemctl is-active gpm
# Move the mouse on a text console (not under X/Wayland).
# Inside HedgeyTTY you should see the Twin mouse pointer.
```

## Notes

- gpm and X/Wayland fighting over the same device can cause oddities; HedgeyTTY
  targets **multi-user / text console** sessions (`multi-user.target`).
- Menus: TurboVision-style (always-visible menubar, left-click) is the
  HedgeyTTY default in `hedgeyttyrc` / `~/.config/hedgeytty/hedgeyttyrc`.
- Without gpm, keyboard still works (`Pause` / `F12` open the menu).
- HedgeyTTY targets the text console; stock apt `twin` can coexist via
  different sockets (`/tmp/.HedgeyTTY:*` vs `/tmp/.Twin:*`).
