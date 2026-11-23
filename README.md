# ASUS Keyboard Backlight

Change the RGB keyboard backlight of ASUS TUF laptops on Linux. It sets the
color, effect and brightness you would otherwise set in Armoury Crate on Windows.

Tested on the **ASUS TUF Gaming A15 (FA507NU)** with Ubuntu.
Other TUF models with a single zone RGB keyboard should work too.

This is an unofficial project and is not affiliated with ASUS.

## How it works

The `asus-wmi` kernel driver already exposes the keyboard backlight in sysfs:

| File | Purpose |
|------|---------|
| `/sys/class/leds/asus::kbd_backlight/brightness` | brightness level from `0` to `3` |
| `/sys/class/leds/asus::kbd_backlight/kbd_rgb_mode` | `cmd mode red green blue speed` |

This project is a thin layer on top of that:

```
src/kbd.c, kbd.h   C library: open()/write() on the sysfs files
src/cli.c          asus-kbd-backlight: command line tool (C)
src/gui/main.cpp   asus-kbd-backlight-gui: graphical window (C++ / Qt6)
udev/              rule that lets normal users write those files
```

Colors are saved to the keyboard firmware, so they persist across reboots
without a background service.

## Requirements

```sh
sudo apt install build-essential cmake qt6-base-dev
```

Your kernel must have `/sys/class/leds/asus::kbd_backlight/kbd_rgb_mode`
(Linux 6.1 or newer).

## Install

```sh
./install.sh
```

This builds the project, installs `asus-kbd-backlight` and `asus-kbd-backlight-gui` to
`/usr/local/bin`, adds a desktop entry, and installs the udev rule.

To build without the GUI: `cmake -S . -B build -DBUILD_GUI=OFF`.

## Usage

Open **ASUS Keyboard Backlight** from your app menu, or use the CLI:

```sh
asus-kbd-backlight color ff0000                 # static red
asus-kbd-backlight color 00ff80 breathing slow  # breathing effect
asus-kbd-backlight color ffffff cycle fast      # cycle through colors
asus-kbd-backlight brightness 2                 # 0 (off) to 3
asus-kbd-backlight status
```

## Permissions

The kernel creates the sysfs files as root only. The udev rule in
`udev/99-asus-kbd-backlight.rules` makes `brightness` and `kbd_rgb_mode`
writable by all users, so the app does not need to run as root. These files
control only the keyboard light.

## Uninstall

```sh
sudo xargs rm < build/install_manifest.txt
```

## License

MIT. See [LICENSE](LICENSE).
